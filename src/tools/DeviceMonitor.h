#pragma once

#include "adb/AdbParsers.h"
#include "adb/MonitorParsers.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Live health data for one device: CPU, memory, network throughput, battery and top
// processes (polled while `active`), plus an on-demand storage breakdown.
// One `adb shell` round-trip per tick keeps the wireless link quiet.
class DeviceMonitor : public QObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

    Q_PROPERTY(double cpuPercent READ cpuPercent NOTIFY sampled)
    Q_PROPERTY(QVariantList coreUsage READ coreUsage NOTIFY sampled)
    Q_PROPERTY(double memUsedPercent READ memUsedPercent NOTIFY sampled)
    Q_PROPERTY(qint64 memUsedKb READ memUsedKb NOTIFY sampled)
    Q_PROPERTY(qint64 memTotalKb READ memTotalKb NOTIFY sampled)
    Q_PROPERTY(qint64 swapUsedKb READ swapUsedKb NOTIFY sampled)
    Q_PROPERTY(qint64 swapTotalKb READ swapTotalKb NOTIFY sampled)
    Q_PROPERTY(double rxRate READ rxRate NOTIFY sampled)
    Q_PROPERTY(double txRate READ txRate NOTIFY sampled)
    Q_PROPERTY(int batteryLevel READ batteryLevel NOTIFY sampled)
    Q_PROPERTY(double batteryTempC READ batteryTempC NOTIFY sampled)
    Q_PROPERTY(int batteryVoltageMv READ batteryVoltageMv NOTIFY sampled)
    Q_PROPERTY(QString batteryStatus READ batteryStatus NOTIFY sampled)
    Q_PROPERTY(QString power READ power NOTIFY sampled)
    Q_PROPERTY(qint64 uptimeSeconds READ uptimeSeconds NOTIFY sampled)

    Q_PROPERTY(QVariantList cpuHistory READ cpuHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList memHistory READ memHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList rxHistory READ rxHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList txHistory READ txHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList batteryHistory READ batteryHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList batteryTempHistory READ batteryTempHistory NOTIFY sampled)
    Q_PROPERTY(QVariantList batteryVoltageHistory READ batteryVoltageHistory NOTIFY sampled)

    Q_PROPERTY(QVariantList topCpu READ topCpu NOTIFY processesChanged)
    Q_PROPERTY(QVariantList topMem READ topMem NOTIFY processesChanged)

    Q_PROPERTY(QVariantMap storage READ storage NOTIFY storageChanged)
    Q_PROPERTY(bool storageBusy READ storageBusy NOTIFY storageChanged)

public:
    static constexpr int kHistoryLength = 60;

    explicit DeviceMonitor(QObject* parent = nullptr);

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    bool active() const { return m_active; }
    void setActive(bool active);
    QString error() const { return m_error; }

    double cpuPercent() const { return m_cpu; }
    QVariantList coreUsage() const { return m_cores; }
    double memUsedPercent() const;
    qint64 memUsedKb() const { return m_mem.usedKb(); }
    qint64 memTotalKb() const { return m_mem.totalKb; }
    qint64 swapUsedKb() const { return m_mem.swapTotalKb - m_mem.swapFreeKb; }
    qint64 swapTotalKb() const { return m_mem.swapTotalKb; }
    double rxRate() const { return m_rxRate; }
    double txRate() const { return m_txRate; }
    int batteryLevel() const { return m_battery.level; }
    double batteryTempC() const { return m_battery.temperatureC; }
    int batteryVoltageMv() const { return m_battery.voltageMv; }
    QString batteryStatus() const { return m_battery.status; }
    QString power() const { return m_battery.power; }
    qint64 uptimeSeconds() const { return m_uptime; }

    QVariantList cpuHistory() const { return m_cpuHistory; }
    QVariantList memHistory() const { return m_memHistory; }
    QVariantList rxHistory() const { return m_rxHistory; }
    QVariantList txHistory() const { return m_txHistory; }
    QVariantList batteryHistory() const { return m_batteryHistory; }
    QVariantList batteryTempHistory() const { return m_batteryTempHistory; }
    QVariantList batteryVoltageHistory() const { return m_batteryVoltageHistory; }
    QVariantList topCpu() const { return m_topCpu; }
    QVariantList topMem() const { return m_topMem; }
    QVariantMap storage() const { return m_storage; }
    bool storageBusy() const { return m_storageBusy; }

    Q_INVOKABLE void refreshStorage();
    // Stops a process from the dashboard list. App processes are force-stopped through the activity
    // manager (plain `kill` is refused for other users' uids); anything else gets `kill -9 <pid>`.
    Q_INVOKABLE void stopProcess(int pid, const QString& name);
    // Version, install dates and installer for one package; answered through packageDetailsReady.
    Q_INVOKABLE void loadPackageDetails(const QString& package);
    // action: "force-stop" | "open-settings" | "uninstall"
    Q_INVOKABLE void packageAction(const QString& package, const QString& action);
    Q_INVOKABLE static QString formatBytes(double bytes);

signals:
    void serialChanged();
    void activeChanged();
    void errorChanged();
    void sampled();
    void processesChanged();
    void storageChanged();
    void packageDetailsReady(const QString& package, const QVariantMap& details);
    void actionFinished(const QString& action, bool ok, const QString& message);

private:
    void tick();
    void pollFast();
    void pollProcesses();
    void applyFast(const QString& text);
    void reset();
    void runDeviceAction(const QString& action, const QString& command);
    void setError(const QString& error);
    static void push(QVariantList& list, double value);
    static QVariantList processList(const QList<adb::ProcessInfo>& rows, bool byMemory);

    QString m_serial;
    bool m_active = false;
    QString m_error;
    QTimer m_timer;
    int m_ticks = 0;
    int m_generation = 0; // discards replies that belong to a previous device
    bool m_fastInFlight = false;
    bool m_procInFlight = false;

    adb::MemInfo m_mem;
    adb::BatteryInfo m_battery;
    QList<adb::CpuTimes> m_lastCpu;
    adb::NetCounters m_lastNet;
    bool m_haveNet = false;
    QElapsedTimer m_netClock;
    double m_cpu = 0;
    QVariantList m_cores;
    double m_rxRate = 0;
    double m_txRate = 0;
    qint64 m_uptime = 0;
    QVariantList m_cpuHistory;
    QVariantList m_memHistory;
    QVariantList m_rxHistory;
    QVariantList m_txHistory;
    QVariantList m_batteryHistory;
    QVariantList m_batteryTempHistory;
    QVariantList m_batteryVoltageHistory;
    QVariantList m_topCpu;
    QVariantList m_topMem;
    QVariantMap m_storage;
    bool m_storageBusy = false;
};

} // namespace polyscope
