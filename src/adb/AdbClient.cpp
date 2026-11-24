#include "adb/AdbClient.h"

#include "core/ToolPaths.h"

#include <QProcess>
#include <QTimer>

namespace polyscope::adb {

AdbClient::AdbClient(ToolPaths* tools, QObject* parent) : QObject(parent), m_tools(tools) {}

bool AdbClient::isAvailable() const {
    return m_tools->adbReady();
}

QString AdbClient::program() const {
    return m_tools->adbPath();
}

QStringList AdbClient::withSerial(const QString& serial, const QStringList& args) const {
    QStringList full;
    if (!serial.isEmpty()) {
        full << QStringLiteral("-s") << serial;
    }
    full << args;
    return full;
}

void AdbClient::run(QObject* context, const QString& serial, const QStringList& args,
                    int timeoutMs, ResultCallback callback) const {
    if (!isAvailable()) {
        ProcessResult result;
        result.err = QStringLiteral("adb not found. Set its location in Settings.");
        // Deliver asynchronously: callers expect run() to return before the callback fires.
        QTimer::singleShot(0, context, [callback = std::move(callback), result]() { callback(result); });
        return;
    }
    ProcessRunner::run(context, program(), withSerial(serial, args), timeoutMs,
                       std::move(callback));
}

void AdbClient::shell(QObject* context, const QString& serial, const QString& command,
                      int timeoutMs, ResultCallback callback) const {
    run(context, serial, {QStringLiteral("shell"), command}, timeoutMs, std::move(callback));
}

QProcess* AdbClient::createProcess(QObject* parent, const QString& serial,
                                   const QStringList& args) const {
    auto* process = new QProcess(parent);
    process->setProgram(program());
    process->setArguments(withSerial(serial, args));
    return process;
}

int AdbClient::serverPort() {
    bool ok = false;
    const int port = qEnvironmentVariable("ANDROID_ADB_SERVER_PORT").toInt(&ok);
    return ok && port > 0 && port < 65536 ? port : 5037;
}

} // namespace polyscope::adb
