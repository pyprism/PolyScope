#include "adb/AdbParsers.h"
#include "adb/MonitorParsers.h"

#include <QtTest>

using namespace polyscope::adb;

// Malformed / unusual input: parsers must degrade to empty results, never crash or throw.
class TstParsersEdge : public QObject {
    Q_OBJECT
private slots:
    void emptyInputs() {
        QVERIFY(parseDevices(QString()).isEmpty());
        QVERIFY(parseGetprop(QString()).isEmpty());
        QVERIFY(!parseBattery(QString()).isValid());
        QVERIFY(parseMdnsServices(QString()).isEmpty());
        QVERIFY(parseLsLong(QString()).isEmpty());
        QVERIFY(parseServiceList(QString()).isEmpty());
        QVERIFY(parseDumpsysList(QString()).isEmpty());
        QVERIFY(parseRunningServices(QString()).isEmpty());
        QVERIFY(parsePackages(QString()).isEmpty());
        QVERIFY(parseIpAddress(QString()).isEmpty());
        QVERIFY(!parseMemInfo(QString()).isValid());
        QVERIFY(parseCpuStat(QString()).isEmpty());
        QCOMPARE(parseNetDev(QString()).rxBytes, qint64(0));
        QVERIFY(parseDf(QString()).isEmpty());
        QVERIFY(!parseDiskStats(QString()).isValid());
        QVERIFY(parseTop(QString()).isEmpty());
    }
    void garbageInputs() {
        const QString junk = QStringLiteral("\x01\x02 ::: \n\n  \t\n[[[ ]]] 99999999999999999999 \n%%%");
        parseDevices(junk);
        parseGetprop(junk);
        parseBattery(junk);
        parseMdnsServices(junk);
        parseLsLong(junk);
        parseServiceList(junk);
        parseRunningServices(junk);
        parseMemInfo(junk);
        parseCpuStat(junk);
        parseNetDev(junk);
        parseDf(junk);
        parseDiskStats(junk);
        parseTop(junk);
        QVERIFY(true);
    }
    void windowsLineEndings() {
        const QList<DeviceEntry> d = parseDevices(
            QStringLiteral("List of devices attached\r\nABC123 device product:p model:Pixel_7 device:d transport_id:3\r\n\r\n"));
        QCOMPARE(d.size(), 1);
        QCOMPARE(d.first().serial, QStringLiteral("ABC123"));
        QCOMPARE(d.first().model, QStringLiteral("Pixel 7")); // adb encodes spaces as underscores
        QVERIFY(d.first().isOnline());
    }
    void deviceStatesNotOnline() {
        const QList<DeviceEntry> d = parseDevices(
            QStringLiteral("List of devices attached\nAAA unauthorized transport_id:1\nBBB offline transport_id:2\n"));
        QCOMPARE(d.size(), 2);
        QVERIFY(!d.at(0).isOnline());
        QCOMPARE(d.at(0).state, QStringLiteral("unauthorized"));
        QVERIFY(!d.at(1).isOnline());
    }
    void getpropIgnoresNoise() {
        const auto p = parseGetprop(QStringLiteral("[ro.a]: [1]\nnot a prop\n[ro.empty]: []\n[ro.sp]: [x y]\n"));
        QCOMPARE(p.value(QStringLiteral("ro.a")), QStringLiteral("1"));
        QCOMPARE(p.value(QStringLiteral("ro.empty")), QString());
        QCOMPARE(p.value(QStringLiteral("ro.sp")), QStringLiteral("x y"));
        QVERIFY(!p.contains(QStringLiteral("not a prop")));
    }
    void batteryTemperatureTenths() {
        const BatteryInfo b = parseBattery(QStringLiteral(
            "Current Battery Service state:\n  AC powered: false\n  USB powered: true\n  status: 2\n  level: 77\n  voltage: 4012\n  temperature: 305\n"));
        QVERIFY(b.isValid());
        QCOMPARE(b.level, 77);
        QCOMPARE(b.voltageMv, 4012);
        QCOMPARE(b.temperatureC, 30.5);
    }
    void failureMessages() {
        QVERIFY(messageIndicatesFailure(QStringLiteral("failed to connect to 1.2.3.4:5555")));
        QVERIFY(messageIndicatesFailure(QStringLiteral("cannot connect to host")));
        QVERIFY(!messageIndicatesFailure(QStringLiteral("connected to 1.2.3.4:5555")));
        QVERIFY(!messageIndicatesFailure(QString()));
    }
    void explicitSerialOnlyBeforeSubcommand() {
        QVERIFY(hasExplicitSerial({QStringLiteral("-s"), QStringLiteral("X"), QStringLiteral("shell")}));
        QVERIFY(hasExplicitSerial({QStringLiteral("-H"), QStringLiteral("h"), QStringLiteral("-s"), QStringLiteral("X")}));
        QVERIFY(!hasExplicitSerial({QStringLiteral("logcat"), QStringLiteral("-s"), QStringLiteral("Tag")}));
        QVERIFY(!hasExplicitSerial({QStringLiteral("shell"), QStringLiteral("ls"), QStringLiteral("-s")}));
        QVERIFY(!hasExplicitSerial({}));
    }
    void wirelessSerials() {
        QVERIFY(isWirelessSerial(QStringLiteral("192.168.1.5:5555")));
        QVERIFY(isWirelessSerial(QStringLiteral("adb-XYZ-abc._adb-tls-connect._tcp")));
        QVERIFY(!isWirelessSerial(QStringLiteral("R58M1234ABC")));
        QVERIFY(!isWirelessSerial(QString()));
    }
    void framingPartialThenComplete() {
        QByteArray buf = encodeServerRequest("host:version");
        QCOMPARE(buf.left(4), QByteArray("000c"));
        QByteArray partial = buf.left(7);
        QVERIFY(!takeServerFrame(partial).has_value());
        QCOMPARE(partial.size(), 7); // untouched while incomplete
        QByteArray two = buf + encodeServerRequest("x");
        const auto first = takeServerFrame(two);
        QVERIFY(first.has_value());
        QCOMPARE(*first, QByteArray("host:version"));
        const auto second = takeServerFrame(two);
        QVERIFY(second.has_value());
        QCOMPARE(*second, QByteArray("x"));
        QVERIFY(two.isEmpty());
    }
    void framingMalformed() {
        QByteArray bad = "zzzzdata";
        bool malformed = false;
        QVERIFY(!takeServerFrame(bad, &malformed).has_value());
        QVERIFY(malformed);
    }
    void quotingTricky() {
        QCOMPARE(shellQuote(QString()), QStringLiteral("''"));
        QCOMPARE(shellQuote(QStringLiteral("it's")), QStringLiteral("'it'\\''s'"));
        QCOMPARE(shellQuote(QStringLiteral("$(x) `y` \"z\"")), QStringLiteral("'$(x) `y` \"z\"'"));
        QCOMPARE(shellQuote(QStringLiteral("a\nb")), QStringLiteral("'a\nb'"));
    }
    void humanSizes() {
        QCOMPARE(parseHumanSize(QStringLiteral("512K")), qint64(512) * 1024);
        QCOMPARE(parseHumanSize(QStringLiteral("1.5G")), qint64(1.5 * 1024 * 1024 * 1024));
        QCOMPARE(parseHumanSize(QStringLiteral("0")), qint64(0));
        QCOMPARE(parseHumanSize(QStringLiteral("  87M ")), qint64(87) * 1024 * 1024);
        QCOMPARE(parseHumanSize(QStringLiteral("abc")), qint64(0));
        QCOMPARE(parseHumanSize(QString()), qint64(0));
    }
    void formatBytesBoundaries() {
        QCOMPARE(formatBytes(0), QStringLiteral("0 B"));
        QCOMPARE(formatBytes(1023), QStringLiteral("1023 B"));
        QCOMPARE(formatBytes(1024), QStringLiteral("1.00 KB"));
        QCOMPARE(formatBytes(qint64(1) << 40), QStringLiteral("1.00 TB"));
        QVERIFY(formatBytes(qint64(1) << 50).endsWith(QStringLiteral("TB"))); // clamps at TB
    }
    void cpuUsageEdgeCases() {
        QCOMPARE(cpuUsagePercent({10, 100}, {10, 100}), 0.0);   // no progress
        QCOMPARE(cpuUsagePercent({50, 100}, {10, 90}), 0.0);    // counter went backwards
        QCOMPARE(cpuUsagePercent({0, 0}, {50, 100}), 50.0);
        QCOMPARE(cpuUsagePercent({0, 0}, {100, 100}), 100.0);
    }
    void memInfoFallbackWhenNoAvailable() {
        const MemInfo m = parseMemInfo(QStringLiteral("MemTotal: 1000 kB\nMemFree: 100 kB\nBuffers: 50 kB\nCached: 150 kB\n"));
        QCOMPARE(m.availableKb, qint64(300));
        QCOMPARE(m.usedKb(), qint64(700));
    }
    void netDevSumsInterfaces() {
        const NetCounters n = parseNetDev(QStringLiteral(
            "Inter-|   Receive                                                |  Transmit\n"
            " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
            "    lo: 999 1 0 0 0 0 0 0 999 1 0 0 0 0 0 0\n"
            " wlan0: 1000 1 0 0 0 0 0 0 200 1 0 0 0 0 0 0\n"
            "rmnet0: 500 1 0 0 0 0 0 0 100 1 0 0 0 0 0 0\n"));
        QCOMPARE(n.rxBytes, qint64(1500));
        QCOMPARE(n.txBytes, qint64(300));
    }
    void topIgnoresHeaderAndKeepsSpacedNames() {
        const auto rows = parseTop(QStringLiteral(
            "Tasks: 5 total\n800%cpu 10%user\n  PID %CPU RES ARGS\n 1234 12.5 87M com.example app\n abc 1 2 bad\n"));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().pid, 1234);
        QCOMPARE(rows.first().cpuPercent, 12.5);
        QCOMPARE(rows.first().name, QStringLiteral("com.example app"));
    }
    void dfDropsNoise() {
        const auto rows = parseDf(QStringLiteral(
            "Filesystem 1K-blocks Used Available Use% Mounted on\n"
            "tmpfs 100 1 99 1% /dev\n"
            "/dev/block/dm-9 1000 400 600 40% /data\n"
            "/dev/zero 0 0 0 0% /storage/empty\n"));
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.first().mount, QStringLiteral("/data"));
        QCOMPARE(rows.first().usedPercent(), 40.0);
    }
};

QTEST_APPLESS_MAIN(TstParsersEdge)
#include "tst_parsersedge.moc"
