#include <QJSEngine>
#include "core/ToolPaths.h"

#include "adb/ProcessRunner.h"
#include "core/AppContext.h"
#include "core/Logging.h"
#include "core/ToolSearch.h"

#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace polyscope {

namespace {

constexpr auto kAdbKey = "tools/adbPath";
constexpr auto kScrcpyKey = "tools/scrcpyPath";

QString toLocalPath(const QString& pathOrUrl) {
    const QString trimmed = pathOrUrl.trimmed();
    if (trimmed.startsWith(QStringLiteral("file:"))) {
        return QUrl(trimmed).toLocalFile();
    }
    return trimmed;
}

bool isExecutableFile(const QString& path) {
    const QFileInfo info(path);
    return info.isFile() && info.isExecutable();
}

QString firstLine(const QString& text) {
    return text.section(QLatin1Char('\n'), 0, 0).trimmed();
}

} // namespace

ToolPaths::ToolPaths(QObject* parent) : QObject(parent) {
    QSettings settings;
    m_adb.override = settings.value(QLatin1String(kAdbKey)).toString();
    m_scrcpy.override = settings.value(QLatin1String(kScrcpyKey)).toString();
}

ToolPaths* ToolPaths::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->tools();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

void ToolPaths::setAdbPath(const QString& pathOrUrl) {
    setOverride(Kind::Adb, pathOrUrl);
}

void ToolPaths::setScrcpyPath(const QString& pathOrUrl) {
    setOverride(Kind::Scrcpy, pathOrUrl);
}

void ToolPaths::setOverride(Kind kind, const QString& pathOrUrl) {
    QSettings settings;
    const QString key = QLatin1String(kind == Kind::Adb ? kAdbKey : kScrcpyKey);
    const QString path = toLocalPath(pathOrUrl);
    entry(kind).override = path;
    if (path.isEmpty()) {
        settings.remove(key);
    } else {
        settings.setValue(key, path);
    }
    check(kind);
}

void ToolPaths::refresh() {
    check(Kind::Adb);
    check(Kind::Scrcpy);
}

void ToolPaths::notify(Kind kind) {
    if (kind == Kind::Adb) {
        emit adbChanged();
    } else {
        emit scrcpyChanged();
    }
}

void ToolPaths::check(Kind kind) {
    Entry& e = entry(kind);
    const tools::Tool tool = kind == Kind::Adb ? tools::Tool::Adb : tools::Tool::Scrcpy;

    QString candidate;
    if (!e.override.isEmpty()) {
        candidate = e.override;
        if (!isExecutableFile(candidate)) {
            e.path = candidate;
            e.ready = false;
            e.version.clear();
            e.error = tr("Not an executable file: %1").arg(candidate);
            notify(kind);
            return;
        }
    } else {
        const QString onPath = QStandardPaths::findExecutable(
            tools::executableName(tool, tools::currentPlatform()));
        if (!onPath.isEmpty()) {
            candidate = onPath;
        } else {
            const auto candidates = tools::candidateExecutables(tool, tools::currentSearchEnv());
            for (const QString& path : candidates) {
                if (isExecutableFile(path)) {
                    candidate = path;
                    break;
                }
            }
        }
    }

    if (candidate.isEmpty()) {
        e.path.clear();
        e.version.clear();
        e.ready = false;
        e.error = tr("Not found. Install it or choose its location.");
        notify(kind);
        return;
    }

    struct Snapshot {
        QString path;
        QString version;
        bool ready;
    };
    const Snapshot before{e.path, e.version, e.ready};
    if (candidate != e.path) {
        // A different binary is unverified until its --version probe answers.
        e.ready = false;
        e.version.clear();
    }
    e.path = candidate;
    ++m_pending;
    emit checkingChanged();
    const QString expected = candidate;
    adb::ProcessRunner::run(
        this, candidate, {QStringLiteral("--version")}, 8000,
        [this, kind, expected, before](const adb::ProcessResult& result) {
            Entry& target = entry(kind);
            --m_pending;
            emit checkingChanged();
            if (target.path != expected) {
                return; // superseded by a newer check
            }
            const QString output = result.text().trimmed();
            const QString first = firstLine(output);
            const bool plausible = kind == Kind::Adb
                                       ? output.contains(QStringLiteral("Android Debug Bridge"))
                                       : first.startsWith(QStringLiteral("scrcpy"));
            if (result.ok() && plausible) {
                target.ready = true;
                target.error.clear();
                target.version = first;
                qCInfo(lcTools) << "found" << target.path << target.version;
            } else {
                target.ready = false;
                target.version.clear();
                target.error = tr("This file does not look like %1.")
                                   .arg(kind == Kind::Adb ? QStringLiteral("adb")
                                                          : QStringLiteral("scrcpy"));
            }
            // Re-checks that change nothing must not restart device tracking.
            if (target.path != before.path || target.ready != before.ready || target.version != before.version) {
                notify(kind);
            }
        });
}

} // namespace polyscope
