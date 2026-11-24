#include <QJSEngine>
#include "models/DeviceModel.h"

#include "adb/AdbClient.h"
#include "adb/AdbTracker.h"
#include "core/AppContext.h"
#include "core/Logging.h"

namespace polyscope {

DeviceModel::DeviceModel(adb::AdbClient* client, adb::AdbTracker* tracker, QObject* parent)
    : QAbstractListModel(parent), m_client(client), m_tracker(tracker) {
    connect(tracker, &adb::AdbTracker::devicesUpdated, this, &DeviceModel::applyListing);
    connect(tracker, &adb::AdbTracker::connectedChanged, this, &DeviceModel::trackerConnectedChanged);
    m_batteryTimer.setInterval(30000);
    connect(&m_batteryTimer, &QTimer::timeout, this, &DeviceModel::refreshAllBatteries);
    m_batteryTimer.start();
}

DeviceModel* DeviceModel::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->devices();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

int DeviceModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_items.size());
}

QHash<int, QByteArray> DeviceModel::roleNames() const {
    return {
        {SerialRole, "serial"},
        {StateRole, "state"},
        {NameRole, "name"},
        {ModelRole, "model"},
        {ProductRole, "product"},
        {TransportRole, "transport"},
        {OnlineRole, "online"},
        {WirelessRole, "wireless"},
        {ManufacturerRole, "manufacturer"},
        {AndroidVersionRole, "androidVersion"},
        {SdkRole, "sdk"},
        {BatteryLevelRole, "batteryLevel"},
        {BatteryStatusRole, "batteryStatus"},
        {PowerRole, "power"},
        {DetailsLoadedRole, "detailsLoaded"},
    };
}

QVariant DeviceModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    const Item& item = m_items.at(index.row());
    switch (role) {
    case SerialRole:
        return item.entry.serial;
    case StateRole:
        return item.entry.state;
    case NameRole:
        return item.entry.model.isEmpty() ? item.entry.serial : item.entry.model;
    case ModelRole:
        return item.entry.model;
    case ProductRole:
        return item.entry.product;
    case TransportRole:
        return adb::isWirelessSerial(item.entry.serial) ? QStringLiteral("wifi") : QStringLiteral("usb");
    case OnlineRole:
        return item.entry.isOnline();
    case WirelessRole:
        return adb::isWirelessSerial(item.entry.serial);
    case ManufacturerRole:
        return item.manufacturer;
    case AndroidVersionRole:
        return item.androidVersion;
    case SdkRole:
        return item.sdk;
    case BatteryLevelRole:
        return item.battery.level;
    case BatteryStatusRole:
        return item.battery.status;
    case PowerRole:
        return item.battery.power;
    case DetailsLoadedRole:
        return item.detailsLoaded;
    default:
        return {};
    }
}

int DeviceModel::onlineCount() const {
    int n = 0;
    for (const Item& item : m_items) {
        n += item.entry.isOnline() ? 1 : 0;
    }
    return n;
}

bool DeviceModel::trackerConnected() const {
    return m_tracker->connected();
}

int DeviceModel::indexOfSerial(const QString& serial) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).entry.serial == serial) {
            return i;
        }
    }
    return -1;
}

QVariantMap DeviceModel::get(const QString& serial) const {
    QVariantMap map;
    const int row = indexOfSerial(serial);
    if (row < 0) {
        return map;
    }
    const auto names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        map.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    }
    return map;
}

QStringList DeviceModel::onlineSerials() const {
    QStringList list;
    for (const Item& item : m_items) {
        if (item.entry.isOnline()) {
            list << item.entry.serial;
        }
    }
    return list;
}

void DeviceModel::applyListing(const QString& listing) {
    const QList<adb::DeviceEntry> entries = adb::parseDevices(listing);

    // Remove vanished devices (back to front keeps row numbers valid).
    for (int row = static_cast<int>(m_items.size()) - 1; row >= 0; --row) {
        const QString serial = m_items.at(row).entry.serial;
        const bool present = std::any_of(entries.cbegin(), entries.cend(),
                                         [&serial](const adb::DeviceEntry& e) { return e.serial == serial; });
        if (!present) {
            beginRemoveRows({}, row, row);
            m_items.removeAt(row);
            endRemoveRows();
        }
    }

    QStringList needDetails;
    for (const adb::DeviceEntry& entry : entries) {
        const int row = indexOfSerial(entry.serial);
        if (row < 0) {
            beginInsertRows({}, static_cast<int>(m_items.size()), static_cast<int>(m_items.size()));
            Item item;
            item.entry = entry;
            m_items.append(item);
            endInsertRows();
            if (entry.isOnline()) {
                needDetails << entry.serial;
            }
        } else if (!(m_items.at(row).entry == entry)) {
            const bool wasOnline = m_items.at(row).entry.isOnline();
            m_items[row].entry = entry;
            emit dataChanged(index(row), index(row));
            if (entry.isOnline() && (!wasOnline || !m_items.at(row).detailsLoaded)) {
                needDetails << entry.serial;
            }
        }
    }
    emit countChanged();
    for (const QString& serial : std::as_const(needDetails)) {
        refreshDetails(serial);
    }
}

void DeviceModel::refreshDetails(const QString& serial) {
    m_client->shell(this, serial, QStringLiteral("getprop"), 15000, [this, serial](const adb::ProcessResult& r) {
        const int row = indexOfSerial(serial);
        if (row < 0 || !r.ok()) {
            return;
        }
        const auto props = adb::parseGetprop(r.text());
        Item& item = m_items[row];
        item.manufacturer = props.value(QStringLiteral("ro.product.manufacturer"));
        item.androidVersion = props.value(QStringLiteral("ro.build.version.release"));
        item.sdk = props.value(QStringLiteral("ro.build.version.sdk")).toInt();
        if (item.entry.model.isEmpty()) {
            item.entry.model = props.value(QStringLiteral("ro.product.model"));
        }
        item.detailsLoaded = true;
        emit dataChanged(index(row), index(row));
    });
    refreshBattery(serial);
}

void DeviceModel::refreshBattery(const QString& serial) {
    m_client->shell(this, serial, QStringLiteral("dumpsys battery"), 10000, [this, serial](const adb::ProcessResult& r) {
        const int row = indexOfSerial(serial);
        if (row < 0 || !r.ok()) {
            return;
        }
        const adb::BatteryInfo info = adb::parseBattery(r.text());
        if (info.isValid()) {
            m_items[row].battery = info;
            emit dataChanged(index(row), index(row), {BatteryLevelRole, BatteryStatusRole, PowerRole});
        }
    });
}

void DeviceModel::refreshAllBatteries() {
    for (const Item& item : std::as_const(m_items)) {
        if (item.entry.isOnline()) {
            refreshBattery(item.entry.serial);
        }
    }
}

} // namespace polyscope
