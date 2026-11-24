#pragma once

#include "adb/AdbParsers.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Remote file browser for one device (`ls -la`, `pull`, `push`, ...).
class FileBrowserModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        IsDirRole,
        IsLinkRole,
        SizeRole,
        SizeTextRole,
        ModifiedRole,
        PermissionsRole,
        LinkTargetRole,
        FullPathRole
    };

    explicit FileBrowserModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    QString path() const { return m_path; }
    bool busy() const { return m_busy > 0; }
    QString error() const { return m_error; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void cd(const QString& path);
    Q_INVOKABLE void open(int row);
    Q_INVOKABLE void up();
    // Name of the entry in `row`, or empty when the row no longer exists (list refreshed meanwhile).
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE void pull(int row, const QString& localDir);
    Q_INVOKABLE void push(const QStringList& localPaths);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void mkdir(const QString& name);
    Q_INVOKABLE void rename(int row, const QString& newName);
    Q_INVOKABLE QString join(const QString& dir, const QString& name) const;

    static QString normalizePath(const QString& path);

signals:
    void serialChanged();
    void pathChanged();
    void busyChanged();
    void errorChanged();
    void operationFinished(bool ok, const QString& message);

private:
    void setError(const QString& error);
    void beginBusy();
    void endBusy();
    void runOp(const QStringList& adbArgs, const QString& okMessage, bool refreshAfter);

    QString m_serial;
    QString m_path = QStringLiteral("/sdcard");
    QString m_listedPath; // directory that m_entries actually belongs to (last successful listing)
    QList<adb::FileEntry> m_entries;
    QString m_error;
    int m_busy = 0;
    int m_generation = 0; // discards stale listings after quick navigation
};

} // namespace polyscope
