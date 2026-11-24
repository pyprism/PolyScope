#pragma once

#include "adb/AdbParsers.h"

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

namespace polyscope {

// Android services of a device: binder services (`service list`), dumpsys-able
// services (`dumpsys -l`) and running app services (`dumpsys activity services`).
class ServicesModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString serial READ serial WRITE setSerial NOTIFY serialChanged)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged) // binder | dumpsys | running
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

public:
    enum Role { NameRole = Qt::UserRole + 1, DetailRole, PackageRole, ComponentRole };

    explicit ServicesModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString serial() const { return m_serial; }
    void setSerial(const QString& serial);
    QString mode() const { return m_mode; }
    void setMode(const QString& mode);
    QString filter() const { return m_filter; }
    void setFilter(const QString& filter);
    bool busy() const { return m_busy; }
    int totalCount() const { return static_cast<int>(m_all.size()); }

    Q_INVOKABLE void refresh();
    // `dumpsys <name>` output arrives through dumpReady.
    Q_INVOKABLE void dump(const QString& name);
    // Stop a running app service (`am stopservice -n`).
    Q_INVOKABLE void stopService(const QString& component);
    Q_INVOKABLE void forceStopPackage(const QString& package);
    Q_INVOKABLE void disablePackage(const QString& package);

signals:
    void serialChanged();
    void modeChanged();
    void filterChanged();
    void busyChanged();
    void countChanged();
    void dumpReady(const QString& name, const QString& text);
    void operationFinished(const QString& op, bool ok, const QString& message);

private:
    struct Row {
        QString name;
        QString detail;
        QString package;
        QString component;
    };
    void applyFilter();
    void setBusy(bool busy);
    void runShell(const QString& op, const QString& command, bool refreshAfter);

    QString m_serial;
    QString m_mode = QStringLiteral("binder");
    QString m_filter;
    QList<Row> m_all;
    QList<Row> m_view;
    bool m_busy = false;
    int m_generation = 0;
};

} // namespace polyscope
