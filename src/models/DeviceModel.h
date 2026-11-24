#pragma once

#include "adb/AdbParsers.h"

#include <QAbstractListModel>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace polyscope {

namespace adb {
class AdbClient;
class AdbTracker;
} // namespace adb

// Live list of adb devices, keyed by serial. Fed by AdbTracker (push-based).
class DeviceModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(Devices)
    QML_SINGLETON
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY countChanged)
    Q_PROPERTY(bool trackerConnected READ trackerConnected NOTIFY trackerConnectedChanged)

public:
    enum Role {
        SerialRole = Qt::UserRole + 1,
        StateRole,
        NameRole,
        ModelRole,
        ProductRole,
        TransportRole,
        OnlineRole,
        WirelessRole,
        ManufacturerRole,
        AndroidVersionRole,
        SdkRole,
        BatteryLevelRole,
        BatteryStatusRole,
        PowerRole,
        DetailsLoadedRole,
    };

    DeviceModel(adb::AdbClient* client, adb::AdbTracker* tracker, QObject* parent = nullptr);
    static DeviceModel* create(QQmlEngine*, QJSEngine*);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int onlineCount() const;
    bool trackerConnected() const;

    Q_INVOKABLE int indexOfSerial(const QString& serial) const;
    Q_INVOKABLE QVariantMap get(const QString& serial) const;
    Q_INVOKABLE QStringList onlineSerials() const;
    Q_INVOKABLE void refreshDetails(const QString& serial);

    // Test hook / tracker slot: apply a full `devices -l` listing.
    void applyListing(const QString& listing);

signals:
    void countChanged();
    void trackerConnectedChanged();

private:
    struct Item {
        adb::DeviceEntry entry;
        QString manufacturer;
        QString androidVersion;
        int sdk = 0;
        adb::BatteryInfo battery;
        bool detailsLoaded = false;
    };

    void refreshBattery(const QString& serial);
    void refreshAllBatteries();

    adb::AdbClient* m_client;
    adb::AdbTracker* m_tracker;
    QList<Item> m_items;
    QTimer m_batteryTimer;
};

} // namespace polyscope
