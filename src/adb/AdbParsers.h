#pragma once

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <optional>

// Pure parsers for adb / Android shell output. No process or UI dependencies so
// they can be unit-tested with canned strings.
namespace polyscope::adb {

struct DeviceEntry {
    QString serial;
    QString state; // device | unauthorized | offline | authorizing | connecting | ...
    QString model;
    QString product;
    QString device;
    QString transportId;
    QString usb;

    bool isOnline() const { return state == QStringLiteral("device"); }
    bool operator==(const DeviceEntry&) const = default;
};

struct BatteryInfo {
    int level = -1;           // percent, -1 unknown
    QString status;           // Charging / Discharging / Full / ...
    QString power;            // AC / USB / Wireless / "" (not plugged)
    double temperatureC = -1; // -1 unknown
    int voltageMv = -1;

    bool isValid() const { return level >= 0; }
};

struct MdnsService {
    QString name;
    QString type; // _adb-tls-connect._tcp, _adb-tls-pairing._tcp, _adb._tcp
    QString host;
    int port = 0;

    bool isPairing() const { return type.startsWith(QStringLiteral("_adb-tls-pairing")); }
    bool isConnect() const { return !isPairing(); }
    QString address() const { return host + QLatin1Char(':') + QString::number(port); }
};

struct FileEntry {
    QString name;
    QString permissions;
    QString modified;
    QString linkTarget;
    qint64 size = 0;
    bool isDir = false;
    bool isLink = false;
};

QList<DeviceEntry> parseDevices(const QString& text);
QHash<QString, QString> parseGetprop(const QString& text);
BatteryInfo parseBattery(const QString& dumpsysBattery);
QList<MdnsService> parseMdnsServices(const QString& text);
QList<FileEntry> parseLsLong(const QString& text);

struct BinderService {
    QString name;      // "activity"
    QString interface; // "android.app.IActivityManager"
};
struct RunningService {
    QString component; // "com.pkg/.MyService"
    QString package;
    int userId = 0;
};
// `service list`
QList<BinderService> parseServiceList(const QString& text);
// `dumpsys -l` (names of dumpsys-able services)
QStringList parseDumpsysList(const QString& text);
// `dumpsys activity services`
QList<RunningService> parseRunningServices(const QString& text);
QStringList parsePackages(const QString& text);
// First non-loopback IPv4 address in `ip -f inet addr show` output; empty if none.
QString parseIpAddress(const QString& text);

// `adb connect` / `adb pair` exit 0 even when they fail; decide from the message text.
bool messageIndicatesFailure(const QString& text);

// True when argv (without the program name) selects a device itself: `-s SERIAL` before the subcommand.
// A later "-s" (e.g. `logcat -s Tag`) belongs to the subcommand, not to adb.
bool hasExplicitSerial(const QStringList& args);

// "ip:port" or mDNS "adb-XXXX._adb-tls-connect._tcp" serials are wireless.
bool isWirelessSerial(const QString& serial);

// adb server wire protocol: "%04x" length prefix + payload.
QByteArray encodeServerRequest(const QByteArray& payload);
// Removes and returns one complete frame from the front of `buffer`.
std::optional<QByteArray> takeServerFrame(QByteArray& buffer, bool* malformed = nullptr);

// Quote an argument for the Android (POSIX) shell: 'it'\''s'.
QString shellQuote(const QString& arg);

} // namespace polyscope::adb
