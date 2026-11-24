#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Parsers for the device-health data behind the dashboard (memory, CPU, network, storage,
// processes). Pure functions over canned text so they can be unit-tested without a device.
namespace polyscope::adb {

struct MemInfo {
    qint64 totalKb = 0;
    qint64 freeKb = 0;
    qint64 availableKb = 0;
    qint64 buffersKb = 0;
    qint64 cachedKb = 0;
    qint64 swapTotalKb = 0;
    qint64 swapFreeKb = 0;

    bool isValid() const { return totalKb > 0; }
    qint64 usedKb() const { return totalKb - availableKb; } // "used" as Android reports pressure
};
MemInfo parseMemInfo(const QString& procMeminfo);

struct CpuTimes {
    qint64 busy = 0;
    qint64 total = 0;
};
// First entry is the aggregate "cpu" line, followed by cpu0..cpuN.
QList<CpuTimes> parseCpuStat(const QString& procStat);
// Utilisation 0..100 between two samples of the same counter; 0 when there was no progress.
double cpuUsagePercent(const CpuTimes& previous, const CpuTimes& current);

struct NetCounters {
    qint64 rxBytes = 0;
    qint64 txBytes = 0;
};
// Sum over all interfaces except loopback.
NetCounters parseNetDev(const QString& procNetDev);

struct DfEntry {
    QString filesystem;
    QString mount;
    qint64 totalKb = 0;
    qint64 usedKb = 0;
    qint64 availableKb = 0;

    double usedPercent() const { return totalKb > 0 ? 100.0 * double(usedKb) / double(totalKb) : 0.0; }
};
// Keeps the mounts a user cares about (/data, /sdcard-like, system partitions) and drops
// tmpfs / apex / loop noise.
QList<DfEntry> parseDf(const QString& dfKOutput);

struct AppStorage {
    QString package;
    qint64 appBytes = 0;
    qint64 dataBytes = 0;
    qint64 cacheBytes = 0;

    qint64 totalBytes() const { return appBytes + dataBytes + cacheBytes; }
};

struct DiskStats {
    qint64 dataTotalKb = 0;
    qint64 dataFreeKb = 0;
    qint64 appBytes = 0;
    qint64 appDataBytes = 0;
    qint64 appCacheBytes = 0;
    qint64 photosBytes = 0;
    qint64 videosBytes = 0;
    qint64 audioBytes = 0;
    qint64 downloadsBytes = 0;
    qint64 systemBytes = 0;
    qint64 otherBytes = 0;
    QList<AppStorage> apps; // sorted by total size, largest first

    bool isValid() const { return dataTotalKb > 0; }
};
DiskStats parseDiskStats(const QString& dumpsysDiskstats);

struct ProcessInfo {
    int pid = 0;
    double cpuPercent = 0;
    qint64 residentBytes = 0;
    QString name;
};
// `top -b -n 1 -o PID,%CPU,RES,ARGS` rows. Header lines are skipped.
QList<ProcessInfo> parseTop(const QString& topOutput);
// "87M", "3.6M", "512K", "1.2G", "0" -> bytes.
qint64 parseHumanSize(const QString& text);

struct PackageDetails {
    QString versionName;
    QString versionCode;
    QString minSdk;
    QString targetSdk;
    QString firstInstall;
    QString lastUpdate;
    QString installer;
    bool isSystem = false;

    bool isValid() const { return !versionName.isEmpty() || !versionCode.isEmpty(); }
};
// Relevant lines of `dumpsys package <name>`; the first occurrence of each field wins.
PackageDetails parsePackageDetails(const QString& dumpsysPackage);

// Human readable size: 1536 -> "1.5 KB". Binary units.
QString formatBytes(qint64 bytes);

} // namespace polyscope::adb
