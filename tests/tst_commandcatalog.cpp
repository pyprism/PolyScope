#include "adb/CommandCatalog.h"

#include <QSet>
#include <QtTest>

using namespace polyscope::adb;

namespace {
ExpandResult run(const char* id, const QVariantMap& values = {}) {
    const CommandSpec* spec = findCommand(QString::fromUtf8(id));
    Q_ASSERT(spec);
    return expandCommand(*spec, values);
}
} // namespace

class TstCommandCatalog : public QObject {
    Q_OBJECT
private slots:
    void idsAreUnique() {
        QSet<QString> ids;
        for (const auto& spec : commandCatalog()) {
            QVERIFY2(!ids.contains(spec.id), qPrintable(spec.id));
            ids.insert(spec.id);
            QVERIFY(!spec.title.isEmpty());
            QVERIFY(!spec.tmpl.isEmpty());
        }
        QVERIFY(commandCatalog().size() > 120);
    }
    void everyCommandExpandsWithDefaults() {
        // Fill each param with a sample so every template is exercised.
        for (const auto& spec : commandCatalog()) {
            QVariantMap values;
            for (const auto& p : spec.params) {
                if (p.type == QStringLiteral("int")) {
                    values[p.key] = 5;
                } else if (p.type == QStringLiteral("choice") && !p.choices.isEmpty()) {
                    values[p.key] = p.defaultValue.isEmpty() ? p.choices.last() : p.defaultValue;
                } else if (p.type == QStringLiteral("bool")) {
                    values[p.key] = true;
                } else {
                    values[p.key] = QStringLiteral("x");
                }
            }
            const auto r = expandCommand(spec, values);
            QVERIFY2(r.ok, qPrintable(spec.id + QStringLiteral(": ") + r.error));
            QVERIFY(!r.args.isEmpty());
        }
    }
    void hostCommand() {
        const auto r = run("conn.connect", {{QStringLiteral("address"), QStringLiteral("1.2.3.4:5555")}});
        QVERIFY(r.ok);
        QCOMPARE(r.args, (QStringList{QStringLiteral("connect"), QStringLiteral("1.2.3.4:5555")}));
    }
    void optionalOmitted() {
        const auto r = run("conn.disconnect");
        QVERIFY(r.ok);
        QCOMPARE(r.args, (QStringList{QStringLiteral("disconnect")}));
    }
    void requiredMissing() {
        const auto r = run("conn.pair", {{QStringLiteral("address"), QStringLiteral("1.2.3.4:1")}});
        QVERIFY(!r.ok);
    }
    void flagsExpand() {
        QVariantMap v{{QStringLiteral("apk"), QStringLiteral("/tmp/a.apk")},
                      {QStringLiteral("grant"), true},
                      {QStringLiteral("downgrade"), false}};
        const auto r = run("app.install", v);
        QVERIFY(r.ok);
        // -r is on by default
        QCOMPARE(r.args, (QStringList{QStringLiteral("install"), QStringLiteral("-r"), QStringLiteral("-g"),
                                      QStringLiteral("/tmp/a.apk")}));
    }
    void filesExpandToMany() {
        const auto r = run("app.install-multiple",
                           {{QStringLiteral("apks"), QStringLiteral("/a.apk\n/b.apk")}});
        QVERIFY(r.ok);
        QVERIFY(r.args.contains(QStringLiteral("/a.apk")));
        QVERIFY(r.args.contains(QStringLiteral("/b.apk")));
    }
    void inlineSubstitution() {
        const auto r = run("conn.wait-for-state", {});
        QVERIFY(r.ok);
        QCOMPARE(r.args, (QStringList{QStringLiteral("wait-for-any-device")}));
    }
    void shellQuotesValues() {
        const auto r = run("file.rm", {{QStringLiteral("path"), QStringLiteral("/sdcard/a b; rm -rf /")}});
        QVERIFY(r.ok);
        QCOMPARE(r.args.at(0), QStringLiteral("shell"));
        QCOMPARE(r.args.at(1), QStringLiteral("rm -rf '/sdcard/a b; rm -rf /'"));
    }
    void rawRejectsMetacharacters() {
        const auto r = run("input.keyevent", {{QStringLiteral("key"), QStringLiteral("3; reboot")}});
        QVERIFY(!r.ok);
        const auto ok = run("input.keyevent", {{QStringLiteral("key"), QStringLiteral("KEYCODE_HOME")}});
        QVERIFY(ok.ok);
        QCOMPARE(ok.args.at(1), QStringLiteral("input keyevent KEYCODE_HOME"));
    }
    void intValidated() {
        QVERIFY(!run("input.tap", {{QStringLiteral("x"), QStringLiteral("a")}, {QStringLiteral("y"), 3}}).ok);
        const auto r = run("input.tap", {{QStringLiteral("x"), 10}, {QStringLiteral("y"), 20}});
        QCOMPARE(r.args.at(1), QStringLiteral("input tap 10 20"));
    }
    void choiceValidated() {
        QVERIFY(!run("net.wifi", {{QStringLiteral("state"), QStringLiteral("explode")}}).ok);
        QCOMPARE(run("net.wifi", {}).args.at(1), QStringLiteral("svc wifi enable"));
    }
    void shellFlagValueQuoted() {
        const auto r = run("app.start-intent", {{QStringLiteral("action"), QStringLiteral("android.intent.action.VIEW")},
                                               {QStringLiteral("data"), QStringLiteral("http://a.b/?x=1&y=2")}});
        QVERIFY(r.ok);
        QCOMPARE(r.args.at(1),
                 QStringLiteral("am start -a 'android.intent.action.VIEW' -d 'http://a.b/?x=1&y=2'"));
    }
    void groupsPresent() {
        const auto groups = commandGroups();
        QVERIFY(groups.contains(QStringLiteral("Apps")));
        QVERIFY(groups.contains(QStringLiteral("Port forwarding")));
        QVERIFY(groups.contains(QStringLiteral("Privileged")));
    }
};

QTEST_APPLESS_MAIN(TstCommandCatalog)
#include "tst_commandcatalog.moc"
