#include "scrcpy/ScrcpyArgs.h"

#include <QtTest>

using namespace polyscope::scrcpy;

class TstScrcpyEdge : public QObject {
    Q_OBJECT
private slots:
    void keysAreUniqueAndFlagsWellFormed() {
        QSet<QString> keys;
        for (const OptionSpec& s : optionSpecs()) {
            QVERIFY2(!keys.contains(s.key), qPrintable(s.key));
            keys.insert(s.key);
            QVERIFY2(!s.group.isEmpty(), qPrintable(s.key));
            QVERIFY2(!s.flag.startsWith(QLatin1Char('-')), qPrintable(s.key));
            if (s.kind == OptionKind::Choice) {
                QVERIFY2(!s.choices.isEmpty(), qPrintable(s.key));
            }
        }
    }
    void everyGroupListed() {
        const QStringList groups = optionGroups();
        for (const OptionSpec& s : optionSpecs()) {
            QVERIFY2(groups.contains(s.group), qPrintable(s.key));
        }
    }
    void presetsBuildWithoutWarnings() {
        const QStringList names = presetNames();
        QVERIFY(!names.isEmpty());
        for (const QString& n : names) {
            const QVariantMap o = presetOptions(n);
            if (n == QStringLiteral("Record")) {
                QVERIFY(!validateOptions(o).isEmpty()); // no output path chosen yet
            } else {
                QVERIFY2(validateOptions(o).isEmpty(), qPrintable(n));
            }
            const QStringList args = buildArguments(QStringLiteral("S"), o);
            QCOMPARE(args.first(), QStringLiteral("--serial=S"));
        }
    }
    void unknownPresetFallsBackToDefaults() {
        QCOMPARE(presetOptions(QStringLiteral("no-such-preset")), defaultOptions());
    }
    void everyArgIsSingleFlagToken() {
        QVariantMap o = defaultOptions();
        o[QStringLiteral("windowTitle")] = QStringLiteral("a --evil=1");
        const QStringList args = buildArguments(QStringLiteral("S"), o);
        for (const QString& a : args) {
            QVERIFY2(a.startsWith(QStringLiteral("--")), qPrintable(a));
        }
    }
    void defaultOptionsAreValid() {
        QVERIFY(validateOptions(defaultOptions()).isEmpty());
    }
    void serialWithSpecialCharsKeptAsOneArg() {
        const QStringList a = buildArguments(QStringLiteral("adb-X._adb-tls-connect._tcp"), {});
        QCOMPARE(a.size(), 1);
        QCOMPARE(a.first(), QStringLiteral("--serial=adb-X._adb-tls-connect._tcp"));
    }
    void previewQuoting() {
        QCOMPARE(commandLinePreview(QStringLiteral("scrcpy"), {}), QStringLiteral("scrcpy"));
        QCOMPARE(commandLinePreview(QStringLiteral("scrcpy"), {QStringLiteral("--a"), QStringLiteral("--b=1")}),
                 QStringLiteral("scrcpy --a --b=1"));
        QVERIFY(commandLinePreview(QStringLiteral("scrcpy"), {QStringLiteral("--t=it's")}).contains(QStringLiteral("'")));
    }
    void schemaMatchesSpecCount() {
        QCOMPARE(optionSchema().size(), optionSpecs().size());
    }
};

QTEST_APPLESS_MAIN(TstScrcpyEdge)
#include "tst_scrcpyedge.moc"
