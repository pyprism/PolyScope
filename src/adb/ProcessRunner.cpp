#include "adb/ProcessRunner.h"

#include "core/Logging.h"

#include <QPointer>
#include <QProcess>
#include <QTimer>
#include <memory>

namespace polyscope::adb {

QString ProcessResult::errorText() const {
    if (!started) {
        return QStringLiteral("Could not start program");
    }
    if (timedOut) {
        return QStringLiteral("Timed out");
    }
    const QString stderrText = err.trimmed();
    if (!stderrText.isEmpty()) {
        return stderrText;
    }
    const QString stdoutText = text().trimmed();
    return stdoutText.isEmpty() ? QStringLiteral("Exit code %1").arg(exitCode) : stdoutText;
}

void ProcessRunner::run(QObject* context, const QString& program, const QStringList& args,
                        int timeoutMs, ResultCallback callback,
                        const QProcessEnvironment& env) {
    auto* process = new QProcess(context);
    auto finished = std::make_shared<bool>(false);
    auto timedOut = std::make_shared<bool>(false);

    if (!env.isEmpty()) {
        process->setProcessEnvironment(env);
    }

    auto complete = [process, finished, timedOut, callback = std::move(callback)](bool started,
                                                                                  int exitCode) {
        if (*finished) {
            return;
        }
        *finished = true;
        ProcessResult result;
        result.started = started;
        result.exitCode = exitCode;
        result.timedOut = *timedOut;
        result.out = process->readAllStandardOutput();
        result.err = QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        callback(result);
    };

    QObject::connect(process, &QProcess::finished, process,
                     [complete](int exitCode, QProcess::ExitStatus) { complete(true, exitCode); });
    QObject::connect(process, &QProcess::errorOccurred, process,
                     [complete](QProcess::ProcessError error) {
                         if (error == QProcess::FailedToStart) {
                             complete(false, -1);
                         }
                     });

    if (timeoutMs > 0) {
        auto* timer = new QTimer(process);
        timer->setSingleShot(true);
        QObject::connect(timer, &QTimer::timeout, process, [process, timedOut]() {
            *timedOut = true;
            process->kill();
        });
        timer->start(timeoutMs);
    }

    qCDebug(lcAdb) << "run" << program << args;
    process->start(program, args);
    process->closeWriteChannel(); // nothing is ever sent; commands that read stdin must see EOF, not hang
}

} // namespace polyscope::adb
