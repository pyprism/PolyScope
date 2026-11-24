#include "adb/AdbParsers.h"

#include <QtTest>

using namespace polyscope::adb;

class TstAdbParsers : public QObject {
    Q_OBJECT
private slots:
    void devicesLong() {
        const QString text = QStringLiteral(
            "List of devices attached\n"
            "R5CT1234ABC            device usb:1-1 product:o1sxeea model:SM_G991B device:o1s transport_id:1\n"
            "192.168.1.20:5555      device product:redfin model:Pixel_5 device:redfin transport_id:2\n"
            "emulator-5554          offline transport_id:3\n"
            "ZX1G22          unauthorized usb:2-1 transport_id:4\n"
            "* daemon started successfully\n");
        const auto list = parseDevices(text);
        QCOMPARE(list.size(), 4);
        QCOMPARE(list[0].serial, QStringLiteral("R5CT1234ABC"));
        QCOMPARE(list[0].model, QStringLiteral("SM G991B"));
        QVERIFY(list[0].isOnline());
        QVERIFY(!isWirelessSerial(list[0].serial));
        QVERIFY(isWirelessSerial(list[1].serial));
        QCOMPARE(list[1].transportId, QStringLiteral("2"));
        QCOMPARE(list[2].state, QStringLiteral("offline"));
        QCOMPARE(list[3].state, QStringLiteral("unauthorized"));
    }
    void devicesNoPermissions() {
        const auto list = parseDevices(QStringLiteral(
            "0123456789ABCDEF      no permissions (user in plugdev group; are your udev rules wrong?); see [http://developer.android.com/tools/device.html] usb:1-2 transport_id:5\n"));
        QCOMPARE(list.size(), 1);
        QCOMPARE(list[0].state, QStringLiteral("no permissions"));
        QCOMPARE(list[0].usb, QStringLiteral("1-2"));
    }
    void devicesEmpty() {
        QVERIFY(parseDevices(QStringLiteral("List of devices attached\n\n")).isEmpty());
    }
    void getprop() {
        const auto props = parseGetprop(QStringLiteral(
            "[ro.product.model]: [Pixel 5]\r\n[ro.build.version.release]: [14]\n[empty]: []\njunk\n"));
        QCOMPARE(props.value(QStringLiteral("ro.product.model")), QStringLiteral("Pixel 5"));
        QCOMPARE(props.value(QStringLiteral("ro.build.version.release")), QStringLiteral("14"));
        QVERIFY(props.contains(QStringLiteral("empty")));
        QCOMPARE(props.size(), 3);
    }
    void battery() {
        const auto b = parseBattery(QStringLiteral(
            "Current Battery Service state:\n  AC powered: false\n  USB powered: true\n"
            "  Wireless powered: false\n  status: 2\n  health: 2\n  level: 87\n  voltage: 4123\n"
            "  temperature: 281\n"));
        QVERIFY(b.isValid());
        QCOMPARE(b.level, 87);
        QCOMPARE(b.status, QStringLiteral("Charging"));
        QCOMPARE(b.power, QStringLiteral("USB"));
        QCOMPARE(b.voltageMv, 4123);
        QVERIFY(qFuzzyCompare(b.temperatureC, 28.1));
        QVERIFY(!parseBattery(QString()).isValid());
    }
    void mdns() {
        const auto list = parseMdnsServices(QStringLiteral(
            "List of discovered mdns services\n"
            "adb-R5CT1234-abc\t_adb-tls-connect._tcp\t192.168.1.20:37123\n"
            "adb-R5CT1234-abc\t_adb-tls-pairing._tcp\t192.168.1.20:40567\n"));
        QCOMPARE(list.size(), 2);
        QVERIFY(list[0].isConnect());
        QVERIFY(list[1].isPairing());
        QCOMPARE(list[1].address(), QStringLiteral("192.168.1.20:40567"));
    }
    void lsLong() {
        const auto list = parseLsLong(QStringLiteral(
            "total 48\n"
            "drwxrwx--x  2 root sdcard_rw 4096 2024-05-01 10:11 Alarms\n"
            "-rw-rw----  1 root sdcard_rw 1234567 2024-05-01 10:12 my file.txt\n"
            "lrwxrwxrwx  1 root root       21 2024-05-01 10:11 sdcard -> /storage/self/primary\n"
            "drwxrwx--x  2 root sdcard_rw 4096 2024-05-01 10:11 .\n"));
        QCOMPARE(list.size(), 3);
        QVERIFY(list[0].isDir);
        QCOMPARE(list[1].name, QStringLiteral("my file.txt"));
        QCOMPARE(list[1].size, qint64(1234567));
        QVERIFY(list[2].isLink);
        QCOMPARE(list[2].name, QStringLiteral("sdcard"));
        QCOMPARE(list[2].linkTarget, QStringLiteral("/storage/self/primary"));
    }
    void packages() {
        const auto list = parsePackages(QStringLiteral("package:com.b\npackage:com.A\nnoise\n"));
        QCOMPARE(list, (QStringList{QStringLiteral("com.A"), QStringLiteral("com.b")}));
    }
    void services() {
        const auto s = parseServiceList(QStringLiteral(
            "Found 2 services:\n0\tactivity: [android.app.IActivityManager]\n1\tbattery: []\n"));
        QCOMPARE(s.size(), 2);
        QCOMPARE(s[0].name, QStringLiteral("activity"));
        QCOMPARE(s[0].interface, QStringLiteral("android.app.IActivityManager"));
        const auto d = parseDumpsysList(QStringLiteral("Currently running services:\n  battery\n  alarm\n"));
        QCOMPARE(d, (QStringList{QStringLiteral("alarm"), QStringLiteral("battery")}));
        const auto r = parseRunningServices(QStringLiteral(
            "  * ServiceRecord{1a2b3c u0 com.pkg/.MyService c:com.pkg}\n"
            "  * ServiceRecord{1a2b3c u0 com.pkg/.MyService c:com.pkg}\n"
            "  * ServiceRecord{ffee u10 org.x/org.x.Svc}\n"));
        QCOMPARE(r.size(), 2);
        QCOMPARE(r[0].component, QStringLiteral("com.pkg/.MyService"));
        QCOMPARE(r[1].userId, 10);
    }
    void ip() {
        QCOMPARE(parseIpAddress(QStringLiteral("1: lo\n    inet 127.0.0.1/8 scope host lo\n"
                                               "3: wlan0\n    inet 192.168.1.44/24 brd 192.168.1.255\n")),
                 QStringLiteral("192.168.1.44"));
        QVERIFY(parseIpAddress(QStringLiteral("nothing")).isEmpty());
    }
    void framing() {
        QCOMPARE(encodeServerRequest("host:track-devices-l"), QByteArray("0014host:track-devices-l"));
        QByteArray buf = QByteArrayLiteral("0005hello0003ab");
        auto f1 = takeServerFrame(buf);
        QVERIFY(f1.has_value());
        QCOMPARE(*f1, QByteArray("hello"));
        QVERIFY(!takeServerFrame(buf).has_value()); // partial
        buf.append('c');
        QCOMPARE(*takeServerFrame(buf), QByteArray("abc"));
        QByteArray bad = QByteArrayLiteral("zzzzdata");
        bool malformed = false;
        QVERIFY(!takeServerFrame(bad, &malformed).has_value());
        QVERIFY(malformed);
        QByteArray empty = QByteArrayLiteral("0000");
        QVERIFY(takeServerFrame(empty)->isEmpty());
    }
    void quoting() {
        QCOMPARE(shellQuote(QStringLiteral("a b")), QStringLiteral("'a b'"));
        QCOMPARE(shellQuote(QStringLiteral("it's")), QStringLiteral("'it'\\''s'"));
    }
};

QTEST_APPLESS_MAIN(TstAdbParsers)
#include "tst_adbparsers.moc"
