#pragma once

#include "adb/AdbParsers.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Wireless debugging services discovered via `adb mdns services`.
class MdnsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, TypeRole, HostRole, PortRole, AddressRole, PairingRole };

    explicit MdnsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool busy() const { return m_busy; }
    bool available() const { return m_available; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();

signals:
    void busyChanged();
    void availableChanged();
    void errorChanged();

private:
    QList<adb::MdnsService> m_services;
    QString m_error;
    bool m_busy = false;
    bool m_available = true;
};

} // namespace polyscope
