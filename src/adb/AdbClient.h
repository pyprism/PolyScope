#pragma once

#include "adb/ProcessRunner.h"

#include <QObject>

class QProcess;

namespace polyscope {
class ToolPaths;
}

namespace polyscope::adb {

// Thin wrapper that runs the user-selected adb binary.
class AdbClient : public QObject {
    Q_OBJECT
public:
    explicit AdbClient(ToolPaths* tools, QObject* parent = nullptr);

    bool isAvailable() const;
    QString program() const;

    // `adb [-s serial] <args>`; empty serial runs a host command.
    void run(QObject* context, const QString& serial, const QStringList& args, int timeoutMs,
             ResultCallback callback) const;
    // `adb -s serial shell <command>`
    void shell(QObject* context, const QString& serial, const QString& command, int timeoutMs,
               ResultCallback callback) const;
    // Unstarted QProcess configured for adb; caller starts it and owns it via `parent`.
    QProcess* createProcess(QObject* parent, const QString& serial, const QStringList& args) const;

    static int serverPort();

private:
    QStringList withSerial(const QString& serial, const QStringList& args) const;

    ToolPaths* m_tools;
};

} // namespace polyscope::adb
