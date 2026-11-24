#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace polyscope {

namespace adb {
class AdbClient;
}
class AppSettings;

// Higher-level device operations that need more than one adb call or binary output.
// Simple one-shot commands live in the command catalogue instead.
class DeviceActions : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Actions)
    QML_SINGLETON

public:
    DeviceActions(adb::AdbClient* client, AppSettings* settings, QObject* parent = nullptr);
    static DeviceActions* create(QQmlEngine*, QJSEngine*);

    Q_INVOKABLE void pair(const QString& hostPort, const QString& code);
    Q_INVOKABLE void connectDevice(const QString& hostPort);
    Q_INVOKABLE void disconnectDevice(const QString& hostPort);
    // Reads the device's Wi-Fi IP, restarts adbd on TCP and connects to it.
    Q_INVOKABLE void usbToWifi(const QString& serial, int port = 5555);

    Q_INVOKABLE void screenshot(const QString& serial);
    Q_INVOKABLE void keyEvent(const QString& serial, const QString& key);
    Q_INVOKABLE void inputText(const QString& serial, const QString& text);
    Q_INVOKABLE void reboot(const QString& serial, const QString& mode);

    // Collects props, display, storage, memory, battery and network in a single adb call.
    Q_INVOKABLE void overview(const QString& serial);

    Q_INVOKABLE static QString revealPath(const QString& path);
    // Writes UTF-8 text, creating parent folders. Used for report/log export from QML.
    Q_INVOKABLE static bool writeText(const QString& path, const QString& text);

signals:
    // op: pair | connect | disconnect | usbToWifi | keyEvent | inputText | reboot | screenshot
    void operationFinished(const QString& op, bool ok, const QString& message);
    void screenshotSaved(const QString& serial, const QString& path);
    // sections: [{title, rows:[{key,value}], text}]
    void overviewReady(const QString& serial, const QVariantList& sections);

private:
    adb::AdbClient* m_client;
    AppSettings* m_settings;
};

} // namespace polyscope
