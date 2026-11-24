#include "adb/CommandCatalog.h"

#include <QRegularExpression>
#include <QtTest>

using namespace polyscope::adb;

// Structural checks over the whole catalogue so a bad entry fails CI, not a user's click.
class TstCatalogIntegrity : public QObject {
    Q_OBJECT
private slots:
    void everyEntryIsComplete() {
        for (const CommandSpec& c : commandCatalog()) {
            QVERIFY2(!c.id.isEmpty(), "empty id");
            QVERIFY2(!c.title.isEmpty(), qPrintable(c.id));
            QVERIFY2(!c.group.isEmpty(), qPrintable(c.id));
            QVERIFY2(!c.tmpl.isEmpty(), qPrintable(c.id));
        }
    }
    void idsAreSlugs() {
        static const QRegularExpression slug(QStringLiteral("^[a-z0-9][a-z0-9._-]*$"));
        for (const CommandSpec& c : commandCatalog()) {
            QVERIFY2(slug.match(c.id).hasMatch(), qPrintable(c.id));
        }
    }
    void placeholdersReferenceDeclaredParams() {
        static const QRegularExpression ph(QStringLiteral("\\{[!*]?([A-Za-z0-9_]+)\\}"));
        for (const CommandSpec& c : commandCatalog()) {
            QSet<QString> keys;
            for (const CommandParam& p : c.params) {
                keys.insert(p.key);
            }
            for (const QString& token : c.tmpl) {
                auto it = ph.globalMatch(token);
                while (it.hasNext()) {
                    const QString key = it.next().captured(1);
                    QVERIFY2(keys.contains(key), qPrintable(c.id + QStringLiteral(" uses undeclared {") + key + QLatin1Char('}')));
                }
            }
        }
    }
    void paramKeysUniqueAndTypesKnown() {
        static const QStringList types = {QStringLiteral("text"), QStringLiteral("int"), QStringLiteral("choice"),
                                          QStringLiteral("file"), QStringLiteral("files"), QStringLiteral("dir"),
                                          QStringLiteral("bool")};
        for (const CommandSpec& c : commandCatalog()) {
            QSet<QString> seen;
            for (const CommandParam& p : c.params) {
                QVERIFY2(!seen.contains(p.key), qPrintable(c.id + QLatin1Char(' ') + p.key));
                seen.insert(p.key);
                QVERIFY2(types.contains(p.type), qPrintable(c.id + QLatin1Char(' ') + p.type));
                if (p.type == QStringLiteral("choice")) {
                    QVERIFY2(!p.choices.isEmpty(), qPrintable(c.id + QLatin1Char(' ') + p.key));
                    if (!p.defaultValue.isEmpty()) {
                        QVERIFY2(p.choices.contains(p.defaultValue), qPrintable(c.id + QLatin1Char(' ') + p.key));
                    }
                }
            }
        }
    }
    void everyGroupListedAndNonEmpty() {
        QStringList groups = commandGroups();
        QVERIFY(groups.size() >= 8);
        QCOMPARE(groups.removeDuplicates(), 0);
        for (const QString& g : groups) {
            int n = 0;
            for (const CommandSpec& c : commandCatalog()) {
                n += c.group == g ? 1 : 0;
            }
            QVERIFY2(n > 0, qPrintable(g));
        }
        for (const CommandSpec& c : commandCatalog()) {
            QVERIFY2(groups.contains(c.group), qPrintable(c.id));
        }
    }
    void shellCommandsNeverEmitShellMetaFromValues() {
        // A hostile value must come out quoted, never as a second command.
        for (const CommandSpec& c : commandCatalog()) {
            if (c.mode != CommandMode::Shell) {
                continue;
            }
            QVariantMap v;
            for (const CommandParam& p : c.params) {
                if (p.type == QStringLiteral("text") || p.type == QStringLiteral("file") ||
                    p.type == QStringLiteral("dir")) {
                    v.insert(p.key, QStringLiteral("a; rm -rf /"));
                }
            }
            const ExpandResult r = expandCommand(c, v);
            if (!r.ok) {
                continue; // rejected outright is fine too
            }
            const QString joined = r.args.join(QLatin1Char(' '));
            if (joined.contains(QStringLiteral("rm -rf"))) {
                QVERIFY2(joined.contains(QStringLiteral("'a; rm -rf /'")), qPrintable(c.id));
            }
        }
    }
    void findCommandRoundTrip() {
        for (const CommandSpec& c : commandCatalog()) {
            const CommandSpec* f = findCommand(c.id);
            QVERIFY(f != nullptr);
            QCOMPARE(f->id, c.id);
        }
        QVERIFY(findCommand(QStringLiteral("no-such-command")) == nullptr);
    }
    void dangerousCommandsExist() {
        int n = 0;
        for (const CommandSpec& c : commandCatalog()) {
            n += c.dangerous ? 1 : 0;
        }
        QVERIFY(n > 0);
    }
};

QTEST_APPLESS_MAIN(TstCatalogIntegrity)
#include "tst_catalogintegrity.moc"
