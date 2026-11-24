#include "tools/ShellConsole.h"

#include "adb/AdbClient.h"
#include "core/AppContext.h"

#include <QTimer>

namespace polyscope {

ShellConsole::ShellConsole(QObject* parent) : QObject(parent) {}

ShellConsole::~ShellConsole() {
    if (m_process) {
        m_process->disconnect(this);
        m_process->kill();
        m_process->waitForFinished(500); // object teardown only
    }
}

bool ShellConsole::running() const {
    return m_process && m_process->state() != QProcess::NotRunning;
}

void ShellConsole::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    stop();
    m_serial = serial;
    emit serialChanged();
}

void ShellConsole::start() {
    if (running() || m_serial.isEmpty()) {
        return;
    }
    auto* client = AppContext::instance()->adb();
    if (!client->isAvailable()) {
        emit output(tr("adb not found.\n"));
        return;
    }
    // -T: never request a pty, so output is plain text.
    m_decoder = QStringDecoder(QStringDecoder::Utf8); // fresh state: a multibyte char never spans sessions
    m_process = client->createProcess(this, m_serial, {QStringLiteral("shell"), QStringLiteral("-T")});
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this,
            [this]() { emit output(m_decoder(m_process->readAllStandardOutput())); });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart && m_process) {
            emit output(tr("Failed to start adb: %1\n").arg(m_process->errorString()));
            m_process->deleteLater();
            m_process = nullptr;
            emit runningChanged();
        }
    });
    connect(m_process, &QProcess::finished, this, [this](int code) {
        emit output(tr("\n[shell exited with code %1]\n").arg(code));
        m_process->deleteLater();
        m_process = nullptr;
        emit runningChanged();
    });
    m_process->start();
    emit runningChanged();
}

void ShellConsole::stop() {
    if (!m_process) {
        return;
    }
    QProcess* old = std::exchange(m_process, nullptr);
    old->disconnect(this);
    connect(old, &QProcess::finished, old, &QObject::deleteLater);
    old->write("exit\n");
    old->closeWriteChannel();
    QTimer::singleShot(1000, old, &QProcess::kill);
    emit runningChanged();
}

void ShellConsole::send(const QString& line) {
    if (!running()) {
        start();
    }
    if (m_process) {
        m_process->write(line.toUtf8() + '\n');
    }
}

} // namespace polyscope
