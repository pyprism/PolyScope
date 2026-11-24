#include "tools/ServicesModel.h"
#include "tools/StaleResult.h"

#include "adb/AdbClient.h"
#include "core/AppContext.h"

#include <QRegularExpression>

namespace polyscope {

namespace {
bool safeToken(const QString& s) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.$/:-]+$"));
    return re.match(s).hasMatch();
}
} // namespace

ServicesModel::ServicesModel(QObject* parent) : QAbstractListModel(parent) {}

int ServicesModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_view.size());
}

QHash<int, QByteArray> ServicesModel::roleNames() const {
    return {{NameRole, "name"}, {DetailRole, "detail"}, {PackageRole, "packageName"}, {ComponentRole, "component"}};
}

QVariant ServicesModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_view.size()) {
        return {};
    }
    const Row& r = m_view.at(index.row());
    switch (role) {
    case NameRole:
        return r.name;
    case DetailRole:
        return r.detail;
    case PackageRole:
        return r.package;
    case ComponentRole:
        return r.component;
    default:
        return {};
    }
}

void ServicesModel::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    m_serial = serial;
    ++m_generation; // drop any listing still in flight for the previous device
    emit serialChanged();
    m_all.clear();
    applyFilter();
    if (serial.isEmpty()) {
        setBusy(false); // the invalidated request will no longer clear it
    } else {
        refresh();
    }
}

void ServicesModel::setMode(const QString& mode) {
    static const QStringList allowed{QStringLiteral("binder"), QStringLiteral("dumpsys"), QStringLiteral("running")};
    if (mode == m_mode || !allowed.contains(mode)) {
        return;
    }
    m_mode = mode;
    emit modeChanged();
    m_all.clear(); // rows of the previous mode must not stay actionable while this one loads
    applyFilter();
    refresh();
}

void ServicesModel::setFilter(const QString& filter) {
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    applyFilter();
    emit filterChanged();
}

void ServicesModel::setBusy(bool busy) {
    if (busy != m_busy) {
        m_busy = busy;
        emit busyChanged();
    }
}

void ServicesModel::applyFilter() {
    beginResetModel();
    m_view.clear();
    for (const Row& r : std::as_const(m_all)) {
        if (m_filter.isEmpty() || r.name.contains(m_filter, Qt::CaseInsensitive) ||
            r.detail.contains(m_filter, Qt::CaseInsensitive) || r.package.contains(m_filter, Qt::CaseInsensitive)) {
            m_view.append(r);
        }
    }
    endResetModel();
    emit countChanged();
}

void ServicesModel::refresh() {
    if (m_serial.isEmpty()) {
        return;
    }
    const QString mode = m_mode;
    const QString command = mode == QStringLiteral("binder")    ? QStringLiteral("service list")
                            : mode == QStringLiteral("dumpsys") ? QStringLiteral("dumpsys -l")
                                                               : QStringLiteral("dumpsys activity services");
    const int generation = ++m_generation;
    setBusy(true);
    AppContext::instance()->adb()->shell(this, m_serial, command, 60000, [this, mode, generation](const adb::ProcessResult& r) {
        if (generation != m_generation) {
            return;
        }
        setBusy(false);
        if (!r.ok()) {
            emit operationFinished(QStringLiteral("list"), false, r.errorText());
            return;
        }
        QList<Row> rows;
        if (mode == QStringLiteral("binder")) {
            for (const auto& s : adb::parseServiceList(r.text())) {
                rows.append({s.name, s.interface, {}, {}});
            }
        } else if (mode == QStringLiteral("dumpsys")) {
            for (const QString& name : adb::parseDumpsysList(r.text())) {
                rows.append({name, {}, {}, {}});
            }
        } else {
            for (const auto& s : adb::parseRunningServices(r.text())) {
                rows.append({s.component, QStringLiteral("user %1").arg(s.userId), s.package, s.component});
            }
        }
        m_all = rows;
        applyFilter();
    });
}

void ServicesModel::dump(const QString& name) {
    if (m_serial.isEmpty() || !safeToken(name)) {
        return;
    }
    const QString serial = m_serial;
    AppContext::instance()->adb()->shell(this, serial, QStringLiteral("dumpsys ") + adb::shellQuote(name), 60000,
                                         [this, serial, name](const adb::ProcessResult& r) {
                                             emit dumpReady(labelForSerial(name, serial, m_serial), r.ok() ? r.text() : r.errorText());
                                         });
}

void ServicesModel::runShell(const QString& op, const QString& command, bool refreshAfter) {
    const QString serial = m_serial;
    AppContext::instance()->adb()->shell(this, serial, command, 30000,
                                         [this, serial, op, refreshAfter](const adb::ProcessResult& r) {
                                             const QString out = r.text().trimmed();
                                             // am/pm report failure on stdout and may still exit 0.
                                             const bool ok = r.ok() && !adb::shellOutputIndicatesFailure(out);
                                             emit operationFinished(
                                                 op, ok,
                                                 labelForSerial(out.isEmpty() ? (ok ? tr("Done") : r.errorText()) : out, serial, m_serial));
                                             if (refreshAfter && serial == m_serial) {
                                                 refresh();
                                             }
                                         });
}

void ServicesModel::stopService(const QString& component) {
    if (!m_serial.isEmpty() && safeToken(component)) {
        runShell(QStringLiteral("stop-service"), QStringLiteral("am stopservice -n ") + adb::shellQuote(component), true);
    }
}

void ServicesModel::forceStopPackage(const QString& package) {
    if (!m_serial.isEmpty() && safeToken(package)) {
        runShell(QStringLiteral("force-stop"), QStringLiteral("am force-stop ") + package, true);
    }
}

void ServicesModel::disablePackage(const QString& package) {
    if (!m_serial.isEmpty() && safeToken(package)) {
        runShell(QStringLiteral("disable"), QStringLiteral("pm disable-user --user 0 ") + package, true);
    }
}

} // namespace polyscope
