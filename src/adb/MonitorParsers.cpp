#include "adb/MonitorParsers.h"

#include <QRegularExpression>

#include <algorithm>

namespace polyscope::adb {

namespace {

qint64 kbValue(const QString& line) {
    // "MemTotal:        7607308 kB"
    static const QRegularExpression re(QStringLiteral("(\\d+)"));
    const auto m = re.match(line, line.indexOf(QLatin1Char(':')));
    return m.hasMatch() ? m.captured(1).toLongLong() : 0;
}

qint64 numberAfter(const QString& text, const QString& label) {
    // "App Size: 14012531200"
    const QRegularExpression re(QStringLiteral("(?m)^\\s*") + QRegularExpression::escape(label) +
                                QStringLiteral(":\\s*(\\d+)"));
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).toLongLong() : 0;
}

QList<qint64> numberList(const QString& text, const QString& label) {
    const QRegularExpression re(QStringLiteral("(?m)^\\s*") + QRegularExpression::escape(label) +
                                QStringLiteral(":\\s*\\[([^\\]]*)\\]"));
    const auto m = re.match(text);
    QList<qint64> out;
    if (!m.hasMatch()) {
        return out;
    }
    const QStringList parts = m.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString& p : parts) {
        out.append(p.trimmed().toLongLong());
    }
    return out;
}

QStringList quotedList(const QString& text, const QString& label) {
    const QRegularExpression re(QStringLiteral("(?m)^\\s*") + QRegularExpression::escape(label) +
                                QStringLiteral(":\\s*\\[([^\\]]*)\\]"));
    const auto m = re.match(text);
    QStringList out;
    if (!m.hasMatch()) {
        return out;
    }
    static const QRegularExpression item(QStringLiteral("\"([^\"]*)\""));
    auto it = item.globalMatch(m.captured(1));
    while (it.hasNext()) {
        out.append(it.next().captured(1));
    }
    return out;
}

} // namespace

MemInfo parseMemInfo(const QString& text) {
    MemInfo info;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            info.totalKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("MemFree:"))) {
            info.freeKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
            info.availableKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("Buffers:"))) {
            info.buffersKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("Cached:"))) {
            info.cachedKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("SwapTotal:"))) {
            info.swapTotalKb = kbValue(line);
        } else if (line.startsWith(QStringLiteral("SwapFree:"))) {
            info.swapFreeKb = kbValue(line);
        }
    }
    if (info.availableKb == 0) {
        info.availableKb = info.freeKb + info.buffersKb + info.cachedKb; // very old kernels
    }
    return info;
}

QList<CpuTimes> parseCpuStat(const QString& text) {
    QList<CpuTimes> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (!line.startsWith(QStringLiteral("cpu"))) {
            continue;
        }
        const QStringList f = line.simplified().split(QLatin1Char(' '));
        if (f.size() < 5) {
            continue;
        }
        // user nice system idle iowait irq softirq steal ...
        qint64 total = 0;
        for (qsizetype i = 1; i < std::min<qsizetype>(f.size(), 9); ++i) {
            total += f.at(i).toLongLong();
        }
        const qint64 idle = f.at(4).toLongLong() + (f.size() > 5 ? f.at(5).toLongLong() : 0);
        out.append({total - idle, total});
    }
    return out;
}

double cpuUsagePercent(const CpuTimes& previous, const CpuTimes& current) {
    const qint64 total = current.total - previous.total;
    const qint64 busy = current.busy - previous.busy;
    if (total <= 0 || busy < 0) {
        return 0.0;
    }
    return std::clamp(100.0 * double(busy) / double(total), 0.0, 100.0);
}

NetCounters parseNetDev(const QString& text) {
    NetCounters sum;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        const qsizetype colon = raw.indexOf(QLatin1Char(':'));
        if (colon < 0) {
            continue; // header lines have no "iface:"
        }
        const QString iface = raw.left(colon).trimmed();
        if (iface == QStringLiteral("lo")) {
            continue;
        }
        const QStringList f = raw.mid(colon + 1).simplified().split(QLatin1Char(' '));
        if (f.size() < 9) {
            continue;
        }
        sum.rxBytes += f.at(0).toLongLong();
        sum.txBytes += f.at(8).toLongLong();
    }
    return sum;
}

QList<DfEntry> parseDf(const QString& text) {
    QList<DfEntry> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QStringList f = line.simplified().split(QLatin1Char(' '));
        if (f.size() < 6 || f.at(0) == QStringLiteral("Filesystem")) {
            continue;
        }
        DfEntry e;
        e.filesystem = f.at(0);
        e.totalKb = f.at(1).toLongLong();
        e.usedKb = f.at(2).toLongLong();
        e.availableKb = f.at(3).toLongLong();
        e.mount = f.mid(5).join(QLatin1Char(' '));
        const bool interesting = e.mount == QStringLiteral("/data") || e.mount == QStringLiteral("/") ||
                                 e.mount == QStringLiteral("/system_ext") || e.mount == QStringLiteral("/product") ||
                                 e.mount == QStringLiteral("/vendor") || e.mount == QStringLiteral("/odm") ||
                                 e.mount == QStringLiteral("/metadata") || e.mount == QStringLiteral("/cache") ||
                                 e.mount.startsWith(QStringLiteral("/mnt/media_rw/")) ||
                                 e.mount.startsWith(QStringLiteral("/storage/"));
        if (interesting && e.totalKb > 0) {
            out.append(e);
        }
    }
    return out;
}

DiskStats parseDiskStats(const QString& text) {
    DiskStats s;
    // "Data-Free: 71094744K / 110800588K total = 64% free"
    static const QRegularExpression dataFree(
        QStringLiteral("Data-Free:\\s*(\\d+)K\\s*/\\s*(\\d+)K"));
    const auto m = dataFree.match(text);
    if (m.hasMatch()) {
        s.dataFreeKb = m.captured(1).toLongLong();
        s.dataTotalKb = m.captured(2).toLongLong();
    }
    s.appBytes = numberAfter(text, QStringLiteral("App Size"));
    s.appDataBytes = numberAfter(text, QStringLiteral("App Data Size"));
    s.appCacheBytes = numberAfter(text, QStringLiteral("App Cache Size"));
    s.photosBytes = numberAfter(text, QStringLiteral("Photos Size"));
    s.videosBytes = numberAfter(text, QStringLiteral("Videos Size"));
    s.audioBytes = numberAfter(text, QStringLiteral("Audio Size"));
    s.downloadsBytes = numberAfter(text, QStringLiteral("Downloads Size"));
    s.systemBytes = numberAfter(text, QStringLiteral("System Size"));
    s.otherBytes = numberAfter(text, QStringLiteral("Other Size"));

    const QStringList names = quotedList(text, QStringLiteral("Package Names"));
    const QList<qint64> app = numberList(text, QStringLiteral("App Sizes"));
    const QList<qint64> data = numberList(text, QStringLiteral("App Data Sizes"));
    const QList<qint64> cache = numberList(text, QStringLiteral("Cache Sizes"));
    for (qsizetype i = 0; i < names.size(); ++i) {
        AppStorage a;
        a.package = names.at(i);
        a.appBytes = i < app.size() ? app.at(i) : 0;
        a.dataBytes = i < data.size() ? data.at(i) : 0;
        a.cacheBytes = i < cache.size() ? cache.at(i) : 0;
        if (a.totalBytes() > 0) {
            s.apps.append(a);
        }
    }
    std::sort(s.apps.begin(), s.apps.end(),
              [](const AppStorage& a, const AppStorage& b) { return a.totalBytes() > b.totalBytes(); });
    return s;
}

qint64 parseHumanSize(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty()) {
        return 0;
    }
    const QChar unit = t.back();
    double scale = 1;
    QString number = t;
    if (unit.isLetter()) {
        number.chop(1);
        switch (unit.toUpper().unicode()) {
        case 'K': scale = 1024.0; break;
        case 'M': scale = 1024.0 * 1024.0; break;
        case 'G': scale = 1024.0 * 1024.0 * 1024.0; break;
        case 'T': scale = 1024.0 * 1024.0 * 1024.0 * 1024.0; break;
        default: break;
        }
    }
    bool ok = false;
    const double value = number.toDouble(&ok);
    return ok ? static_cast<qint64>(value * scale) : 0;
}

QList<ProcessInfo> parseTop(const QString& text) {
    QList<ProcessInfo> out;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || !trimmed.at(0).isDigit()) {
            continue; // headers start with letters / "800%cpu"
        }
        // "PID %CPU RES NAME..." -> PID must be a plain integer followed by spaces.
        const QStringList f = trimmed.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (f.size() < 4) {
            continue;
        }
        bool pidOk = false;
        bool cpuOk = false;
        ProcessInfo p;
        p.pid = f.at(0).toInt(&pidOk);
        p.cpuPercent = f.at(1).toDouble(&cpuOk);
        if (!pidOk || !cpuOk) {
            continue;
        }
        p.residentBytes = parseHumanSize(f.at(2));
        p.name = f.mid(3).join(QLatin1Char(' '));
        out.append(p);
    }
    return out;
}

PackageDetails parsePackageDetails(const QString& text) {
    PackageDetails d;
    auto field = [&text](const QString& key) {
        const QRegularExpression re(QStringLiteral("(?m)^\\s*") + QRegularExpression::escape(key) +
                                    QStringLiteral("=(\\S+(?: \\d{2}:\\d{2}:\\d{2})?)"));
        const auto m = re.match(text);
        return m.hasMatch() ? m.captured(1) : QString();
    };
    d.versionName = field(QStringLiteral("versionName"));
    d.versionCode = field(QStringLiteral("versionCode"));
    d.firstInstall = field(QStringLiteral("firstInstallTime"));
    d.lastUpdate = field(QStringLiteral("lastUpdateTime"));
    d.installer = field(QStringLiteral("installerPackageName"));
    // "versionCode=744404833 minSdk=32 targetSdk=36" shares one line.
    static const QRegularExpression sdk(QStringLiteral("minSdk=(\\d+)\\s+targetSdk=(\\d+)"));
    const auto m = sdk.match(text);
    if (m.hasMatch()) {
        d.minSdk = m.captured(1);
        d.targetSdk = m.captured(2);
    }
    static const QRegularExpression flags(QStringLiteral("(?m)^\\s*flags=\\[([^\\]]*)\\]"));
    d.isSystem = flags.match(text).captured(1).split(QLatin1Char(' ')).contains(QStringLiteral("SYSTEM"));
    return d;
}

QString formatBytes(qint64 bytes) {
    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    const int decimals = (unit == 0 || value >= 100) ? 0 : (value >= 10 ? 1 : 2);
    return QString::number(value, 'f', decimals) + QLatin1Char(' ') + QString::fromLatin1(units[unit]);
}

} // namespace polyscope::adb
