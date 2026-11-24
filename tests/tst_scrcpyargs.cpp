#include "scrcpy/ScrcpyArgs.h"

#include <QtTest>

using namespace polyscope::scrcpy;

class TstScrcpyArgs : public QObject {
    Q_OBJECT
private slots:
    void defaultsOnlySerial() {
        QCOMPARE(buildArguments(QStringLiteral("ABC"), {}), (QStringList{QStringLiteral("--serial=ABC")}));
        QVERIFY(buildArguments(QString(), {}).isEmpty());
    }
    void videoAndAudio() {
        QVariantMap o;
        o[QStringLiteral("maxSize")] = 1024;
        o[QStringLiteral("videoBitRate")] = QStringLiteral("8M");
        o[QStringLiteral("maxFps")] = 60;
        o[QStringLiteral("videoCodec")] = QStringLiteral("h265");
        o[QStringLiteral("audio")] = false;
        const auto a = buildArguments(QStringLiteral("S"), o);
        QVERIFY(a.contains(QStringLiteral("--max-size=1024")));
        QVERIFY(a.contains(QStringLiteral("--video-bit-rate=8M")));
        QVERIFY(a.contains(QStringLiteral("--max-fps=60")));
        QVERIFY(a.contains(QStringLiteral("--video-codec=h265")));
        QVERIFY(a.contains(QStringLiteral("--no-audio")));
        QVERIFY(!a.contains(QStringLiteral("--no-control")));
    }
    void invalidBitrateDropped() {
        QVariantMap o;
        o[QStringLiteral("videoBitRate")] = QStringLiteral("8M --evil");
        for (const QString& s : buildArguments({}, o)) {
            QVERIFY(!s.contains(QStringLiteral("evil")));
        }
        QVERIFY(!validateOptions(o).isEmpty());
    }
    void valueCannotInjectFlag() {
        QVariantMap o;
        o[QStringLiteral("windowTitle")] = QStringLiteral("--no-audio");
        const auto a = buildArguments({}, o);
        QCOMPARE(a, (QStringList{QStringLiteral("--window-title=--no-audio")}));
    }
    void controlOptionsNeedControl() {
        QVariantMap o;
        o[QStringLiteral("stayAwake")] = true;
        o[QStringLiteral("keyboard")] = QStringLiteral("uhid");
        QVERIFY(buildArguments({}, o).contains(QStringLiteral("--stay-awake")));
        o[QStringLiteral("control")] = false;
        const auto a = buildArguments({}, o);
        QVERIFY(a.contains(QStringLiteral("--no-control")));
        QVERIFY(!a.contains(QStringLiteral("--stay-awake")));
        QVERIFY(!a.contains(QStringLiteral("--keyboard=uhid")));
    }
    void camera() {
        const auto a = buildArguments(QStringLiteral("S"), presetOptions(QStringLiteral("Camera")));
        QVERIFY(a.contains(QStringLiteral("--video-source=camera")));
        QVERIFY(a.contains(QStringLiteral("--camera-facing=back")));
        QVERIFY(a.contains(QStringLiteral("--no-control")));
    }
    void cameraIgnoresDisplayOptions() {
        QVariantMap o = presetOptions(QStringLiteral("Camera"));
        o[QStringLiteral("crop")] = QStringLiteral("100:100:0:0");
        QVERIFY(!buildArguments({}, o).join(QLatin1Char(' ')).contains(QStringLiteral("--crop")));
    }
    void otgOnlyHid() {
        QVariantMap o = presetOptions(QStringLiteral("OTG keyboard & mouse"));
        o[QStringLiteral("maxSize")] = 800;
        o[QStringLiteral("keyboard")] = QStringLiteral("aoa");
        o[QStringLiteral("alwaysOnTop")] = true;
        const auto a = buildArguments(QStringLiteral("S"), o);
        QVERIFY(a.contains(QStringLiteral("--otg")));
        QVERIFY(a.contains(QStringLiteral("--keyboard=aoa")));
        QVERIFY(a.contains(QStringLiteral("--always-on-top")));
        QVERIFY(!a.contains(QStringLiteral("--max-size=800")));
        QVERIFY(!a.contains(QStringLiteral("--no-audio")));
    }
    void audioOnly() {
        const auto a = buildArguments({}, presetOptions(QStringLiteral("Audio only")));
        QVERIFY(a.contains(QStringLiteral("--no-video")));
        QVERIFY(a.contains(QStringLiteral("--no-control")));
        QVERIFY(!a.contains(QStringLiteral("--no-audio")));
    }
    void recordAndNewDisplay() {
        QVariantMap o;
        o[QStringLiteral("record")] = true;
        o[QStringLiteral("recordPath")] = QStringLiteral("/tmp/a b.mp4");
        o[QStringLiteral("newDisplay")] = true;
        o[QStringLiteral("newDisplaySize")] = QStringLiteral("1920x1080/420");
        const auto a = buildArguments({}, o);
        QVERIFY(a.contains(QStringLiteral("--record=/tmp/a b.mp4")));
        QVERIFY(a.contains(QStringLiteral("--new-display=1920x1080/420")));
    }
    void extraArgsSplit() {
        QVariantMap o;
        o[QStringLiteral("extraArgs")] = QStringLiteral("--render-driver=software \"--window-title=a b\"");
        const auto a = buildArguments({}, o);
        QVERIFY(a.contains(QStringLiteral("--render-driver=software")));
        QVERIFY(a.contains(QStringLiteral("--window-title=a b")));
    }
    void windowPositionAllowsZero() {
        QVariantMap o;
        o[QStringLiteral("windowX")] = QStringLiteral("0");
        QVERIFY(buildArguments({}, o).contains(QStringLiteral("--window-x=0")));
    }
    void schemaCoversEveryKey() {
        const auto defaults = defaultOptions();
        const auto schema = optionSchema();
        QCOMPARE(schema.size(), defaults.size());
        QVERIFY(optionGroups().contains(QStringLiteral("Audio")));
        for (const QString& name : presetNames()) {
            QVERIFY(!presetOptions(name).isEmpty());
        }
    }
    void preview() {
        QCOMPARE(commandLinePreview(QStringLiteral("scrcpy"), {QStringLiteral("--window-title=a b")}),
                 QStringLiteral("scrcpy '--window-title=a b'"));
    }
};

QTEST_APPLESS_MAIN(TstScrcpyArgs)
#include "tst_scrcpyargs.moc"
