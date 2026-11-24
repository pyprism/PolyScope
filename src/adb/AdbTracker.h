#pragma once

#include <QByteArray>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

namespace polyscope::adb {

class AdbClient;

// Push-based device tracking over the adb server socket (`host:track-devices-l`).
// Reconnects with backoff if the server goes away.
class AdbTracker : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
public:
    explicit AdbTracker(AdbClient* client, QObject* parent = nullptr);

    bool connected() const { return m_connected; }

public slots:
    void restart();
    void stop();

signals:
    void devicesUpdated(const QString& longListing);
    void connectedChanged();

private:
    void startServerThenConnect();
    void connectSocket();
    void onReadyRead();
    void onDisconnected();
    void scheduleReconnect();
    void setConnected(bool connected);

    AdbClient* m_client;
    QTcpSocket m_socket;
    QTimer m_retry;
    QByteArray m_buffer;
    bool m_handshakeDone = false;
    bool m_running = false;
    int m_generation = 0; // invalidates start-server replies from before the last stop()/restart()
    bool m_connected = false;
    int m_backoffMs = 1000;
};

} // namespace polyscope::adb
