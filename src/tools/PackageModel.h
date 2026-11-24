#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Installed packages for one device plus the common package operations.
class PackageModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(QString scope READ scope WRITE setScope NOTIFY scopeChanged) // user|system|all|disabled|enabled
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    explicit PackageModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    QString scope() const { return m_scope; }
    void setScope(const QString& scope);
    QString filter() const { return m_filter; }
    void setFilter(const QString& filter);
    bool busy() const { return m_busy > 0; }
    int totalCount() const { return static_cast<int>(m_all.size()); }

    Q_INVOKABLE void refresh();
    // Local paths or file:// URLs. `asSplit` installs them together (install-multiple).
    Q_INVOKABLE void install(const QStringList& apks, bool asSplit = false, bool grantPermissions = false);
    Q_INVOKABLE void uninstall(const QString& package, bool keepData = false);
    Q_INVOKABLE void uninstallForUser(const QString& package);
    Q_INVOKABLE void restore(const QString& package);
    Q_INVOKABLE void disable(const QString& package);
    Q_INVOKABLE void enable(const QString& package);
    Q_INVOKABLE void launch(const QString& package);
    Q_INVOKABLE void forceStop(const QString& package);
    Q_INVOKABLE void clearData(const QString& package);
    Q_INVOKABLE void info(const QString& package);
    // Pull the APK(s) of a package into a local folder.
    Q_INVOKABLE void extractApk(const QString& package, const QString& localDir);

signals:
    void serialChanged();
    void scopeChanged();
    void filterChanged();
    void busyChanged();
    void countChanged();
    void operationFinished(const QString& op, bool ok, const QString& message);
    void infoReady(const QString& package, const QString& text);

private:
    void applyFilter();
    void runShell(const QString& op, const QString& command, bool refreshAfter, int timeoutMs = 30000);
    void runAdb(const QString& op, const QStringList& args, bool refreshAfter, int timeoutMs);
    void beginBusy();
    void endBusy();

    QString m_serial;
    QString m_scope = QStringLiteral("user");
    QString m_filter;
    QStringList m_all;
    QStringList m_view;
    int m_busy = 0;
    int m_generation = 0;
};

} // namespace polyscope
