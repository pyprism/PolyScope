#include "core/ToolSearch.h"

#include <QtTest>

using namespace polyscope::tools;

class TstToolSearch : public QObject {
    Q_OBJECT
private slots:
    void macAdb() {
        SearchEnv env;
        env.platform = Platform::MacOS;
        env.home = QStringLiteral("/Users/me");
        env.pathDirs = {QStringLiteral("/usr/bin")};
        const auto list = candidateExecutables(Tool::Adb, env);
        QCOMPARE(list.first(), QStringLiteral("/usr/bin/adb")); // PATH wins
        QVERIFY(list.contains(QStringLiteral("/opt/homebrew/bin/adb")));
        QVERIFY(list.contains(QStringLiteral("/Users/me/Library/Android/sdk/platform-tools/adb")));
    }
    void androidHome() {
        SearchEnv env;
        env.platform = Platform::Linux;
        env.home = QStringLiteral("/home/me");
        env.androidHome = QStringLiteral("/opt/sdk");
        const auto list = candidateExecutables(Tool::Adb, env);
        QVERIFY(list.contains(QStringLiteral("/opt/sdk/platform-tools/adb")));
        QVERIFY(list.contains(QStringLiteral("/home/me/Android/Sdk/platform-tools/adb")));
    }
    void windowsExe() {
        SearchEnv env;
        env.platform = Platform::Windows;
        env.home = QStringLiteral("C:/Users/me");
        env.localAppData = QStringLiteral("C:/Users/me/AppData/Local");
        const auto list = candidateExecutables(Tool::Scrcpy, env);
        QVERIFY(list.contains(QStringLiteral("C:/scrcpy/scrcpy.exe")));
        QVERIFY(list.contains(QStringLiteral("C:/Users/me/AppData/Local/Microsoft/WinGet/Links/scrcpy.exe")));
        QCOMPARE(executableName(Tool::Adb, Platform::Windows), QStringLiteral("adb.exe"));
    }
    void noDuplicates() {
        SearchEnv env;
        env.platform = Platform::Linux;
        env.pathDirs = {QStringLiteral("/usr/bin"), QStringLiteral("/usr/bin")};
        const auto list = candidateExecutables(Tool::Scrcpy, env);
        QCOMPARE(list.count(QStringLiteral("/usr/bin/scrcpy")), 1);
    }
};

QTEST_APPLESS_MAIN(TstToolSearch)
#include "tst_toolsearch.moc"
