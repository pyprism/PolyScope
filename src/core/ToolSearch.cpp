#include "core/ToolSearch.h"

#include <QDir>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace polyscope::tools {

Platform currentPlatform() {
#if defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_MACOS)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

SearchEnv currentSearchEnv() {
    const auto env = QProcessEnvironment::systemEnvironment();
    SearchEnv result;
    result.platform = currentPlatform();
    result.home = QDir::homePath();
    result.androidHome = env.value(QStringLiteral("ANDROID_HOME"));
    result.androidSdkRoot = env.value(QStringLiteral("ANDROID_SDK_ROOT"));
    result.localAppData = env.value(QStringLiteral("LOCALAPPDATA"));
    result.programFiles = env.value(QStringLiteral("ProgramFiles"));
    result.pathDirs =
        env.value(QStringLiteral("PATH")).split(QDir::listSeparator(), Qt::SkipEmptyParts);
    return result;
}

QString executableName(Tool tool, Platform platform) {
    QString name = tool == Tool::Adb ? QStringLiteral("adb") : QStringLiteral("scrcpy");
    if (platform == Platform::Windows) {
        name += QStringLiteral(".exe");
    }
    return name;
}

QStringList candidateExecutables(Tool tool, const SearchEnv& env) {
    QStringList dirs = env.pathDirs;

    auto add = [&dirs](const QString& dir) {
        if (!dir.isEmpty()) {
            dirs.append(dir);
        }
    };
    auto addSdk = [&add](const QString& root) {
        if (!root.isEmpty()) {
            add(root + QStringLiteral("/platform-tools"));
        }
    };

    switch (env.platform) {
    case Platform::MacOS:
        add(QStringLiteral("/opt/homebrew/bin"));
        add(QStringLiteral("/usr/local/bin"));
        add(QStringLiteral("/opt/local/bin"));
        if (tool == Tool::Adb) {
            addSdk(env.androidHome);
            addSdk(env.androidSdkRoot);
            addSdk(env.home + QStringLiteral("/Library/Android/sdk"));
            add(QStringLiteral("/opt/homebrew/share/android-commandlinetools/platform-tools"));
        }
        break;
    case Platform::Linux:
        add(QStringLiteral("/usr/bin"));
        add(QStringLiteral("/usr/local/bin"));
        add(QStringLiteral("/snap/bin"));
        add(QStringLiteral("/home/linuxbrew/.linuxbrew/bin"));
        add(env.home + QStringLiteral("/.local/bin"));
        if (tool == Tool::Adb) {
            addSdk(env.androidHome);
            addSdk(env.androidSdkRoot);
            addSdk(env.home + QStringLiteral("/Android/Sdk"));
        }
        break;
    case Platform::Windows:
        add(env.localAppData + QStringLiteral("/Microsoft/WinGet/Links"));
        add(env.home + QStringLiteral("/scoop/shims"));
        add(QStringLiteral("C:/ProgramData/chocolatey/bin"));
        if (tool == Tool::Scrcpy) {
            add(env.programFiles + QStringLiteral("/scrcpy"));
            add(QStringLiteral("C:/scrcpy"));
        } else {
            addSdk(env.androidHome);
            addSdk(env.androidSdkRoot);
            addSdk(env.localAppData + QStringLiteral("/Android/Sdk"));
            add(QStringLiteral("C:/platform-tools"));
        }
        break;
    }

    const QString name = executableName(tool, env.platform);
    QStringList result;
    for (const QString& dir : std::as_const(dirs)) {
        const QString full = QDir::cleanPath(dir + QLatin1Char('/') + name);
        if (!result.contains(full)) {
            result.append(full);
        }
    }
    return result;
}

} // namespace polyscope::tools
