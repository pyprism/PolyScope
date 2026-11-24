#include "tools/PackageModel.h"
#include "tools/StaleResult.h"

#include "adb/AdbClient.h"
#include "adb/AdbParsers.h"
#include "core/AppContext.h"

#include <QDir>
#include <QRegularExpression>
#include <QUrl>

namespace polyscope {

namespace {

QString toLocalPath(const QString& pathOrUrl) {
    return pathOrUrl.startsWith(QStringLiteral("file:")) ? QUrl(pathOrUrl).toLocalFile() : pathOrUrl;
}

// Package names are dotted identifiers; reject anything else before it reaches a shell.
bool validPackage(const QString& package) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.]+$"));
    return re.match(package).hasMatch();
}

} // namespace

PackageModel::PackageModel(QObject* parent) : QAbstractListModel(parent) {}

int PackageModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_view.size());
}

QHash<int, QByteArray> PackageModel::roleNames() const {
    return {{Qt::DisplayRole, "packageName"}};
}

QVariant PackageModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_view.size() || role != Qt::DisplayRole) {
        return {};
    }
    return m_view.at(index.row());
}

void PackageModel::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    m_serial = serial;
    ++m_generation; // drop any listing still in flight for the previous device
    emit serialChanged();
    m_all.clear();
    applyFilter();
    if (!m_serial.isEmpty()) {
        refresh();
    }
}

void PackageModel::setScope(const QString& scope) {
    static const QStringList allowed{QStringLiteral("user"), QStringLiteral("system"), QStringLiteral("all"),
                                     QStringLiteral("disabled"), QStringLiteral("enabled")};
    if (scope == m_scope || !allowed.contains(scope)) {
        return;
    }
    m_scope = scope;
    emit scopeChanged();
    m_all.clear();
    applyFilter();
    refresh();
}

void PackageModel::setFilter(const QString& filter) {
    if (filter == m_filter) {
        return;
    }
    m_filter = filter;
    applyFilter();
    emit filterChanged();
}

void PackageModel::applyFilter() {
    beginResetModel();
    m_view.clear();
    for (const QString& p : std::as_const(m_all)) {
        if (m_filter.isEmpty() || p.contains(m_filter, Qt::CaseInsensitive)) {
            m_view.append(p);
        }
    }
    endResetModel();
    emit countChanged();
}

void PackageModel::beginBusy() {
    if (m_busy++ == 0) {
        emit busyChanged();
    }
}

void PackageModel::endBusy() {
    if (--m_busy == 0) {
        emit busyChanged();
    }
}

void PackageModel::refresh() {
    if (m_serial.isEmpty()) {
        return;
    }
    static const QHash<QString, QString> flags = {
        {QStringLiteral("user"), QStringLiteral(" -3")},   {QStringLiteral("system"), QStringLiteral(" -s")},
        {QStringLiteral("all"), QString()},                {QStringLiteral("disabled"), QStringLiteral(" -d")},
        {QStringLiteral("enabled"), QStringLiteral(" -e")}};
    const int generation = ++m_generation;
    beginBusy();
    AppContext::instance()->adb()->shell(this, m_serial, QStringLiteral("pm list packages") + flags.value(m_scope), 30000,
                                         [this, generation](const adb::ProcessResult& r) {
                                             endBusy();
                                             if (generation != m_generation) {
                                                 return;
                                             }
                                             if (!r.ok()) {
                                                 emit operationFinished(QStringLiteral("list"), false, r.errorText());
                                                 return;
                                             }
                                             m_all = adb::parsePackages(r.text());
                                             applyFilter();
                                         });
}

void PackageModel::runAdb(const QString& op, const QStringList& args, bool refreshAfter, int timeoutMs) {
    if (m_serial.isEmpty()) {
        return;
    }
    beginBusy();
    const QString serial = m_serial;
    AppContext::instance()->adb()->run(this, serial, args, timeoutMs,
                                       [this, serial, op, refreshAfter](const adb::ProcessResult& r) {
                                           endBusy();
                                           const QString out = r.text().trimmed();
                                           // `pm`/`adb install` print "Failure [...]" but may still exit 0.
                                           const bool ok = r.ok() && !out.contains(QStringLiteral("Failure")) &&
                                                           !out.contains(QStringLiteral("No activities found")); // monkey exits 0
                                           emit operationFinished(op, ok,
                                                                  labelForSerial(ok ? (out.isEmpty() ? tr("Done") : out)
                                                                                    : (out.isEmpty() ? r.errorText() : out),
                                                                                 serial, m_serial));
                                           if (refreshAfter && serial == m_serial) {
                                               refresh();
                                           }
                                       });
}

void PackageModel::runShell(const QString& op, const QString& command, bool refreshAfter, int timeoutMs) {
    runAdb(op, {QStringLiteral("shell"), command}, refreshAfter, timeoutMs);
}

void PackageModel::install(const QStringList& apks, bool asSplit, bool grantPermissions) {
    QStringList files;
    for (const QString& a : apks) {
        files << toLocalPath(a);
    }
    if (files.isEmpty()) {
        return;
    }
    QStringList base{asSplit && files.size() > 1 ? QStringLiteral("install-multiple") : QStringLiteral("install"),
                     QStringLiteral("-r"), QStringLiteral("-t")};
    if (grantPermissions) {
        base << QStringLiteral("-g");
    }
    if (asSplit && files.size() > 1) {
        runAdb(QStringLiteral("install"), base + files, true, 600000);
    } else {
        for (const QString& f : files) {
            runAdb(QStringLiteral("install"), base + QStringList{f}, true, 600000);
        }
    }
}

void PackageModel::uninstall(const QString& package, bool keepData) {
    if (!validPackage(package)) {
        return;
    }
    QStringList args{QStringLiteral("uninstall")};
    if (keepData) {
        args << QStringLiteral("-k");
    }
    args << package;
    runAdb(QStringLiteral("uninstall"), args, true, 60000);
}

void PackageModel::uninstallForUser(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("uninstall"), QStringLiteral("pm uninstall --user 0 ") + package, true);
    }
}

void PackageModel::restore(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("restore"), QStringLiteral("cmd package install-existing ") + package, true);
    }
}

void PackageModel::disable(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("disable"), QStringLiteral("pm disable-user --user 0 ") + package, true);
    }
}

void PackageModel::enable(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("enable"), QStringLiteral("pm enable ") + package, true);
    }
}

void PackageModel::launch(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("launch"),
                 QStringLiteral("monkey -p ") + package + QStringLiteral(" -c android.intent.category.LAUNCHER 1"), false);
    }
}

void PackageModel::forceStop(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("force-stop"), QStringLiteral("am force-stop ") + package, false);
    }
}

void PackageModel::clearData(const QString& package) {
    if (validPackage(package)) {
        runShell(QStringLiteral("clear"), QStringLiteral("pm clear ") + package, false);
    }
}

void PackageModel::info(const QString& package) {
    if (!validPackage(package) || m_serial.isEmpty()) {
        return;
    }
    const QString serial = m_serial;
    AppContext::instance()->adb()->shell(this, serial, QStringLiteral("dumpsys package ") + package, 30000,
                                         [this, serial, package](const adb::ProcessResult& r) {
                                             emit infoReady(labelForSerial(package, serial, m_serial),
                                                            r.ok() ? r.text() : r.errorText());
                                         });
}

void PackageModel::extractApk(const QString& package, const QString& localDir) {
    if (!validPackage(package) || m_serial.isEmpty()) {
        return;
    }
    // Every package ships "base.apk"; give each its own folder so extractions never overwrite each other.
    const QString dir = QDir(toLocalPath(localDir)).filePath(package);
    if (!QDir().mkpath(dir)) {
        emit operationFinished(QStringLiteral("extract"), false, tr("Cannot create %1").arg(dir));
        return;
    }
    beginBusy();
    auto* client = AppContext::instance()->adb();
    const QString serial = m_serial;
    client->shell(this, serial, QStringLiteral("pm path ") + package, 15000,
                  [this, client, serial, dir, package](const adb::ProcessResult& r) {
                      QStringList remotes;
                      if (!r.ok()) {
                          endBusy();
                          emit operationFinished(QStringLiteral("extract"), false, labelForSerial(r.errorText(), serial, m_serial));
                          return;
                      }
                      for (const QString& line : r.text().split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                          if (line.startsWith(QStringLiteral("package:"))) {
                              remotes << line.mid(8).trimmed();
                          }
                      }
                      if (remotes.isEmpty()) {
                          endBusy();
                          emit operationFinished(QStringLiteral("extract"), false,
                                                 labelForSerial(tr("Package path not found."), serial, m_serial));
                          return;
                      }
                      client->run(this, serial, QStringList{QStringLiteral("pull")} + remotes + QStringList{dir}, 0,
                                  [this, serial, dir, package](const adb::ProcessResult& pr) {
                                      endBusy();
                                      emit operationFinished(QStringLiteral("extract"), pr.ok(),
                                                             labelForSerial(pr.ok() ? tr("Saved %1 to %2").arg(package, dir)
                                                                                    : pr.errorText(),
                                                                            serial, m_serial));
                                  });
                  });
}

} // namespace polyscope
