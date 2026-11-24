#include <QJSEngine>
#include "scrcpy/ScrcpyManager.h"

#include "adb/ProcessRunner.h"
#include "core/AppContext.h"
#include "core/AppSettings.h"
#include "core/Logging.h"
#include "core/ToolPaths.h"
#include "scrcpy/ScrcpyArgs.h"

#include <QDateTime>
#include <QDir>
#include <QRegularExpression>
#include <QTimer>

#include <memory>

#include "core/WindowActivation.h"

namespace polyscope {

namespace {
constexpr int kMaxLogChars = 200000;

QString safeName(const QString& serial) {
    QString name = serial;
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
    return name;
}
} // namespace

// scrcpy only registers with the window server once its window is being created, so keep
// trying for a few seconds; then activate once more after the window is actually up.
void ScrcpyManager::raiseWindowWhenReady(QProcess* process) {
    auto* timer = new QTimer(process); // dies with the process
    auto attempts = std::make_shared<int>(0);
    auto activated = std::make_shared<bool>(false);
    timer->setInterval(500);
    connect(timer, &QTimer::timeout, process, [timer, process, attempts, activated]() {
        ++*attempts;
        const bool ok = bringProcessToFront(process->processId());
        if (ok && *activated) {
            timer->stop();
        } else if (ok) {
            *activated = true; // one more pass after the window has had time to appear
        }
        if (*attempts >= 24) {
            timer->stop();
        }
    });
    timer->start();
}

ScrcpyManager::ScrcpyManager(ToolPaths* tools, AppSettings* settings, QObject* parent)
    : QObject(parent), m_tools(tools), m_settings(settings) {}

ScrcpyManager::~ScrcpyManager() = default;

ScrcpyManager* ScrcpyManager::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->scrcpy();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

QStringList ScrcpyManager::runningSerials() const {
    return m_sessions.keys();
}

bool ScrcpyManager::isRunning(const QString& serial) const {
    return m_sessions.contains(serial);
}

QString ScrcpyManager::log(const QString& serial) const {
    const auto it = m_sessions.constFind(serial);
    if (it != m_sessions.cend()) {
        return it->log;
    }
    return m_lastLogs.value(serial);
}

void ScrcpyManager::clearLog(const QString& serial) {
    m_lastLogs.remove(serial);
    auto it = m_sessions.find(serial);
    if (it != m_sessions.end()) {
        it->log.clear();
    }
}

QProcessEnvironment ScrcpyManager::environment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    // Make scrcpy use the same adb as the rest of the app (one adb server).
    if (m_tools->adbReady()) {
        env.insert(QStringLiteral("ADB"), m_tools->adbPath());
    }
    return env;
}

QVariantMap ScrcpyManager::withDefaults(const QString& serial, QVariantMap options) const {
    const QString title = options.value(QStringLiteral("windowTitle")).toString().trimmed();
    if (title.isEmpty()) {
        options[QStringLiteral("windowTitle")] = QStringLiteral("PolyScope - %1").arg(serial);
    }
    if (options.value(QStringLiteral("record")).toBool() &&
        options.value(QStringLiteral("recordPath")).toString().trimmed().isEmpty()) {
        const QString dir = m_settings->recordDir();
        QDir().mkpath(dir);
        const QString format = options.value(QStringLiteral("recordFormat")).toString();
        const QString ext = format.isEmpty() ? QStringLiteral("mp4") : format;
        options[QStringLiteral("recordPath")] =
            QDir(dir).filePath(QStringLiteral("PolyScope_%1_%2.%3")
                                   .arg(safeName(serial),
                                        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")),
                                        ext));
    }
    return options;
}

QString ScrcpyManager::commandLine(const QString& serial, const QVariantMap& options) const {
    const QStringList args = scrcpy::buildArguments(serial, withDefaults(serial, options));
    return scrcpy::commandLinePreview(m_tools->scrcpyPath().isEmpty() ? QStringLiteral("scrcpy")
                                                                      : m_tools->scrcpyPath(),
                                      args);
}

QStringList ScrcpyManager::warnings(const QVariantMap& options) const {
    return scrcpy::validateOptions(options);
}

QVariantList ScrcpyManager::schema() const {
    return scrcpy::optionSchema();
}

QStringList ScrcpyManager::groups() const {
    return scrcpy::optionGroups();
}

QVariantMap ScrcpyManager::defaults() const {
    return scrcpy::defaultOptions();
}

QStringList ScrcpyManager::presetNames() const {
    return scrcpy::presetNames() + m_settings->userPresetNames();
}

QVariantMap ScrcpyManager::preset(const QString& name) const {
    if (scrcpy::presetNames().contains(name)) {
        return scrcpy::presetOptions(name);
    }
    QVariantMap options = scrcpy::defaultOptions();
    const QVariantMap saved = m_settings->userPreset(name);
    for (auto it = saved.cbegin(); it != saved.cend(); ++it) {
        options.insert(it.key(), it.value());
    }
    return options;
}

QString ScrcpyManager::start(const QString& serial, const QVariantMap& options) {
    if (!m_tools->scrcpyReady()) {
        return tr("scrcpy not found. Set its location in Settings.");
    }
    if (serial.isEmpty()) {
        return tr("No device selected.");
    }
    if (m_sessions.contains(serial)) {
        return tr("scrcpy is already running for this device.");
    }

    const QStringList args = scrcpy::buildArguments(serial, withDefaults(serial, options));
    auto* process = new QProcess(this);
    process->setProcessEnvironment(environment());
    process->setProcessChannelMode(QProcess::MergedChannels);
    process->setProgram(m_tools->scrcpyPath());
    process->setArguments(args);

    Session session;
    session.process = process;
    session.killTimer = new QTimer(process);
    session.killTimer->setSingleShot(true);
    m_sessions.insert(serial, session);

    connect(session.killTimer, &QTimer::timeout, process, [process]() { process->kill(); });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, serial, process]() {
        appendLog(serial, QString::fromUtf8(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::started, this, [this, serial, process]() {
        emit started(serial);
        raiseWindowWhenReady(process);
    });
    connect(process, &QProcess::errorOccurred, this, [this, serial, process](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart) {
            return;
        }
        // QProcess emits no finished() after a failed start, so clean up here.
        const QString message = tr("Failed to start scrcpy: %1").arg(process->errorString());
        appendLog(serial, message + QLatin1Char('\n'));
        const Session failed = m_sessions.take(serial);
        m_lastLogs.insert(serial, failed.log);
        process->deleteLater();
        emit runningChanged();
        emit finished(serial, -1, message);
    });
    connect(process, &QProcess::finished, this,
            [this, serial, process](int exitCode, QProcess::ExitStatus status) {
                const Session session = m_sessions.take(serial);
                m_lastLogs.insert(serial, session.log);
                const QString message = session.stopRequested ? tr("Stopped")
                                        : status == QProcess::CrashExit ? tr("scrcpy crashed")
                                        : exitCode == 0 ? tr("Closed")
                                                        : tr("scrcpy exited with code %1").arg(exitCode);
                process->deleteLater();
                emit runningChanged();
                emit finished(serial, exitCode, message);
            });

    qCInfo(lcScrcpy) << "start" << m_tools->scrcpyPath() << args;
    process->start();
    if (!m_sessions.contains(serial)) {
        return tr("Failed to start scrcpy."); // start failed synchronously
    }
    emit runningChanged();
    return {};
}

void ScrcpyManager::stop(const QString& serial) {
    auto it = m_sessions.find(serial);
    if (it == m_sessions.end()) {
        return;
    }
    it->stopRequested = true;
    if (it->process->state() == QProcess::Running) {
        it->process->terminate();
        it->killTimer->start(3000);
    } else {
        it->process->kill(); // terminate() is a no-op while still starting
    }
}

void ScrcpyManager::appendLog(const QString& serial, const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    auto it = m_sessions.find(serial);
    if (it != m_sessions.end()) {
        it->log += text;
        if (it->log.size() > kMaxLogChars) {
            it->log.remove(0, it->log.size() - kMaxLogChars);
        }
    }
    emit logAppended(serial, text);
}

void ScrcpyManager::query(const QString& serial, const QString& what) {
    static const QHash<QString, QString> flags = {
        {QStringLiteral("encoders"), QStringLiteral("--list-encoders")},
        {QStringLiteral("displays"), QStringLiteral("--list-displays")},
        {QStringLiteral("cameras"), QStringLiteral("--list-cameras")},
        {QStringLiteral("camera-sizes"), QStringLiteral("--list-camera-sizes")},
        {QStringLiteral("apps"), QStringLiteral("--list-apps")},
    };
    const QString flag = flags.value(what);
    if (flag.isEmpty() || !m_tools->scrcpyReady()) {
        emit queryFinished(serial, what, false, tr("Unavailable (is scrcpy installed?)"));
        return;
    }
    QStringList args;
    if (!serial.isEmpty()) {
        args << QStringLiteral("--serial=%1").arg(serial);
    }
    args << flag;
    adb::ProcessRunner::run(this, m_tools->scrcpyPath(), args, 30000,
                            [this, serial, what](const adb::ProcessResult& r) {
                                // scrcpy prints listings on stdout and diagnostics on stderr.
                                const QString text = r.text() + (r.err.isEmpty() ? QString() : QLatin1Char('\n') + r.err);
                                emit queryFinished(serial, what, r.ok(), text.trimmed());
                            },
                            environment());
}

void ScrcpyManager::shutdown() {
    // Intentional blocking wait: only at process exit, bounded, and required so
    // scrcpy windows do not outlive the app.
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        it->stopRequested = true;
        it->process->disconnect(this);
        it->process->terminate();
    }
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        if (!it->process->waitForFinished(1500)) {
            it->process->kill();
            it->process->waitForFinished(500);
        }
    }
    m_sessions.clear();
}

} // namespace polyscope
