#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace polyscope {

// Locates and validates the user-installed adb and scrcpy binaries.
// Resolution order: user override (Settings) -> $PATH -> well-known locations.
class ToolPaths : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Tools)
    QML_SINGLETON
    Q_PROPERTY(QString adbPath READ adbPath NOTIFY adbChanged)
    Q_PROPERTY(QString adbVersion READ adbVersion NOTIFY adbChanged)
    Q_PROPERTY(bool adbReady READ adbReady NOTIFY adbChanged)
    Q_PROPERTY(QString adbError READ adbError NOTIFY adbChanged)
    Q_PROPERTY(bool adbOverridden READ adbOverridden NOTIFY adbChanged)
    Q_PROPERTY(QString scrcpyPath READ scrcpyPath NOTIFY scrcpyChanged)
    Q_PROPERTY(QString scrcpyVersion READ scrcpyVersion NOTIFY scrcpyChanged)
    Q_PROPERTY(bool scrcpyReady READ scrcpyReady NOTIFY scrcpyChanged)
    Q_PROPERTY(QString scrcpyError READ scrcpyError NOTIFY scrcpyChanged)
    Q_PROPERTY(bool scrcpyOverridden READ scrcpyOverridden NOTIFY scrcpyChanged)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)

public:
    // No default argument: a default-constructible type makes QML bypass create() for singletons.
    explicit ToolPaths(QObject* parent);
    static ToolPaths* create(QQmlEngine*, QJSEngine*);

    QString adbPath() const { return m_adb.path; }
    QString adbVersion() const { return m_adb.version; }
    bool adbReady() const { return m_adb.ready; }
    QString adbError() const { return m_adb.error; }
    bool adbOverridden() const { return !m_adb.override.isEmpty(); }
    QString scrcpyPath() const { return m_scrcpy.path; }
    QString scrcpyVersion() const { return m_scrcpy.version; }
    bool scrcpyReady() const { return m_scrcpy.ready; }
    QString scrcpyError() const { return m_scrcpy.error; }
    bool scrcpyOverridden() const { return !m_scrcpy.override.isEmpty(); }
    bool checking() const { return m_pending > 0; }

    // Accept a local path or a file:// URL (from FileDialog). Empty clears the override.
    Q_INVOKABLE void setAdbPath(const QString& pathOrUrl);
    Q_INVOKABLE void setScrcpyPath(const QString& pathOrUrl);
    Q_INVOKABLE void refresh();

signals:
    void adbChanged();
    void scrcpyChanged();
    void checkingChanged();

private:
    struct Entry {
        QString override;
        QString path;
        QString version;
        QString error;
        bool ready = false;
    };
    enum class Kind { Adb, Scrcpy };

    void check(Kind kind);
    void setOverride(Kind kind, const QString& pathOrUrl);
    Entry& entry(Kind kind) { return kind == Kind::Adb ? m_adb : m_scrcpy; }
    void notify(Kind kind);

    Entry m_adb;
    Entry m_scrcpy;
    int m_pending = 0;
};

} // namespace polyscope
