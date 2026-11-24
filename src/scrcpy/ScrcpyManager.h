#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QTimer;

namespace polyscope {

class AppSettings;
class ToolPaths;

// One scrcpy process per device serial. scrcpy opens its own window; this class
// builds the command line, supervises the process and collects its output.
class ScrcpyManager : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Scrcpy)
    QML_SINGLETON
    Q_PROPERTY(QStringList runningSerials READ runningSerials NOTIFY runningChanged)

public:
    ScrcpyManager(ToolPaths* tools, AppSettings* settings, QObject* parent = nullptr);
    ~ScrcpyManager() override;
    static ScrcpyManager* create(QQmlEngine*, QJSEngine*);

    QStringList runningSerials() const;

    // Returns an empty string on success, otherwise an error message.
    Q_INVOKABLE QString start(const QString& serial, const QVariantMap& options);
    Q_INVOKABLE void stop(const QString& serial);
    Q_INVOKABLE bool isRunning(const QString& serial) const;
    Q_INVOKABLE QString log(const QString& serial) const;
    Q_INVOKABLE void clearLog(const QString& serial);

    Q_INVOKABLE QString commandLine(const QString& serial, const QVariantMap& options) const;
    Q_INVOKABLE QStringList warnings(const QVariantMap& options) const;
    Q_INVOKABLE QVariantList schema() const;
    Q_INVOKABLE QStringList groups() const;
    Q_INVOKABLE QVariantMap defaults() const;
    Q_INVOKABLE QStringList presetNames() const;
    Q_INVOKABLE QVariantMap preset(const QString& name) const;

    // what: encoders | displays | cameras | camera-sizes | apps
    Q_INVOKABLE void query(const QString& serial, const QString& what);

    // Stops every session; blocks briefly. Only for application shutdown.
    void shutdown();

signals:
    void runningChanged();
    void started(const QString& serial);
    void finished(const QString& serial, int exitCode, const QString& message);
    void logAppended(const QString& serial, const QString& text);
    void queryFinished(const QString& serial, const QString& what, bool ok, const QString& text);

private:
    struct Session {
        QProcess* process = nullptr;
        QString log;
        QTimer* killTimer = nullptr;
        bool stopRequested = false;
    };

    static void raiseWindowWhenReady(QProcess* process);
    QProcessEnvironment environment() const;
    QVariantMap withDefaults(const QString& serial, QVariantMap options) const;
    void appendLog(const QString& serial, const QString& text);

    ToolPaths* m_tools;
    AppSettings* m_settings;
    QHash<QString, Session> m_sessions;
    QHash<QString, QString> m_lastLogs; // logs of finished sessions
};

} // namespace polyscope
