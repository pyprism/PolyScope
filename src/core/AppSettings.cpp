#include <QJSEngine>
#include "core/AppSettings.h"

#include "core/AppContext.h"

#include <QDir>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>

namespace polyscope {

namespace {

QString serialKey(const QString& serial) {
    return QString::fromLatin1(serial.toUtf8().toBase64(QByteArray::Base64UrlEncoding |
                                                        QByteArray::OmitTrailingEquals));
}

QString encodeMap(const QVariantMap& map) {
    return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(map)).toJson(QJsonDocument::Compact));
}

QVariantMap decodeMap(const QString& json) {
    return QJsonDocument::fromJson(json.toUtf8()).object().toVariantMap();
}

constexpr int kMaxHistory = 200;

} // namespace

AppSettings::AppSettings(QObject* parent) : QObject(parent) {
    QSettings settings;
    const QString mode = settings.value(QStringLiteral("ui/themeMode"), QStringLiteral("system")).toString();
    m_themeMode = (mode == QStringLiteral("light") || mode == QStringLiteral("dark")) ? mode
                                                                                     : QStringLiteral("system");
    applyTheme();
    if (auto* hints = QGuiApplication::styleHints()) {
        connect(hints, &QStyleHints::colorSchemeChanged, this, &AppSettings::darkChanged);
    }
}

AppSettings* AppSettings::create(QQmlEngine*, QJSEngine*) {
    auto* object = AppContext::instance()->settings();
    // The engine must not delete objects owned by AppContext.
    QJSEngine::setObjectOwnership(object, QJSEngine::CppOwnership);
    return object;
}

void AppSettings::applyTheme() {
    auto* hints = QGuiApplication::styleHints();
    if (!hints) {
        return;
    }
    // Forcing the scheme also recolours native window decorations on supported platforms.
    if (m_themeMode == QStringLiteral("light")) {
        hints->setColorScheme(Qt::ColorScheme::Light);
    } else if (m_themeMode == QStringLiteral("dark")) {
        hints->setColorScheme(Qt::ColorScheme::Dark);
    } else {
        hints->unsetColorScheme();
    }
}

void AppSettings::setThemeMode(const QString& mode) {
    const QString normalized =
        (mode == QStringLiteral("light") || mode == QStringLiteral("dark")) ? mode : QStringLiteral("system");
    if (normalized == m_themeMode) {
        return;
    }
    m_themeMode = normalized;
    QSettings().setValue(QStringLiteral("ui/themeMode"), m_themeMode);
    applyTheme();
    emit themeModeChanged();
    emit darkChanged();
}

bool AppSettings::dark() const {
    // Explicit choices must not depend on the platform honouring setColorScheme().
    if (m_themeMode == QStringLiteral("dark")) {
        return true;
    }
    if (m_themeMode == QStringLiteral("light")) {
        return false;
    }
    auto* hints = QGuiApplication::styleHints();
    return hints && hints->colorScheme() == Qt::ColorScheme::Dark;
}

QString AppSettings::defaultDir(const QString& kind) {
    const auto location = kind == QStringLiteral("record") ? QStandardPaths::MoviesLocation
                                                          : QStandardPaths::PicturesLocation;
    QString base = QStandardPaths::writableLocation(location);
    if (base.isEmpty()) {
        base = QDir::homePath();
    }
    return QDir(base).filePath(QStringLiteral("PolyScope"));
}

QString AppSettings::screenshotDir() const {
    return QSettings().value(QStringLiteral("paths/screenshots"), defaultDir(QStringLiteral("screenshot"))).toString();
}

void AppSettings::setScreenshotDir(const QString& path) {
    QSettings().setValue(QStringLiteral("paths/screenshots"), path);
    emit dirsChanged();
}

QString AppSettings::recordDir() const {
    return QSettings().value(QStringLiteral("paths/recordings"), defaultDir(QStringLiteral("record"))).toString();
}

void AppSettings::setRecordDir(const QString& path) {
    QSettings().setValue(QStringLiteral("paths/recordings"), path);
    emit dirsChanged();
}

QString AppSettings::monoFamily() const {
    return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

QString AppSettings::version() const {
    return QStringLiteral(POLYSCOPE_VERSION);
}

QVariantMap AppSettings::scrcpyOptions(const QString& serial) const {
    return decodeMap(QSettings().value(QStringLiteral("scrcpy/device/") + serialKey(serial)).toString());
}

void AppSettings::setScrcpyOptions(const QString& serial, const QVariantMap& options) {
    QSettings().setValue(QStringLiteral("scrcpy/device/") + serialKey(serial), encodeMap(options));
}

void AppSettings::resetScrcpyOptions(const QString& serial) {
    QSettings().remove(QStringLiteral("scrcpy/device/") + serialKey(serial));
}

QStringList AppSettings::userPresetNames() const {
    QSettings s;
    s.beginGroup(QStringLiteral("scrcpy/presets"));
    QStringList names;
    for (const QString& key : s.childKeys()) {
        names << QString::fromUtf8(QByteArray::fromBase64(key.toLatin1(), QByteArray::Base64UrlEncoding));
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QVariantMap AppSettings::userPreset(const QString& name) const {
    return decodeMap(QSettings().value(QStringLiteral("scrcpy/presets/") + serialKey(name)).toString());
}

void AppSettings::saveUserPreset(const QString& name, const QVariantMap& options) {
    if (name.trimmed().isEmpty()) {
        return;
    }
    QSettings().setValue(QStringLiteral("scrcpy/presets/") + serialKey(name.trimmed()), encodeMap(options));
    emit userPresetsChanged();
}

void AppSettings::deleteUserPreset(const QString& name) {
    QSettings().remove(QStringLiteral("scrcpy/presets/") + serialKey(name));
    emit userPresetsChanged();
}

QStringList AppSettings::commandHistory() const {
    return QSettings().value(QStringLiteral("commands/history")).toStringList();
}

void AppSettings::addCommandHistory(const QString& line) {
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }
    QStringList history = commandHistory();
    history.removeAll(trimmed);
    history.prepend(trimmed);
    while (history.size() > kMaxHistory) {
        history.removeLast();
    }
    QSettings().setValue(QStringLiteral("commands/history"), history);
}

void AppSettings::clearCommandHistory() {
    QSettings().remove(QStringLiteral("commands/history"));
}

QStringList AppSettings::snippets() const {
    return QSettings().value(QStringLiteral("commands/snippets")).toStringList();
}

void AppSettings::setSnippets(const QStringList& list) {
    QSettings().setValue(QStringLiteral("commands/snippets"), list);
}

} // namespace polyscope
