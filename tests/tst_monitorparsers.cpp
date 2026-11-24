#include "adb/MonitorParsers.h"

#include <QtTest>

using namespace polyscope::adb;

class TstMonitorParsers : public QObject {
    Q_OBJECT
private slots:
    void memInfo() {
        const auto m = parseMemInfo(QStringLiteral("MemTotal:        7607308 kB\nMemFree:          318996 kB\n"
                                                   "MemAvailable:    2806452 kB\nBuffers:            6516 kB\n"
                                                   "Cached:          3070708 kB\nSwapTotal: 6291452 kB\nSwapFree: 4443688 kB\n"));
        QVERIFY(m.isValid());
        QCOMPARE(m.totalKb, 7607308);
        QCOMPARE(m.usedKb(), 7607308 - 2806452);
        QCOMPARE(m.swapFreeKb, 4443688);
        QVERIFY(!parseMemInfo(QString()).isValid());
    }

    void cpuStatAndUsage() {
        const auto a = parseCpuStat(QStringLiteral("cpu  100 0 100 800 0 0 0 0 0 0\ncpu0 50 0 50 400 0 0 0 0\nintr 1\n"));
        QCOMPARE(a.size(), 2);
        QCOMPARE(a.at(0).total, 1000);
        QCOMPARE(a.at(0).busy, 200);
        const auto b = parseCpuStat(QStringLiteral("cpu  200 0 200 900 0 0 0 0 0 0\ncpu0 100 0 100 450 0 0 0 0\n"));
        QVERIFY(qAbs(cpuUsagePercent(a.at(0), b.at(0)) - 66.667) < 0.01); // +200 busy of +300 total
        QCOMPARE(cpuUsagePercent(a.at(0), a.at(0)), 0.0); // no progress
        QCOMPARE(cpuUsagePercent(b.at(0), a.at(0)), 0.0); // counter went backwards (reboot)
    }

    void netDevSkipsLoopback() {
        const auto n = parseNetDev(QStringLiteral(
            "Inter-|   Receive                                                |  Transmit\n"
            " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed\n"
            "    lo: 3330247   24039    0    0    0     0          0         0  3330247   24039    0    0    0     0       0          0\n"
            " wlan0: 1000   10    0    0    0     0          0         0  2000   20    0    0    0     0       0          0\n"
            "rmnet0:  500    5    0    0    0     0          0         0   700    7    0    0    0     0       0          0\n"));
        QCOMPARE(n.rxBytes, 1500);
        QCOMPARE(n.txBytes, 2700);
    }

    void dfKeepsRealMounts() {
        const auto list = parseDf(QStringLiteral(
            "Filesystem        1K-blocks     Used Available Use% Mounted on\n"
            "/dev/block/dm-28    1103260  1103260         0 100% /\n"
            "tmpfs               3781636        0   3781636   0% /mnt\n"
            "/dev/block/loop0        788      756        16  98% /apex/com.android.tzdata@1\n"
            "/dev/block/dm-73  110800588 39181460  71094840  36% /data\n"));
        QCOMPARE(list.size(), 2);
        QCOMPARE(list.at(1).mount, QStringLiteral("/data"));
        QVERIFY(qAbs(list.at(1).usedPercent() - 35.36) < 0.1);
    }

    void diskStats() {
        const auto s = parseDiskStats(QStringLiteral(
            "Data-Free: 71094744K / 110800588K total = 64% free\n"
            "App Size: 1000\nApp Data Size: 2000\nApp Cache Size: 300\nPhotos Size: 40\nVideos Size: 50\n"
            "Audio Size: 6\nDownloads Size: 0\nSystem Size: 7000\nOther Size: 8\n"
            "Package Names: [\"a.small\",\"b.big\",\"c.empty\"]\n"
            "App Sizes: [10,500,0]\nApp Data Sizes: [5,400,0]\nCache Sizes: [1,100]\n"));
        QVERIFY(s.isValid());
        QCOMPARE(s.dataTotalKb, 110800588);
        QCOMPARE(s.appDataBytes, 2000);
        QCOMPARE(s.systemBytes, 7000);
        QCOMPARE(s.apps.size(), 2); // zero-size app dropped
        QCOMPARE(s.apps.at(0).package, QStringLiteral("b.big"));
        QCOMPARE(s.apps.at(0).totalBytes(), 1000);
        QCOMPARE(s.apps.at(1).cacheBytes, 1); // cache list shorter than names: no crash
    }

    void topRowsAndSizes() {
        const auto p = parseTop(QStringLiteral(
            "Tasks: 757 total,   1 running\n  Mem:  7607308K total\n800%cpu  16%user\n"
            "  PID %CPU  RES[ARGS]\n 1303 39.2 4.4M android.system.suspend-service\n"
            " 3446 10.7 687M system_server\n26430  3.5    0 [kworker/u16:19-events_unbound]\n"));
        QCOMPARE(p.size(), 3);
        QCOMPARE(p.at(0).pid, 1303);
        QCOMPARE(p.at(1).name, QStringLiteral("system_server"));
        QCOMPARE(p.at(1).residentBytes, qint64(687) * 1024 * 1024);
        QCOMPARE(p.at(2).residentBytes, 0);
        QCOMPARE(parseHumanSize(QStringLiteral("1.5G")), qint64(1.5 * 1024 * 1024 * 1024));
        QCOMPARE(parseHumanSize(QStringLiteral("junk")), 0);
    }

    void packageDetails() {
        const auto d = parsePackageDetails(QStringLiteral(
            "    flags=0x0\n    versionCode=744404833 minSdk=32 targetSdk=36\n    versionName=142.0.7444.48\n"
            "    flags=[ SYSTEM HAS_CODE ALLOW_BACKUP ]\n    firstInstallTime=2024-01-02 03:04:05\n"
            "    lastUpdateTime=2025-11-05 17:59:14\n    installerPackageName=com.android.vending\n"));
        QVERIFY(d.isValid());
        QCOMPARE(d.versionName, QStringLiteral("142.0.7444.48"));
        QCOMPARE(d.versionCode, QStringLiteral("744404833"));
        QCOMPARE(d.minSdk, QStringLiteral("32"));
        QCOMPARE(d.targetSdk, QStringLiteral("36"));
        QCOMPARE(d.firstInstall, QStringLiteral("2024-01-02 03:04:05"));
        QCOMPARE(d.lastUpdate, QStringLiteral("2025-11-05 17:59:14"));
        QCOMPARE(d.installer, QStringLiteral("com.android.vending"));
        QVERIFY(d.isSystem);
        QVERIFY(!parsePackageDetails(QStringLiteral("flags=[ HAS_CODE ]\nversionName=1\n")).isSystem);
        QVERIFY(!parsePackageDetails(QString()).isValid());
    }
    void formatBytesUnits() {
        QCOMPARE(formatBytes(0), QStringLiteral("0 B"));
        QCOMPARE(formatBytes(1536), QStringLiteral("1.50 KB"));
        QCOMPARE(formatBytes(qint64(14012531200)), QStringLiteral("13.1 GB"));
    }
};

QTEST_APPLESS_MAIN(TstMonitorParsers)
#include "tst_monitorparsers.moc"
