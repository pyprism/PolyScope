#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace polyscope {

// Persistent user settings plus the effective colour scheme.
class AppSettings : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(AppSettings)
    QML_SINGLETON
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool dark READ dark NOTIFY darkChanged)
    Q_PROPERTY(QString screenshotDir READ screenshotDir WRITE setScreenshotDir NOTIFY dirsChanged)
    Q_PROPERTY(QString recordDir READ recordDir WRITE setRecordDir NOTIFY dirsChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString monoFamily READ monoFamily CONSTANT)

public:
    // No default argument: a default-constructible type makes QML bypass create() for singletons.
    explicit AppSettings(QObject* parent);
    static AppSettings* create(QQmlEngine*, QJSEngine*);

    QString themeMode() const { return m_themeMode; } // system | light | dark
    void setThemeMode(const QString& mode);
    bool dark() const;

    QString screenshotDir() const;
    void setScreenshotDir(const QString& path);
    QString recordDir() const;
    void setRecordDir(const QString& path);
    QString version() const;
    // Installed fixed-width family. The generic name "monospace" is not installed on macOS, and Qt
    // spends ~100 ms resolving the alias at startup.
    QString monoFamily() const;

    // Per-device scrcpy options (stored as JSON, keyed by serial).
    Q_INVOKABLE QVariantMap scrcpyOptions(const QString& serial) const;
    Q_INVOKABLE void setScrcpyOptions(const QString& serial, const QVariantMap& options);
    Q_INVOKABLE void resetScrcpyOptions(const QString& serial);

    // User-saved presets (name -> options).
    Q_INVOKABLE QStringList userPresetNames() const;
    Q_INVOKABLE QVariantMap userPreset(const QString& name) const;
    Q_INVOKABLE void saveUserPreset(const QString& name, const QVariantMap& options);
    Q_INVOKABLE void deleteUserPreset(const QString& name);

    // Command history for the Advanced runner.
    Q_INVOKABLE QStringList commandHistory() const;
    Q_INVOKABLE void addCommandHistory(const QString& line);
    Q_INVOKABLE void clearCommandHistory();

    // Favourite / snippet commands.
    Q_INVOKABLE QStringList snippets() const;
    Q_INVOKABLE void setSnippets(const QStringList& list);

    Q_INVOKABLE static QString defaultDir(const QString& kind);

signals:
    void themeModeChanged();
    void darkChanged();
    void dirsChanged();
    void userPresetsChanged();

private:
    void applyTheme();

    QString m_themeMode = QStringLiteral("system");
};

} // namespace polyscope
