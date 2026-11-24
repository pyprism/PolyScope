#include <QJSEngine>
#include "adb/CommandRunner.h"
#include "adb/AdbParsers.h"

#include "adb/AdbClient.h"
#include "adb/CommandCatalog.h"
#include "core/AppContext.h"
#include "core/AppSettings.h"
#include "scrcpy/ScrcpyArgs.h"

#include <QProcess>
#include <QTimer>

namespace polyscope::adb {

namespace {

QVariantMap paramToMap(const CommandParam& p) {
    return {{QStringLiteral("key"), p.key},
            {QStringLiteral("label"), p.label},
            {QStringLiteral("type"), p.type},
            {QStringLiteral("default"), p.type == QStringLiteral("bool") ? QVariant(!p.defaultValue.isEmpty())
                                                                          : QVariant(p.defaultValue)},
            {QStringLiteral("choices"), p.choices},
            {QStringLiteral("required"), p.required},
            {QStringLiteral("placeholder"), p.placeholder}};
}

QString modeName(CommandMode m) {
    switch (m) {
    case CommandMode::Host:
        return QStringLiteral("host");
    case CommandMode::Device:
        return QStringLiteral("device");
    case CommandMode::Shell:
        return QStringLiteral("shell");
    }
    return {};
}

QVariantMap specToMap(const CommandSpec& spec) {
    QVariantList params;
    for (const CommandParam& p : spec.params) {
        params.append(paramToMap(p));
    }
    return {{QStringLiteral("id"), spec.id},
            {QStringLiteral("title"), spec.title},
            {QStringLiteral("group"), spec.group},
            {QStringLiteral("mode"), modeName(spec.mode)},
            {QStringLiteral("dangerous"), spec.dangerous},
            {QStringLiteral("needsDevice"), spec.needsDevice()},
            {QStringLiteral("params"), params}};
}

QString outputOf(const ProcessResult& r) {
    QString text = r.text();
    if (!r.err.trimmed().isEmpty()) {
        text += (text.isEmpty() || text.endsWith(QLatin1Char('\n')) ? QString() : QStringLiteral("\n")) + r.err;
    }
    if (!r.started) {
        return r.errorText();
    }
    if (r.timedOut) {
        text += QStringLiteral("\n[timed out]");
    }
    return text;
}

} // namespace

CommandRunner::CommandRunner(AdbClient* client, AppSettings* settings, QObject* parent)
    : QObject(parent), m_client(client), m_settings(settings) {}

CommandRunner* CommandRunner::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->commands();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

QVariantList CommandRunner::groups() const {
    QVariantList result;
    for (const QString& group : commandGroups()) {
        QVariantList commands;
        for (const CommandSpec& spec : commandCatalog()) {
            if (spec.group == group) {
                commands.append(specToMap(spec));
            }
        }
        result.append(QVariantMap{{QStringLiteral("name"), group}, {QStringLiteral("commands"), commands}});
    }
    return result;
}

QVariantMap CommandRunner::command(const QString& id) const {
    const CommandSpec* spec = findCommand(id);
    return spec ? specToMap(*spec) : QVariantMap();
}

QVariantList CommandRunner::search(const QString& text) const {
    const QStringList terms = text.toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    QVariantList result;
    for (const CommandSpec& spec : commandCatalog()) {
        const QString hay = (spec.id + QLatin1Char(' ') + spec.title + QLatin1Char(' ') + spec.group).toLower();
        const bool all = std::all_of(terms.cbegin(), terms.cend(),
                                     [&hay](const QString& t) { return hay.contains(t); });
        if (all) {
            result.append(specToMap(spec));
        }
    }
    return result;
}

QString CommandRunner::preview(const QString& id, const QString& serial, const QVariantMap& values) const {
    const CommandSpec* spec = findCommand(id);
    if (!spec) {
        return QStringLiteral("error: unknown command");
    }
    const ExpandResult r = expandCommand(*spec, values);
    if (!r.ok) {
        return QStringLiteral("error: ") + r.error;
    }
    QStringList parts{QStringLiteral("adb")};
    if (spec->needsDevice() && !serial.isEmpty()) {
        parts << QStringLiteral("-s") << serial;
    }
    parts << r.args;
    return scrcpy::commandLinePreview(parts.first(), parts.mid(1));
}

void CommandRunner::fail(int id, const QString& commandId, const QString& serial, const QString& message) {
    // Deliver asynchronously so callers can connect after run() returns.
    QTimer::singleShot(0, this, [this, id, commandId, serial, message]() {
        emit finished(id, commandId, serial, false, message, -1);
    });
}

int CommandRunner::run(const QString& id, const QString& serial, const QVariantMap& values) {
    const int requestId = nextId();
    const CommandSpec* spec = findCommand(id);
    if (!spec) {
        fail(requestId, id, serial, tr("Unknown command: %1").arg(id));
        return requestId;
    }
    if (spec->needsDevice() && serial.isEmpty()) {
        fail(requestId, id, serial, tr("Select a device first."));
        return requestId;
    }
    const ExpandResult expanded = expandCommand(*spec, values);
    if (!expanded.ok) {
        fail(requestId, id, serial, expanded.error);
        return requestId;
    }
    m_client->run(this, spec->needsDevice() ? serial : QString(), expanded.args, spec->timeoutMs,
                  [this, requestId, id, serial](const ProcessResult& r) {
                      // connect/pair exit 0 on failure, so judge them by their message.
                      const bool ok = (id == QStringLiteral("conn.connect") || id == QStringLiteral("conn.pair"))
                                          ? r.ok() && !adb::messageIndicatesFailure(r.text() + r.err)
                                          : r.ok();
                      emit finished(requestId, id, serial, ok, outputOf(r), r.exitCode);
                  });
    return requestId;
}

int CommandRunner::runRaw(const QString& serial, const QString& commandLine) {
    const int requestId = nextId();
    QStringList args = QProcess::splitCommand(commandLine.trimmed());
    if (!args.isEmpty() && (args.first() == QStringLiteral("adb") || args.first().endsWith(QStringLiteral("/adb")))) {
        args.removeFirst();
    }
    if (args.isEmpty()) {
        fail(requestId, QStringLiteral("raw"), serial, tr("Empty command."));
        return requestId;
    }
    m_settings->addCommandHistory(commandLine);
    // An explicit -s in the command wins over the selected device.
    const bool explicitSerial = adb::hasExplicitSerial(args);
    m_client->run(this, explicitSerial ? QString() : serial, args, 120000,
                  [this, requestId, serial](const ProcessResult& r) {
                      emit finished(requestId, QStringLiteral("raw"), serial, r.ok(), outputOf(r), r.exitCode);
                  });
    return requestId;
}

QVariantList CommandRunner::runBatch(const QStringList& serials, const QString& id, const QVariantMap& values) {
    QVariantList ids;
    for (const QString& serial : serials) {
        ids.append(run(id, serial, values));
    }
    return ids;
}

} // namespace polyscope::adb
