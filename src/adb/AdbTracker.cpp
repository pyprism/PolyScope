#include "adb/AdbTracker.h"

#include "adb/AdbClient.h"
#include "adb/AdbParsers.h"
#include "core/Logging.h"

#include <QHostAddress>

namespace polyscope::adb {

AdbTracker::AdbTracker(AdbClient* client, QObject* parent) : QObject(parent), m_client(client) {
    m_retry.setSingleShot(true);
    connect(&m_retry, &QTimer::timeout, this, &AdbTracker::startServerThenConnect);
    connect(&m_socket, &QTcpSocket::connected, this, [this]() {
        m_handshakeDone = false;
        m_buffer.clear();
        m_socket.write(encodeServerRequest("host:track-devices-l"));
    });
    connect(&m_socket, &QTcpSocket::readyRead, this, &AdbTracker::onReadyRead);
    connect(&m_socket, &QTcpSocket::disconnected, this, &AdbTracker::onDisconnected);
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, [this]() {
        if (m_running && m_socket.state() == QAbstractSocket::UnconnectedState) {
            onDisconnected();
        }
    });
}

void AdbTracker::restart() {
    stop();
    m_running = true;
    m_backoffMs = 1000;
    startServerThenConnect();
}

void AdbTracker::stop() {
    m_running = false;
    ++m_generation;
    m_retry.stop();
    m_socket.abort();
    setConnected(false);
}

void AdbTracker::startServerThenConnect() {
    if (!m_running) {
        return;
    }
    if (!m_client->isAvailable()) {
        scheduleReconnect();
        return;
    }
    const int generation = m_generation;
    m_client->run(this, QString(), {QStringLiteral("start-server")}, 15000, [this, generation](const ProcessResult& r) {
        if (generation != m_generation) {
            return; // a newer restart owns the connection now
        }
        if (!r.ok()) {
            qCWarning(lcDevices) << "adb start-server failed:" << r.errorText();
        }
        if (m_running) {
            connectSocket();
        }
    });
}

void AdbTracker::connectSocket() {
    m_socket.abort();
    m_socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(AdbClient::serverPort()));
}

void AdbTracker::onReadyRead() {
    m_buffer.append(m_socket.readAll());
    if (!m_handshakeDone) {
        if (m_buffer.size() < 4) {
            return;
        }
        const QByteArray status = m_buffer.left(4);
        m_buffer.remove(0, 4);
        if (status != "OKAY") {
            qCWarning(lcDevices) << "track-devices rejected:" << status << m_buffer.left(64);
            m_socket.abort();
            return;
        }
        m_handshakeDone = true;
        m_backoffMs = 1000;
        setConnected(true);
    }
    for (;;) {
        bool malformed = false;
        const auto frame = takeServerFrame(m_buffer, &malformed);
        if (!frame) {
            if (malformed) {
                m_socket.abort();
            }
            break;
        }
        emit devicesUpdated(QString::fromUtf8(*frame));
    }
}

void AdbTracker::onDisconnected() {
    const bool wasConnected = m_connected;
    setConnected(false);
    if (wasConnected) {
        emit devicesUpdated(QString()); // clear list: nothing reachable
    }
    scheduleReconnect();
}

void AdbTracker::scheduleReconnect() {
    if (!m_running || m_retry.isActive()) {
        return;
    }
    m_retry.start(m_backoffMs);
    m_backoffMs = qMin(m_backoffMs * 2, 15000);
}

void AdbTracker::setConnected(bool connected) {
    if (m_connected != connected) {
        m_connected = connected;
        qCInfo(lcDevices) << (connected ? "tracking devices" : "device tracking stopped");
        emit connectedChanged();
    }
}

} // namespace polyscope::adb
