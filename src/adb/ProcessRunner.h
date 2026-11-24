#pragma once

#include <QByteArray>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <functional>

class QObject;

namespace polyscope::adb {

struct ProcessResult {
    int exitCode = -1;
    QByteArray out;
    QString err;
    bool started = false;
    bool timedOut = false;

    bool ok() const { return started && !timedOut && exitCode == 0; }
    QString text() const { return QString::fromUtf8(out); }
    // Best human-readable failure explanation.
    QString errorText() const;
};

using ResultCallback = std::function<void(const ProcessResult&)>;

// Runs a program asynchronously. The callback runs on the caller's thread. If
// `context` is destroyed first, the process is killed and the callback is dropped.
// A `timeoutMs` <= 0 disables the timeout.
class ProcessRunner {
public:
    static void run(QObject* context, const QString& program, const QStringList& args,
                    int timeoutMs, ResultCallback callback,
                    const QProcessEnvironment& env = QProcessEnvironment());
};

} // namespace polyscope::adb
