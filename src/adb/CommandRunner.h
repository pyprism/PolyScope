#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace polyscope {
class AppSettings;
}

namespace polyscope::adb {

class AdbClient;

// Runs entries of the command catalogue (and raw adb command lines) and reports
// results asynchronously. This is what makes the whole adb surface reachable from QML.
class CommandRunner : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Commands)
    QML_SINGLETON

public:
    CommandRunner(AdbClient* client, AppSettings* settings, QObject* parent = nullptr);
    static CommandRunner* create(QQmlEngine*, QJSEngine*);

    // [{name, commands:[{id,title,group,mode,dangerous,needsDevice,params:[...]}]}]
    Q_INVOKABLE QVariantList groups() const;
    Q_INVOKABLE QVariantMap command(const QString& id) const;
    // Fuzzy text search over id/title/group for a command palette.
    Q_INVOKABLE QVariantList search(const QString& text) const;
    // "adb -s SERIAL shell 'ls'" or an error string prefixed with "error: ".
    Q_INVOKABLE QString preview(const QString& id, const QString& serial, const QVariantMap& values) const;

    // Returns a request id (> 0). Result arrives through `finished`.
    Q_INVOKABLE int run(const QString& id, const QString& serial, const QVariantMap& values);
    // Raw adb arguments, e.g. "shell getprop ro.build.version.sdk" (a leading "adb" is ignored).
    Q_INVOKABLE int runRaw(const QString& serial, const QString& commandLine);
    // Same command on several devices; returns the request ids.
    Q_INVOKABLE QVariantList runBatch(const QStringList& serials, const QString& id, const QVariantMap& values);

signals:
    void finished(int requestId, const QString& commandId, const QString& serial, bool ok,
                  const QString& output, int exitCode);

private:
    int nextId() { return ++m_nextId; }
    void fail(int id, const QString& commandId, const QString& serial, const QString& message);

    AdbClient* m_client;
    AppSettings* m_settings;
    int m_nextId = 0;
};

} // namespace polyscope::adb
