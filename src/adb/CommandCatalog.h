#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// Data-driven catalogue of adb operations. Each entry is a template plus typed
// parameters, so the UI can render a form for it and tests can check the exact
// argv produced. This is what gives the app coverage of the whole adb surface
// without a hand-written screen per command.
namespace polyscope::adb {

enum class CommandMode {
    Host,  // `adb <args>`            (no device selected)
    Device, // `adb -s SERIAL <args>`
    Shell   // `adb -s SERIAL shell <command string>`
};

struct CommandParam {
    QString key;
    QString label;
    QString type = QStringLiteral("text"); // text int choice file files dir bool
    QString defaultValue;
    QStringList choices;
    QString flag;     // for {*key} expansion: "-r", "--user"
    QString placeholder;
    bool required = true;
};

struct CommandSpec {
    QString id;
    QString group;
    QString title;
    QString description;
    CommandMode mode = CommandMode::Shell;
    QStringList tmpl; // Host/Device: argv tokens. Shell: one command string.
    QList<CommandParam> params;
    bool dangerous = false; // UI should confirm
    int timeoutMs = 30000;  // <= 0: no timeout

    bool needsDevice() const { return mode != CommandMode::Host; }
};

struct ExpandResult {
    bool ok = false;
    QString error;
    QStringList args; // adb arguments WITHOUT the leading "-s SERIAL"
};

const QList<CommandSpec>& commandCatalog();
const CommandSpec* findCommand(const QString& id);
QStringList commandGroups();

// Template syntax:
//   {key}   value, quoted for the shell in Shell mode; a required param left empty is an error
//   {!key}  raw value in Shell mode; only [A-Za-z0-9_.:/@+,=-] accepted
//   {*key}  flag form: bool -> flag when true; other -> "flag value" when non-empty
// Params of type "files" hold newline-separated paths and expand to several argv entries.
ExpandResult expandCommand(const CommandSpec& spec, const QVariantMap& values);

} // namespace polyscope::adb
