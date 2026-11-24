#include <QJSEngine>
#include "tools/DeviceActions.h"

#include "adb/AdbClient.h"
#include "adb/AdbParsers.h"
#include "core/AppContext.h"
#include "core/AppSettings.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

namespace polyscope {

namespace {

QString summarize(const adb::ProcessResult& r) {
    const QString out = r.text().trimmed();
    return r.ok() ? (out.isEmpty() ? QStringLiteral("OK") : out) : r.errorText();
}

// adb connect/pair exit 0 even on failure; inspect the message.
bool looksSuccessful(const adb::ProcessResult& r) {
    return r.ok() && !adb::messageIndicatesFailure(r.text() + r.err);
}

} // namespace

DeviceActions::DeviceActions(adb::AdbClient* client, AppSettings* settings, QObject* parent)
    : QObject(parent), m_client(client), m_settings(settings) {}

DeviceActions* DeviceActions::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->actions();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

void DeviceActions::pair(const QString& hostPort, const QString& code) {
    m_client->run(this, QString(), {QStringLiteral("pair"), hostPort.trimmed(), code.trimmed()}, 30000,
                  [this](const adb::ProcessResult& r) {
                      emit operationFinished(QStringLiteral("pair"), looksSuccessful(r), summarize(r));
                  });
}

void DeviceActions::connectDevice(const QString& hostPort) {
    m_client->run(this, QString(), {QStringLiteral("connect"), hostPort.trimmed()}, 20000,
                  [this](const adb::ProcessResult& r) {
                      emit operationFinished(QStringLiteral("connect"), looksSuccessful(r), summarize(r));
                  });
}

void DeviceActions::disconnectDevice(const QString& hostPort) {
    QStringList args{QStringLiteral("disconnect")};
    if (!hostPort.trimmed().isEmpty()) {
        args << hostPort.trimmed();
    }
    m_client->run(this, QString(), args, 15000, [this](const adb::ProcessResult& r) {
        emit operationFinished(QStringLiteral("disconnect"), r.ok(), summarize(r));
    });
}

void DeviceActions::usbToWifi(const QString& serial, int port) {
    if (port <= 0 || port > 65535) {
        emit operationFinished(QStringLiteral("usbToWifi"), false, tr("Invalid port."));
        return;
    }
    m_client->shell(this, serial, QStringLiteral("ip -f inet addr show wlan0 || ip -f inet addr show"), 10000,
                    [this, serial, port](const adb::ProcessResult& r) {
                        const QString ip = adb::parseIpAddress(r.text());
                        if (ip.isEmpty()) {
                            emit operationFinished(QStringLiteral("usbToWifi"), false,
                                                   tr("Could not read the device's Wi-Fi address. Is Wi-Fi connected?"));
                            return;
                        }
                        m_client->run(this, serial, {QStringLiteral("tcpip"), QString::number(port)}, 15000,
                                      [this, ip, port](const adb::ProcessResult& t) {
                                          if (!t.ok()) {
                                              emit operationFinished(QStringLiteral("usbToWifi"), false, t.errorText());
                                              return;
                                          }
                                          // adbd needs a moment to restart in TCP mode.
                                          QTimer::singleShot(1500, this, [this, ip, port]() {
                                              connectDevice(QStringLiteral("%1:%2").arg(ip).arg(port));
                                          });
                                      });
                    });
}

void DeviceActions::screenshot(const QString& serial) {
    m_client->run(this, serial, {QStringLiteral("exec-out"), QStringLiteral("screencap"), QStringLiteral("-p")}, 20000,
                  [this, serial](const adb::ProcessResult& r) {
                      static const QByteArray pngMagic("\x89PNG", 4);
                      if (!r.ok() || !r.out.startsWith(pngMagic)) {
                          emit operationFinished(QStringLiteral("screenshot"), false,
                                                 r.ok() ? tr("Device returned no image.") : r.errorText());
                          return;
                      }
                      const QString dir = m_settings->screenshotDir();
                      if (!QDir().mkpath(dir)) {
                          emit operationFinished(QStringLiteral("screenshot"), false, tr("Cannot create %1").arg(dir));
                          return;
                      }
                      QString safe = serial;
                      safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
                      const QString path = QDir(dir).filePath(
                          QStringLiteral("PolyScope_%1_%2.png")
                              .arg(safe, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
                      QFile file(path);
                      if (!file.open(QIODevice::WriteOnly) || file.write(r.out) != r.out.size()) {
                          emit operationFinished(QStringLiteral("screenshot"), false, tr("Cannot write %1").arg(path));
                          return;
                      }
                      file.close();
                      emit screenshotSaved(serial, path);
                      emit operationFinished(QStringLiteral("screenshot"), true, path);
                  });
}

void DeviceActions::keyEvent(const QString& serial, const QString& key) {
    static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_]+$"));
    if (!safe.match(key).hasMatch()) {
        emit operationFinished(QStringLiteral("keyEvent"), false, tr("Invalid key."));
        return;
    }
    m_client->shell(this, serial, QStringLiteral("input keyevent ") + key, 10000, [this](const adb::ProcessResult& r) {
        emit operationFinished(QStringLiteral("keyEvent"), r.ok(), summarize(r));
    });
}

void DeviceActions::inputText(const QString& serial, const QString& text) {
    // `input text` only understands ASCII; spaces must be encoded as %s.
    QString encoded = text;
    encoded.replace(QLatin1Char(' '), QStringLiteral("%s"));
    for (const QChar c : std::as_const(encoded)) {
        if (c.unicode() > 126 || c.unicode() < 32) {
            emit operationFinished(QStringLiteral("inputText"), false,
                                   tr("Only ASCII text can be typed through adb. Use scrcpy for other characters."));
            return;
        }
    }
    m_client->shell(this, serial, QStringLiteral("input text ") + adb::shellQuote(encoded), 10000,
                    [this](const adb::ProcessResult& r) {
                        emit operationFinished(QStringLiteral("inputText"), r.ok(), summarize(r));
                    });
}

void DeviceActions::reboot(const QString& serial, const QString& mode) {
    static const QStringList allowed{QString(), QStringLiteral("bootloader"), QStringLiteral("recovery"),
                                     QStringLiteral("sideload"), QStringLiteral("sideload-auto-reboot"),
                                     QStringLiteral("fastboot")};
    if (!allowed.contains(mode)) {
        emit operationFinished(QStringLiteral("reboot"), false, tr("Invalid reboot mode."));
        return;
    }
    QStringList args{QStringLiteral("reboot")};
    if (!mode.isEmpty()) {
        args << mode;
    }
    m_client->run(this, serial, args, 30000, [this](const adb::ProcessResult& r) {
        emit operationFinished(QStringLiteral("reboot"), r.ok(), summarize(r));
    });
}

void DeviceActions::overview(const QString& serial) {
    const QString script = QStringLiteral(
        "getprop; echo '@@WM'; wm size; wm density; echo '@@STORAGE'; df -h /data; "
        "echo '@@MEMORY'; head -n 3 /proc/meminfo; echo '@@BATTERY'; dumpsys battery; "
        "echo '@@NETWORK'; ip -f inet addr show");
    m_client->shell(this, serial, script, 30000, [this, serial](const adb::ProcessResult& r) {
        if (!r.ok()) {
            emit operationFinished(QStringLiteral("overview"), false, r.errorText());
            return;
        }
        // Split the combined output on "@@NAME" marker lines.
        QStringList names{QStringLiteral("PROPS")};
        QStringList bodies{QString()};
        for (const QString& line : r.text().split(QLatin1Char('\n'))) {
            if (line.startsWith(QStringLiteral("@@"))) {
                names << line.mid(2).trimmed();
                bodies << QString();
            } else {
                bodies.last() += line + QLatin1Char('\n');
            }
        }
        QVariantList sections;

        // Properties first, as key/value rows.
        static const QList<QPair<QString, QString>> keys = {
            {QStringLiteral("Model"), QStringLiteral("ro.product.model")},
            {QStringLiteral("Manufacturer"), QStringLiteral("ro.product.manufacturer")},
            {QStringLiteral("Device"), QStringLiteral("ro.product.device")},
            {QStringLiteral("Android"), QStringLiteral("ro.build.version.release")},
            {QStringLiteral("SDK"), QStringLiteral("ro.build.version.sdk")},
            {QStringLiteral("Security patch"), QStringLiteral("ro.build.version.security_patch")},
            {QStringLiteral("Build"), QStringLiteral("ro.build.display.id")},
            {QStringLiteral("Fingerprint"), QStringLiteral("ro.build.fingerprint")},
            {QStringLiteral("ABI"), QStringLiteral("ro.product.cpu.abi")},
            {QStringLiteral("Hardware"), QStringLiteral("ro.hardware")},
            {QStringLiteral("Serial"), QStringLiteral("ro.serialno")},
        };
        const auto props = adb::parseGetprop(bodies.value(0));
        QVariantList rows;
        for (const auto& kv : keys) {
            const QString value = props.value(kv.second);
            if (!value.isEmpty()) {
                rows.append(QVariantMap{{QStringLiteral("key"), kv.first}, {QStringLiteral("value"), value}});
            }
        }
        sections.append(QVariantMap{{QStringLiteral("title"), tr("Device")}, {QStringLiteral("rows"), rows}});

        static const QHash<QString, QString> titles = {
            {QStringLiteral("WM"), QStringLiteral("Display")},
            {QStringLiteral("STORAGE"), QStringLiteral("Storage")},
            {QStringLiteral("MEMORY"), QStringLiteral("Memory")},
            {QStringLiteral("BATTERY"), QStringLiteral("Battery")},
            {QStringLiteral("NETWORK"), QStringLiteral("Network")},
        };
        for (qsizetype i = 1; i < names.size(); ++i) {
            sections.append(QVariantMap{{QStringLiteral("title"), titles.value(names.at(i), names.at(i))},
                                        {QStringLiteral("text"), bodies.at(i).trimmed()}});
        }
        emit overviewReady(serial, sections);
    });
}

QString DeviceActions::revealPath(const QString& path) {
    return QUrl::fromLocalFile(path).toString();
}

bool DeviceActions::writeText(const QString& path, const QString& text) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    file.write(text.toUtf8());
    return file.commit();
}

} // namespace polyscope
