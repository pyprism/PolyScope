#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Builds scrcpy command lines from a flat option map. Pure: no process access.
//
// The option table below mirrors `scrcpy --help` (5.0.1). Every value is emitted
// as one "--flag=value" argument, so a user-supplied value can never become a
// separate flag. "extraArgs" is the only free-form escape hatch.
namespace polyscope::scrcpy {

enum class OptionKind { Bool, NegBool, Int, IntAny, Text, Choice, Bitrate, Path };

enum Mode { ModeDisplay = 1, ModeCamera = 2, ModeOtg = 4, ModeAll = 7 };
enum Requires { ReqNone = 0, ReqVideo = 1, ReqAudio = 2, ReqControl = 4 };

struct OptionSpec {
    QString key;   // key in the option map (and QML)
    QString flag;  // long flag without "--"; empty = handled specially
    QString group; // UI grouping
    OptionKind kind = OptionKind::Text;
    int modes = ModeAll;
    int requires_ = ReqNone;
    QStringList choices;
    QVariant defaultValue;
    QString platform; // "" = all, otherwise "linux"
};

const QList<OptionSpec>& optionSpecs();
QStringList optionGroups();
// Schema as QVariantList of maps (key, label, group, kind, choices, default, platform) for QML.
QVariantList optionSchema();

QVariantMap defaultOptions();
QStringList presetNames();
QVariantMap presetOptions(const QString& name);

// `serial` may be empty (scrcpy then picks the only device).
QStringList buildArguments(const QString& serial, const QVariantMap& options);

// Human-readable warnings about combinations that scrcpy will reject or ignore.
QStringList validateOptions(const QVariantMap& options);

// "scrcpy --a --b" for display / copy. Quotes only when needed.
QString commandLinePreview(const QString& program, const QStringList& args);

} // namespace polyscope::scrcpy
