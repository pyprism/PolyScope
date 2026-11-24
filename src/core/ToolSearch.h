#pragma once

#include <QString>
#include <QStringList>

namespace polyscope::tools {

enum class Platform { MacOS, Linux, Windows };
enum class Tool { Adb, Scrcpy };

struct SearchEnv {
    Platform platform = Platform::Linux;
    QString home;
    QString androidHome;
    QString androidSdkRoot;
    QString localAppData;
    QString programFiles;
    QStringList pathDirs; // entries from $PATH
};

Platform currentPlatform();
SearchEnv currentSearchEnv();

QString executableName(Tool tool, Platform platform);

// Ordered, de-duplicated list of full executable paths worth probing.
// $PATH entries come first, followed by well-known install locations. GUI-launched
// apps (macOS Finder, Linux desktop files) often miss Homebrew / user paths.
QStringList candidateExecutables(Tool tool, const SearchEnv& env);

} // namespace polyscope::tools
