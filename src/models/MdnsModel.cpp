#include "models/MdnsModel.h"

#include "adb/AdbClient.h"
#include "core/AppContext.h"

namespace polyscope {

MdnsModel::MdnsModel(QObject* parent) : QAbstractListModel(parent) {}

int MdnsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_services.size());
}

QHash<int, QByteArray> MdnsModel::roleNames() const {
    return {{NameRole, "name"}, {TypeRole, "type"},       {HostRole, "host"},
            {PortRole, "port"}, {AddressRole, "address"}, {PairingRole, "isPairing"}};
}

QVariant MdnsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_services.size()) {
        return {};
    }
    const adb::MdnsService& s = m_services.at(index.row());
    switch (role) {
    case NameRole:
        return s.name;
    case TypeRole:
        return s.type;
    case HostRole:
        return s.host;
    case PortRole:
        return s.port;
    case AddressRole:
        return s.address();
    case PairingRole:
        return s.isPairing();
    default:
        return {};
    }
}

void MdnsModel::refresh() {
    auto* client = AppContext::instance()->adb();
    if (m_busy) {
        return;
    }
    m_busy = true;
    emit busyChanged();
    client->run(this, QString(), {QStringLiteral("mdns"), QStringLiteral("services")}, 10000,
                [this](const adb::ProcessResult& r) {
                    m_busy = false;
                    emit busyChanged();
                    const QString err = r.ok() ? QString() : r.errorText();
                    if (err != m_error) {
                        m_error = err;
                        emit errorChanged();
                    }
                    const bool available = r.ok();
                    if (available != m_available) {
                        m_available = available;
                        emit availableChanged();
                    }
                    beginResetModel();
                    m_services = r.ok() ? adb::parseMdnsServices(r.text()) : QList<adb::MdnsService>();
                    endResetModel();
                });
}

} // namespace polyscope
