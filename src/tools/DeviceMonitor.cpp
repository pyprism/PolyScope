#include "tools/DeviceMonitor.h"

#include "adb/AdbClient.h"
#include "adb/AdbParsers.h"
#include "core/AppContext.h"

#include <QRegularExpression>

#include <algorithm>

namespace polyscope {

namespace {

constexpr int kIntervalMs = 2000;
constexpr int kProcessEveryTicks = 3; // top is the slow part of a tick
constexpr int kTopCount = 6;

const QString kFastCommand = QStringLiteral(
    "echo @@MEM; grep -E '^(MemTotal|MemFree|MemAvailable|Buffers|Cached|SwapTotal|SwapFree):' /proc/meminfo; "
    "echo @@CPU; grep '^cpu' /proc/stat; "
    "echo @@NET; cat /proc/net/dev; "
    "echo @@BAT; dumpsys battery; "
    "echo @@UP; cat /proc/uptime");

const QString kProcessCommand = QStringLiteral(
    "echo @@TOPCPU; top -b -n 1 -m %1 -s 2 -o PID,%CPU,RES,ARGS; "
    "echo @@TOPMEM; top -b -n 1 -m %1 -s 3 -o PID,%CPU,RES,ARGS").arg(kTopCount);

// Splits "@@NAME\n...body..." output into name -> body.
QHash<QString, QString> sections(const QString& text) {
    QHash<QString, QString> out;
    static const QRegularExpression marker(QStringLiteral("(?m)^@@(\\w+)\\s*$"));
    auto it = marker.globalMatch(text);
    QString name;
    qsizetype bodyStart = 0;
    while (it.hasNext()) {
        const auto m = it.next();
        if (!name.isEmpty()) {
            out.insert(name, text.mid(bodyStart, m.capturedStart() - bodyStart));
        }
        name = m.captured(1);
        bodyStart = m.capturedEnd();
    }
    if (!name.isEmpty()) {
        out.insert(name, text.mid(bodyStart));
    }
    return out;
}

} // namespace

DeviceMonitor::DeviceMonitor(QObject* parent) : QObject(parent) {
    m_timer.setInterval(kIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &DeviceMonitor::tick);
}

void DeviceMonitor::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    m_serial = serial;
    reset();
    emit serialChanged();
    if (m_active && !m_serial.isEmpty()) {
        tick();
    }
}

void DeviceMonitor::setActive(bool active) {
    if (active == m_active) {
        return;
    }
    m_active = active;
    if (m_active) {
        m_timer.start();
        if (!m_serial.isEmpty()) {
            tick();
        }
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

double DeviceMonitor::memUsedPercent() const {
    return m_mem.isValid() ? 100.0 * double(m_mem.usedKb()) / double(m_mem.totalKb) : 0.0;
}

void DeviceMonitor::reset() {
    ++m_generation;
    m_fastInFlight = false;
    m_procInFlight = false;
    m_storageBusy = false;
    m_mem = {};
    m_battery = {};
    m_lastCpu.clear();
    m_haveNet = false;
    m_cpu = m_rxRate = m_txRate = 0;
    m_uptime = 0;
    m_cores.clear();
    m_cpuHistory.clear();
    m_memHistory.clear();
    m_rxHistory.clear();
    m_txHistory.clear();
    m_batteryHistory.clear();
    m_batteryTempHistory.clear();
    m_batteryVoltageHistory.clear();
    m_topCpu.clear();
    m_topMem.clear();
    m_storage.clear();
    m_ticks = 0;
    setError(QString());
    emit sampled();
    emit processesChanged();
    emit storageChanged();
}

void DeviceMonitor::setError(const QString& error) {
    if (error == m_error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void DeviceMonitor::push(QVariantList& list, double value) {
    list.append(value);
    while (list.size() > kHistoryLength) {
        list.removeFirst();
    }
}

void DeviceMonitor::tick() {
    if (m_serial.isEmpty()) {
        return;
    }
    pollFast();
    if (m_ticks % kProcessEveryTicks == 0) {
        pollProcesses();
    }
    ++m_ticks;
}

void DeviceMonitor::pollFast() {
    auto* client = AppContext::instance()->adb();
    if (m_fastInFlight || !client || !client->isAvailable()) {
        return;
    }
    m_fastInFlight = true;
    const int generation = m_generation;
    client->shell(this, m_serial, kFastCommand, 10000, [this, generation](const adb::ProcessResult& r) {
        if (generation != m_generation) {
            return;
        }
        m_fastInFlight = false;
        if (!r.ok()) {
            setError(r.errorText());
            return;
        }
        setError(QString());
        applyFast(r.text());
    });
}

void DeviceMonitor::applyFast(const QString& text) {
    const auto parts = sections(text);

    m_mem = adb::parseMemInfo(parts.value(QStringLiteral("MEM")));
    m_battery = adb::parseBattery(parts.value(QStringLiteral("BAT")));
    m_uptime = static_cast<qint64>(parts.value(QStringLiteral("UP")).trimmed().section(QLatin1Char(' '), 0, 0).toDouble());

    const auto cpu = adb::parseCpuStat(parts.value(QStringLiteral("CPU")));
    if (!cpu.isEmpty() && m_lastCpu.size() == cpu.size()) {
        m_cpu = adb::cpuUsagePercent(m_lastCpu.first(), cpu.first());
        m_cores.clear();
        for (qsizetype i = 1; i < cpu.size(); ++i) {
            m_cores.append(adb::cpuUsagePercent(m_lastCpu.at(i), cpu.at(i)));
        }
        push(m_cpuHistory, m_cpu);
    }
    m_lastCpu = cpu;

    const auto net = adb::parseNetDev(parts.value(QStringLiteral("NET")));
    if (m_haveNet && m_netClock.isValid()) {
        const double seconds = std::max(0.001, double(m_netClock.elapsed()) / 1000.0);
        // Counters reset on interface restarts; never report a negative rate.
        m_rxRate = std::max<qint64>(0, net.rxBytes - m_lastNet.rxBytes) / seconds;
        m_txRate = std::max<qint64>(0, net.txBytes - m_lastNet.txBytes) / seconds;
        push(m_rxHistory, m_rxRate);
        push(m_txHistory, m_txRate);
    }
    m_lastNet = net;
    m_haveNet = true;
    m_netClock.restart();

    if (m_mem.isValid()) {
        push(m_memHistory, memUsedPercent());
    }
    if (m_battery.isValid()) {
        push(m_batteryHistory, m_battery.level);
        if (m_battery.temperatureC >= 0) {
            push(m_batteryTempHistory, m_battery.temperatureC);
        }
        if (m_battery.voltageMv > 0) {
            push(m_batteryVoltageHistory, m_battery.voltageMv / 1000.0);
        }
    }
    emit sampled();
}

QVariantList DeviceMonitor::processList(const QList<adb::ProcessInfo>& rows, bool byMemory) {
    QVariantList out;
    for (const auto& p : rows) {
        // Skip our own probe and kernel threads, which would otherwise crowd the lists.
        if (p.name.startsWith(QStringLiteral("top ")) || p.name.startsWith(QLatin1Char('['))) {
            continue;
        }
        out.append(QVariantMap{{QStringLiteral("pid"), p.pid},
                               {QStringLiteral("name"), p.name},
                               {QStringLiteral("cpu"), p.cpuPercent},
                               {QStringLiteral("mem"), p.residentBytes},
                               {QStringLiteral("value"), byMemory ? double(p.residentBytes) : p.cpuPercent}});
    }
    return out;
}

void DeviceMonitor::pollProcesses() {
    auto* client = AppContext::instance()->adb();
    if (m_procInFlight || !client || !client->isAvailable()) {
        return;
    }
    m_procInFlight = true;
    const int generation = m_generation;
    client->shell(this, m_serial, kProcessCommand, 15000, [this, generation](const adb::ProcessResult& r) {
        if (generation != m_generation) {
            return;
        }
        m_procInFlight = false;
        if (!r.ok()) {
            return;
        }
        const auto parts = sections(r.text());
        m_topCpu = processList(adb::parseTop(parts.value(QStringLiteral("TOPCPU"))), false);
        m_topMem = processList(adb::parseTop(parts.value(QStringLiteral("TOPMEM"))), true);
        emit processesChanged();
    });
}

void DeviceMonitor::refreshStorage() {
    auto* client = AppContext::instance()->adb();
    if (m_serial.isEmpty() || m_storageBusy || !client || !client->isAvailable()) {
        return;
    }
    m_storageBusy = true;
    emit storageChanged();
    const int generation = m_generation;
    client->shell(this, m_serial, QStringLiteral("df -k; echo @@DS; dumpsys diskstats"), 90000,
                  [this, generation](const adb::ProcessResult& r) {
        if (generation != m_generation) {
            return;
        }
        m_storageBusy = false;
        if (!r.ok()) {
            m_storage = {{QStringLiteral("error"), r.errorText()}};
            emit storageChanged();
            return;
        }
        const QString text = r.text();
        const qsizetype split = text.indexOf(QStringLiteral("@@DS"));
        const auto partitions = adb::parseDf(split < 0 ? text : text.left(split));
        const auto ds = adb::parseDiskStats(split < 0 ? QString() : text.mid(split));

        QVariantList partitionList;
        for (const auto& p : partitions) {
            partitionList.append(QVariantMap{{QStringLiteral("mount"), p.mount},
                                             {QStringLiteral("total"), p.totalKb * 1024},
                                             {QStringLiteral("used"), p.usedKb * 1024},
                                             {QStringLiteral("percent"), p.usedPercent()}});
        }
        QVariantList categories;
        auto add = [&categories](const QString& label, qint64 bytes) {
            if (bytes > 0) {
                categories.append(QVariantMap{{QStringLiteral("label"), label}, {QStringLiteral("bytes"), bytes}});
            }
        };
        add(tr("Apps"), ds.appBytes);
        add(tr("App data"), ds.appDataBytes);
        add(tr("Cache"), ds.appCacheBytes);
        add(tr("Photos"), ds.photosBytes);
        add(tr("Videos"), ds.videosBytes);
        add(tr("Audio"), ds.audioBytes);
        add(tr("Downloads"), ds.downloadsBytes);
        add(tr("Other files"), ds.otherBytes);

        const qint64 total = ds.dataTotalKb * 1024;
        const qint64 free = ds.dataFreeKb * 1024;
        qint64 accounted = 0;
        for (const QVariant& c : std::as_const(categories)) {
            accounted += c.toMap().value(QStringLiteral("bytes")).toLongLong();
        }
        // diskstats does not attribute everything; show the rest honestly instead of hiding it.
        add(tr("Unattributed"), std::max<qint64>(0, total - free - accounted));

        QVariantList apps;
        const qsizetype limit = std::min<qsizetype>(ds.apps.size(), 60);
        for (qsizetype i = 0; i < limit; ++i) {
            const auto& a = ds.apps.at(i);
            apps.append(QVariantMap{{QStringLiteral("package"), a.package},
                                    {QStringLiteral("total"), a.totalBytes()},
                                    {QStringLiteral("app"), a.appBytes},
                                    {QStringLiteral("data"), a.dataBytes},
                                    {QStringLiteral("cache"), a.cacheBytes}});
        }
        m_storage = {{QStringLiteral("total"), total},
                     {QStringLiteral("free"), free},
                     {QStringLiteral("used"), total - free},
                     {QStringLiteral("categories"), categories},
                     {QStringLiteral("apps"), apps},
                     {QStringLiteral("partitions"), partitionList},
                     {QStringLiteral("loaded"), ds.isValid()}};
        emit storageChanged();
    });
}

namespace {

bool validPackage(const QString& package) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z][A-Za-z0-9_]*(\\.[A-Za-z0-9_]+)+$"));
    return re.match(package).hasMatch();
}

} // namespace

void DeviceMonitor::runDeviceAction(const QString& action, const QString& command) {
    auto* client = AppContext::instance()->adb();
    if (m_serial.isEmpty() || !client || !client->isAvailable()) {
        return;
    }
    const QString serial = m_serial;
    client->shell(this, serial, command, 30000, [this, serial, action](const adb::ProcessResult& r) {
        const QString out = r.text().trimmed();
        // `am`/`pm` report failures on stdout and may still exit 0.
        const bool ok = r.ok() && !adb::shellOutputIndicatesFailure(out);
        const QString message = out.isEmpty() ? (ok ? tr("Done") : r.errorText()) : out;
        emit actionFinished(action, ok, serial == m_serial ? message : message + tr(" (on %1)").arg(serial));
        if (serial == m_serial && !m_procInFlight &&
            (action == QStringLiteral("stop-process") || action == QStringLiteral("force-stop"))) {
            pollProcesses();
        }
    });
}

void DeviceMonitor::stopProcess(int pid, const QString& name) {
    if (pid <= 1) {
        return;
    }
    // "com.example.app:service" -> package "com.example.app"
    const QString package = name.section(QLatin1Char(':'), 0, 0);
    if (validPackage(package)) {
        runDeviceAction(QStringLiteral("stop-process"), QStringLiteral("am force-stop ") + package);
    } else {
        runDeviceAction(QStringLiteral("stop-process"), QStringLiteral("kill -9 ") + QString::number(pid));
    }
}

void DeviceMonitor::loadPackageDetails(const QString& package) {
    auto* client = AppContext::instance()->adb();
    if (m_serial.isEmpty() || !validPackage(package) || !client || !client->isAvailable()) {
        return;
    }
    const int generation = m_generation;
    client->shell(this, m_serial, QStringLiteral("dumpsys package ") + package, 30000,
                  [this, generation, package](const adb::ProcessResult& r) {
        if (generation != m_generation) {
            return;
        }
        const auto d = adb::parsePackageDetails(r.text());
        emit packageDetailsReady(package, QVariantMap{{QStringLiteral("found"), d.isValid()},
                                                      {QStringLiteral("versionName"), d.versionName},
                                                      {QStringLiteral("versionCode"), d.versionCode},
                                                      {QStringLiteral("minSdk"), d.minSdk},
                                                      {QStringLiteral("targetSdk"), d.targetSdk},
                                                      {QStringLiteral("firstInstall"), d.firstInstall},
                                                      {QStringLiteral("lastUpdate"), d.lastUpdate},
                                                      {QStringLiteral("installer"), d.installer},
                                                      {QStringLiteral("system"), d.isSystem}});
    });
}

void DeviceMonitor::packageAction(const QString& package, const QString& action) {
    if (!validPackage(package)) {
        return;
    }
    if (action == QStringLiteral("force-stop")) {
        runDeviceAction(action, QStringLiteral("am force-stop ") + package);
    } else if (action == QStringLiteral("open-settings")) {
        runDeviceAction(action, QStringLiteral("am start -a android.settings.APPLICATION_DETAILS_SETTINGS -d package:") + package);
    } else if (action == QStringLiteral("uninstall")) {
        runDeviceAction(action, QStringLiteral("pm uninstall ") + package);
    }
}

QString DeviceMonitor::formatBytes(double bytes) {
    return adb::formatBytes(static_cast<qint64>(bytes));
}

} // namespace polyscope
