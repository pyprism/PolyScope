#include "tools/FileBrowserModel.h"
#include "tools/StaleResult.h"

#include "adb/AdbClient.h"
#include "core/AppContext.h"

#include <QDir>
#include <QLocale>
#include <QUrl>
#include <algorithm>

namespace polyscope {

namespace {

QString toLocalPath(const QString& pathOrUrl) {
    if (pathOrUrl.startsWith(QStringLiteral("file:"))) {
        return QUrl(pathOrUrl).toLocalFile();
    }
    return pathOrUrl;
}

// A single path component: no separators, and not "." / "..".
bool validName(const QString& name) {
    return !name.isEmpty() && !name.contains(QLatin1Char('/')) && name != QStringLiteral(".") &&
           name != QStringLiteral("..");
}

} // namespace

FileBrowserModel::FileBrowserModel(QObject* parent) : QAbstractListModel(parent) {}

QString FileBrowserModel::normalizePath(const QString& path) {
    QString p = path.trimmed();
    if (!p.startsWith(QLatin1Char('/'))) {
        p.prepend(QLatin1Char('/'));
    }
    return QDir::cleanPath(p);
}

QString FileBrowserModel::join(const QString& dir, const QString& name) const {
    return dir == QStringLiteral("/") ? QLatin1Char('/') + name : dir + QLatin1Char('/') + name;
}

int FileBrowserModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QHash<int, QByteArray> FileBrowserModel::roleNames() const {
    return {{NameRole, "name"},        {IsDirRole, "isDir"},       {IsLinkRole, "isLink"},
            {SizeRole, "size"},        {SizeTextRole, "sizeText"}, {ModifiedRole, "modified"},
            {PermissionsRole, "permissions"}, {LinkTargetRole, "linkTarget"}, {FullPathRole, "fullPath"}};
}

QVariant FileBrowserModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_entries.size()) {
        return {};
    }
    const adb::FileEntry& e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case IsDirRole:
        return e.isDir;
    case IsLinkRole:
        return e.isLink;
    case SizeRole:
        return e.size;
    case SizeTextRole:
        return e.isDir ? QString() : QLocale().formattedDataSize(e.size);
    case ModifiedRole:
        return e.modified;
    case PermissionsRole:
        return e.permissions;
    case LinkTargetRole:
        return e.linkTarget;
    case FullPathRole:
        return join(m_path, e.name);
    default:
        return {};
    }
}

void FileBrowserModel::setSerial(const QString& serial) {
    if (serial == m_serial) {
        return;
    }
    m_serial = serial;
    ++m_generation; // drop any listing still in flight for the previous device
    m_listedPath.clear();
    emit serialChanged();
    beginResetModel();
    m_entries.clear();
    endResetModel();
    if (!m_serial.isEmpty()) {
        refresh();
    }
}

void FileBrowserModel::setError(const QString& error) {
    if (error != m_error) {
        m_error = error;
        emit errorChanged();
    }
}

void FileBrowserModel::beginBusy() {
    if (m_busy++ == 0) {
        emit busyChanged();
    }
}

void FileBrowserModel::endBusy() {
    if (--m_busy == 0) {
        emit busyChanged();
    }
}

void FileBrowserModel::refresh() {
    if (m_serial.isEmpty()) {
        return;
    }
    const int generation = ++m_generation;
    const QString path = m_path;
    beginBusy();
    AppContext::instance()->adb()->shell(
        this, m_serial, QStringLiteral("ls -la ") + adb::shellQuote(path + QLatin1Char('/')), 20000,
        [this, generation, path](const adb::ProcessResult& r) {
            endBusy();
            if (generation != m_generation) {
                return;
            }
            if (!r.ok() && !m_listedPath.isEmpty() && m_listedPath != path) {
                // Not a directory, missing or denied: stay where the visible rows are.
                emit operationFinished(false, r.errorText().isEmpty() ? tr("Cannot open %1").arg(path) : r.errorText());
                m_path = m_listedPath;
                emit pathChanged();
                refresh();
                return;
            }
            QList<adb::FileEntry> entries = adb::parseLsLong(r.text());
            std::sort(entries.begin(), entries.end(), [](const adb::FileEntry& a, const adb::FileEntry& b) {
                const bool aDir = a.isDir || (a.isLink && !a.linkTarget.isEmpty() && a.linkTarget.endsWith(QLatin1Char('/')));
                const bool bDir = b.isDir || (b.isLink && !b.linkTarget.isEmpty() && b.linkTarget.endsWith(QLatin1Char('/')));
                if (aDir != bDir) {
                    return aDir;
                }
                return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
            });
            setError(entries.isEmpty() && !r.ok() ? r.errorText() : QString());
            beginResetModel();
            m_entries = entries;
            m_listedPath = path;
            endResetModel();
        });
}

void FileBrowserModel::cd(const QString& path) {
    const QString normalized = normalizePath(path);
    if (normalized != m_path) {
        m_path = normalized;
        emit pathChanged();
    }
    // Rows from the old directory must not be actionable while the new one loads.
    if (!m_entries.isEmpty()) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
    }
    refresh();
}

void FileBrowserModel::open(int row) {
    if (row < 0 || row >= m_entries.size()) {
        return;
    }
    const adb::FileEntry& e = m_entries.at(row);
    if (e.isDir) {
        cd(join(m_path, e.name));
    } else if (e.isLink) {
        // Symlinks to directories are common (/sdcard); try to enter them.
        cd(e.linkTarget.startsWith(QLatin1Char('/')) ? e.linkTarget : join(m_path, e.linkTarget));
    }
}

QString FileBrowserModel::nameAt(int row) const {
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).name : QString();
}

void FileBrowserModel::up() {
    cd(QDir::cleanPath(m_path + QStringLiteral("/..")));
}

void FileBrowserModel::runOp(const QStringList& adbArgs, const QString& okMessage, bool refreshAfter) {
    beginBusy();
    const QString serial = m_serial;
    AppContext::instance()->adb()->run(this, serial, adbArgs, 0,
                                       [this, serial, okMessage, refreshAfter](const adb::ProcessResult& r) {
                                           endBusy();
                                           emit operationFinished(r.ok(), labelForSerial(r.ok() ? okMessage : r.errorText(), serial, m_serial));
                                           if (refreshAfter && serial == m_serial) {
                                               refresh();
                                           }
                                       });
}

void FileBrowserModel::pull(int row, const QString& localDir) {
    if (row < 0 || row >= m_entries.size() || m_serial.isEmpty()) {
        return;
    }
    const QString local = toLocalPath(localDir);
    runOp({QStringLiteral("pull"), join(m_path, m_entries.at(row).name), local},
          tr("Saved %1 to %2").arg(m_entries.at(row).name, local), false);
}

void FileBrowserModel::push(const QStringList& localPaths) {
    if (m_serial.isEmpty() || localPaths.isEmpty()) {
        return;
    }
    QStringList args{QStringLiteral("push")};
    for (const QString& p : localPaths) {
        args << toLocalPath(p);
    }
    args << m_path + QLatin1Char('/');
    runOp(args, tr("Uploaded %n file(s)", nullptr, static_cast<int>(localPaths.size())), true);
}

void FileBrowserModel::remove(int row) {
    if (row < 0 || row >= m_entries.size() || m_serial.isEmpty()) {
        return;
    }
    const QString name = m_entries.at(row).name;
    runOp({QStringLiteral("shell"), QStringLiteral("rm -rf ") + adb::shellQuote(join(m_path, name))},
          tr("Deleted %1").arg(name), true);
}

void FileBrowserModel::mkdir(const QString& name) {
    const QString trimmed = name.trimmed();
    if (m_serial.isEmpty() || !validName(trimmed)) {
        emit operationFinished(false, tr("Invalid name."));
        return;
    }
    runOp({QStringLiteral("shell"), QStringLiteral("mkdir -p ") + adb::shellQuote(join(m_path, trimmed))},
          tr("Created %1").arg(trimmed), true);
}

void FileBrowserModel::rename(int row, const QString& newName) {
    const QString trimmed = newName.trimmed();
    if (row < 0 || row >= m_entries.size() || m_serial.isEmpty() || !validName(trimmed)) {
        emit operationFinished(false, tr("Invalid name."));
        return;
    }
    const QString from = join(m_path, m_entries.at(row).name);
    const QString to = join(m_path, trimmed);
    runOp({QStringLiteral("shell"), QStringLiteral("mv ") + adb::shellQuote(from) + QLatin1Char(' ') + adb::shellQuote(to)},
          tr("Renamed to %1").arg(trimmed), true);
}

} // namespace polyscope
