#include "scrcpy/ScrcpyArgs.h"

#include <QProcess>
#include <QRegularExpression>

namespace polyscope::scrcpy {

namespace {

using K = OptionKind;

QString qs(const char* s) {
    return QString::fromUtf8(s);
}

QStringList L(std::initializer_list<const char*> items) {
    QStringList list;
    for (const char* item : items) {
        list << qs(item);
    }
    return list;
}

OptionSpec row(const char* key, const char* flag, const char* group, K kind,
               QVariant def = QVariant(), int modes = ModeAll, int req = ReqNone,
               QStringList choices = {}, const char* platform = "") {
    OptionSpec s;
    s.key = qs(key);
    s.flag = qs(flag);
    s.group = qs(group);
    s.kind = kind;
    s.defaultValue = def.isValid() ? def
                     : (kind == K::Bool)    ? QVariant(false)
                     : (kind == K::NegBool) ? QVariant(true)
                     : (kind == K::Int)     ? QVariant(0)
                                            : QVariant(QString());
    s.modes = modes;
    s.requires_ = req;
    s.choices = std::move(choices);
    s.platform = qs(platform);
    return s;
}

constexpr int D = ModeDisplay;
constexpr int DC = ModeDisplay | ModeCamera;
constexpr int V = ReqVideo;
constexpr int A = ReqAudio;
constexpr int C = ReqControl;

QList<OptionSpec> buildSpecs() {
    const QStringList hid = L({"disabled", "sdk", "uhid", "aoa"});
    return {
        // ---- Video
        row("video", "no-video", "Video", K::NegBool, QVariant(true), DC),
        row("videoSource", "", "Video", K::Choice, QVariant(QStringLiteral("display")), DC, V,
            L({"display", "camera"})),
        row("maxSize", "max-size", "Video", K::Int, {}, DC, V),
        row("videoBitRate", "video-bit-rate", "Video", K::Bitrate, {}, DC, V),
        row("maxFps", "max-fps", "Video", K::Int, {}, DC, V),
        row("videoCodec", "video-codec", "Video", K::Choice, {}, DC, V,
            L({"", "h264", "h265", "av1", "vp8", "vp9"})),
        row("videoEncoder", "video-encoder", "Video", K::Text, {}, DC, V),
        row("videoCodecOptions", "video-codec-options", "Video", K::Text, {}, DC, V),
        row("videoBuffer", "video-buffer", "Video", K::Int, {}, DC, V),
        row("displayOrientation", "display-orientation", "Video", K::Choice, {}, D, V,
            L({"", "0", "90", "180", "270", "flip0", "flip90", "flip180", "flip270"})),
        row("captureOrientation", "capture-orientation", "Video", K::Text, {}, DC, V),
        row("angle", "angle", "Video", K::Text, {}, ModeCamera | ModeDisplay, V),
        row("crop", "crop", "Video", K::Text, {}, D, V),
        row("displayId", "display-id", "Video", K::Int, {}, D, V),
        row("noDownsizeOnError", "no-downsize-on-error", "Video", K::Bool, {}, D, V),
        row("ignoreVideoEncoderConstraints", "ignore-video-encoder-constraints", "Video", K::Bool,
            {}, DC, V),
        row("hwdec", "hwdec", "Video", K::Text, {}, DC, V),
        row("renderDriver", "render-driver", "Video", K::Choice, {}, DC, V,
            L({"", "direct3d", "opengl", "opengles2", "opengles", "metal", "software"})),
        row("renderFit", "render-fit", "Video", K::Text, {}, DC, V),
        row("noMipmaps", "no-mipmaps", "Video", K::Bool, {}, DC, V),
        row("minSizeAlignment", "min-size-alignment", "Video", K::Int, {}, DC, V),
        row("noVideoPlayback", "no-video-playback", "Video", K::Bool, {}, DC, V),
        // ---- Virtual display
        row("newDisplay", "", "Virtual display", K::Bool, {}, D, V),
        row("newDisplaySize", "", "Virtual display", K::Text, {}, D, V),
        row("startApp", "start-app", "Virtual display", K::Text, {}, D, V),
        row("noVdDestroyContent", "no-vd-destroy-content", "Virtual display", K::Bool, {}, D, V),
        row("noVdSystemDecorations", "no-vd-system-decorations", "Virtual display", K::Bool, {}, D, V),
        row("displayImePolicy", "display-ime-policy", "Virtual display", K::Choice, {}, D, V,
            L({"", "local", "fallback", "hide"})),
        row("flexDisplay", "flex-display", "Virtual display", K::Bool, {}, D, V),
        // ---- Audio
        row("audio", "no-audio", "Audio", K::NegBool, QVariant(true), DC),
        row("audioCodec", "audio-codec", "Audio", K::Choice, {}, DC, A, L({"", "opus", "aac", "flac", "raw"})),
        row("audioEncoder", "audio-encoder", "Audio", K::Text, {}, DC, A),
        row("audioSource", "audio-source", "Audio", K::Choice, {}, DC, A,
            L({"", "output", "playback", "mic", "mic-unprocessed", "mic-camcorder",
               "mic-voice-recognition", "mic-voice-communication", "voice-call",
               "voice-call-uplink", "voice-call-downlink", "voice-performance"})),
        row("audioBitRate", "audio-bit-rate", "Audio", K::Bitrate, {}, DC, A),
        row("audioBuffer", "audio-buffer", "Audio", K::Int, {}, DC, A),
        row("audioOutputBuffer", "audio-output-buffer", "Audio", K::Int, {}, DC, A),
        row("audioCodecOptions", "audio-codec-options", "Audio", K::Text, {}, DC, A),
        row("audioDup", "audio-dup", "Audio", K::Bool, {}, DC, A),
        row("requireAudio", "require-audio", "Audio", K::Bool, {}, DC, A),
        row("noAudioPlayback", "no-audio-playback", "Audio", K::Bool, {}, DC, A),
        // ---- Camera
        row("cameraFacing", "camera-facing", "Camera", K::Choice, {}, ModeCamera, V,
            L({"", "front", "back", "external"})),
        row("cameraId", "camera-id", "Camera", K::Text, {}, ModeCamera, V),
        row("cameraSize", "camera-size", "Camera", K::Text, {}, ModeCamera, V),
        row("cameraAr", "camera-ar", "Camera", K::Text, {}, ModeCamera, V),
        row("cameraFps", "camera-fps", "Camera", K::Int, {}, ModeCamera, V),
        row("cameraHighSpeed", "camera-high-speed", "Camera", K::Bool, {}, ModeCamera, V),
        row("cameraTorch", "camera-torch", "Camera", K::Bool, {}, ModeCamera, V),
        row("cameraZoom", "camera-zoom", "Camera", K::Text, {}, ModeCamera, V),
        // ---- Input
        row("control", "no-control", "Input", K::NegBool, QVariant(true), DC),
        row("otg", "", "Input", K::Bool, {}, ModeAll),
        row("keyboard", "keyboard", "Input", K::Choice, {}, D | ModeOtg, C, hid),
        row("mouse", "mouse", "Input", K::Choice, {}, D | ModeOtg, C, hid),
        row("gamepad", "gamepad", "Input", K::Choice, {}, D | ModeOtg, C, L({"", "disabled", "uhid", "aoa"})),
        row("mouseBind", "mouse-bind", "Input", K::Text, {}, D, C),
        row("rawKeyEvents", "raw-key-events", "Input", K::Bool, {}, D, C),
        row("preferText", "prefer-text", "Input", K::Bool, {}, D, C),
        row("noKeyRepeat", "no-key-repeat", "Input", K::Bool, {}, D, C),
        row("noMouseHover", "no-mouse-hover", "Input", K::Bool, {}, D, C),
        row("legacyPaste", "legacy-paste", "Input", K::Bool, {}, D, C),
        row("clipboardSync", "no-clipboard-autosync", "Input", K::NegBool, QVariant(true), D, C),
        row("shortcutMod", "shortcut-mod", "Input", K::Text, {}, D, C),
        // ---- Device
        row("stayAwake", "stay-awake", "Device", K::Bool, {}, D, C),
        row("turnScreenOff", "turn-screen-off", "Device", K::Bool, {}, D, C),
        row("screenOffTimeout", "screen-off-timeout", "Device", K::Int, {}, D, C),
        row("showTouches", "show-touches", "Device", K::Bool, {}, D, C),
        row("powerOn", "no-power-on", "Device", K::NegBool, QVariant(true), D, C),
        row("powerOffOnClose", "power-off-on-close", "Device", K::Bool, {}, D, C),
        row("disableScreensaver", "disable-screensaver", "Device", K::Bool, {}, D, C),
        row("keepActive", "keep-active", "Device", K::Bool, {}, D, C),
        // ---- Window
        row("windowTitle", "window-title", "Window", K::Text),
        row("windowX", "window-x", "Window", K::IntAny, {}, ModeAll),
        row("windowY", "window-y", "Window", K::IntAny, {}, ModeAll),
        row("windowWidth", "window-width", "Window", K::Int, {}, ModeAll),
        row("windowHeight", "window-height", "Window", K::Int, {}, ModeAll),
        row("alwaysOnTop", "always-on-top", "Window", K::Bool),
        row("fullscreen", "fullscreen", "Window", K::Bool),
        row("borderless", "window-borderless", "Window", K::Bool),
        row("noWindow", "no-window", "Window", K::Bool, {}, DC),
        row("noPlayback", "no-playback", "Window", K::Bool, {}, DC),
        row("noWindowAspectRatioLock", "no-window-aspect-ratio-lock", "Window", K::Bool, {}, DC),
        row("backgroundColor", "background-color", "Window", K::Text, {}, DC),
        row("printFps", "print-fps", "Window", K::Bool, {}, DC),
        row("noTerminalTitle", "no-terminal-title", "Window", K::Bool, {}, DC),
        // ---- Recording
        row("record", "", "Recording", K::Bool, {}, DC),
        row("recordPath", "", "Recording", K::Path, {}, DC),
        row("recordFormat", "record-format", "Recording", K::Choice, {}, DC, ReqNone, L({"", "mp4", "mkv", "m4a", "mka", "opus", "aac", "flac", "wav"})),
        row("recordOrientation", "record-orientation", "Recording", K::Choice, {}, D, V,
            L({"", "0", "90", "180", "270", "flip0", "flip90", "flip180", "flip270"})),
        row("timeLimit", "time-limit", "Recording", K::Int, {}, DC),
        row("pauseOnExit", "pause-on-exit", "Recording", K::Choice, {}, DC, ReqNone,
            L({"", "true", "false", "if-error"})),
        // ---- Connection
        row("forceAdbForward", "force-adb-forward", "Connection", K::Bool, {}, DC),
        row("tunnelHost", "tunnel-host", "Connection", K::Text, {}, DC),
        row("tunnelPort", "tunnel-port", "Connection", K::Int, {}, DC),
        row("port", "port", "Connection", K::Text, {}, DC),
        row("killAdbOnClose", "kill-adb-on-close", "Connection", K::Bool, {}, DC),
        row("pushTarget", "push-target", "Connection", K::Text, {}, D, C),
        row("noCleanup", "no-cleanup", "Connection", K::Bool, {}, DC),
        row("v4l2Sink", "v4l2-sink", "Connection", K::Text, {}, DC, V, {}, "linux"),
        row("v4l2Buffer", "v4l2-buffer", "Connection", K::Int, {}, DC, V, {}, "linux"),
        row("verbosity", "verbosity", "Connection", K::Choice, {}, ModeAll, ReqNone,
            L({"", "verbose", "debug", "info", "warn", "error"})),
        // ---- Advanced
        row("extraArgs", "", "Advanced", K::Text, {}, ModeAll),
    };
}

QString humanize(const QString& key) {
    QString out;
    for (const QChar c : key) {
        if (c.isUpper() && !out.isEmpty()) {
            out += QLatin1Char(' ');
            out += c.toLower();
        } else {
            out += out.isEmpty() ? c.toUpper() : c;
        }
    }
    return out;
}

QString kindName(OptionKind kind) {
    switch (kind) {
    case K::Bool:
    case K::NegBool:
        return QStringLiteral("bool");
    case K::Int:
    case K::IntAny:
        return QStringLiteral("int");
    case K::Choice:
        return QStringLiteral("choice");
    case K::Bitrate:
        return QStringLiteral("bitrate");
    case K::Path:
        return QStringLiteral("path");
    case K::Text:
        break;
    }
    return QStringLiteral("text");
}

bool validBitrate(const QString& value) {
    static const QRegularExpression re(QStringLiteral("^\\d+[KkMm]?$"));
    return re.match(value).hasMatch();
}

QVariantMap merged(const QVariantMap& options) {
    QVariantMap o = defaultOptions();
    for (auto it = options.cbegin(); it != options.cend(); ++it) {
        o.insert(it.key(), it.value());
    }
    return o;
}

QString text(const QVariantMap& o, const QString& key) {
    return o.value(key).toString().trimmed();
}

} // namespace

const QList<OptionSpec>& optionSpecs() {
    static const QList<OptionSpec> specs = buildSpecs();
    return specs;
}

QStringList optionGroups() {
    QStringList groups;
    for (const OptionSpec& s : optionSpecs()) {
        if (!groups.contains(s.group)) {
            groups.append(s.group);
        }
    }
    return groups;
}

QVariantList optionSchema() {
    QVariantList list;
    for (const OptionSpec& s : optionSpecs()) {
        QVariantMap m;
        m[QStringLiteral("key")] = s.key;
        m[QStringLiteral("label")] = humanize(s.key);
        m[QStringLiteral("group")] = s.group;
        m[QStringLiteral("kind")] = kindName(s.kind);
        m[QStringLiteral("choices")] = s.choices;
        m[QStringLiteral("default")] = s.defaultValue;
        m[QStringLiteral("platform")] = s.platform;
        m[QStringLiteral("flag")] = s.flag;
        list.append(m);
    }
    return list;
}

QVariantMap defaultOptions() {
    QVariantMap map;
    for (const OptionSpec& s : optionSpecs()) {
        map.insert(s.key, s.defaultValue);
    }
    return map;
}

QStringList presetNames() {
    return {QStringLiteral("Balanced"),       QStringLiteral("Low latency"),
            QStringLiteral("High quality"),   QStringLiteral("Screen off"),
            QStringLiteral("Record"),         QStringLiteral("Virtual display"),
            QStringLiteral("Camera"),         QStringLiteral("Audio only"),
            QStringLiteral("OTG keyboard & mouse")};
}

QVariantMap presetOptions(const QString& name) {
    QVariantMap o = defaultOptions();
    if (name == QStringLiteral("Low latency")) {
        o[QStringLiteral("maxSize")] = 1024;
        o[QStringLiteral("videoBitRate")] = QStringLiteral("4M");
        o[QStringLiteral("maxFps")] = 60;
        o[QStringLiteral("audio")] = false;
    } else if (name == QStringLiteral("High quality")) {
        o[QStringLiteral("videoBitRate")] = QStringLiteral("24M");
        o[QStringLiteral("maxFps")] = 60;
        o[QStringLiteral("videoCodec")] = QStringLiteral("h265");
    } else if (name == QStringLiteral("Screen off")) {
        o[QStringLiteral("turnScreenOff")] = true;
        o[QStringLiteral("stayAwake")] = true;
        o[QStringLiteral("powerOffOnClose")] = true;
    } else if (name == QStringLiteral("Record")) {
        o[QStringLiteral("record")] = true;
    } else if (name == QStringLiteral("Virtual display")) {
        o[QStringLiteral("newDisplay")] = true;
        o[QStringLiteral("newDisplaySize")] = QStringLiteral("1920x1080/420");
    } else if (name == QStringLiteral("Camera")) {
        o[QStringLiteral("videoSource")] = QStringLiteral("camera");
        o[QStringLiteral("cameraFacing")] = QStringLiteral("back");
    } else if (name == QStringLiteral("Audio only")) {
        o[QStringLiteral("video")] = false;
        o[QStringLiteral("control")] = false;
    } else if (name == QStringLiteral("OTG keyboard & mouse")) {
        o[QStringLiteral("otg")] = true;
    }
    return o;
}

QStringList buildArguments(const QString& serial, const QVariantMap& options) {
    QVariantMap o = merged(options);

    const bool otg = o.value(QStringLiteral("otg")).toBool();
    const bool camera =
        !otg && text(o, QStringLiteral("videoSource")) == QStringLiteral("camera");
    const int mode = otg ? ModeOtg : camera ? ModeCamera : ModeDisplay;
    if (camera) {
        o[QStringLiteral("control")] = false; // camera mirroring has no device control
    }

    const bool video = !otg && o.value(QStringLiteral("video")).toBool();
    const bool audio = !otg && o.value(QStringLiteral("audio")).toBool();
    const bool control = !otg && mode == ModeDisplay && video && o.value(QStringLiteral("control")).toBool();

    QStringList args;
    if (!serial.trimmed().isEmpty()) {
        args << QStringLiteral("--serial=%1").arg(serial.trimmed());
    }
    if (otg) {
        args << QStringLiteral("--otg");
    }
    if (camera) {
        args << QStringLiteral("--video-source=camera");
    }

    for (const OptionSpec& s : optionSpecs()) {
        if (s.flag.isEmpty() || !(s.modes & mode)) {
            continue;
        }
        if ((s.requires_ & ReqVideo) && !video) {
            continue;
        }
        if ((s.requires_ & ReqAudio) && !audio) {
            continue;
        }
        if ((s.requires_ & ReqControl) && !control && !otg) {
            continue;
        }
        const QVariant v = o.value(s.key);
        const QString flag = QStringLiteral("--") + s.flag;
        switch (s.kind) {
        case K::Bool:
            if (v.toBool()) {
                args << flag;
            }
            break;
        case K::NegBool:
            if (!v.toBool()) {
                args << flag;
            }
            break;
        case K::Int: {
            bool ok = false;
            const int n = v.toInt(&ok);
            if (ok && n > 0) {
                args << QStringLiteral("%1=%2").arg(flag).arg(n);
            }
            break;
        }
        case K::IntAny: {
            const QString t = v.toString().trimmed();
            bool ok = false;
            const int n = t.toInt(&ok);
            if (ok) {
                args << QStringLiteral("%1=%2").arg(flag).arg(n);
            }
            break;
        }
        case K::Bitrate:
            if (validBitrate(v.toString().trimmed())) {
                args << QStringLiteral("%1=%2").arg(flag, v.toString().trimmed());
            }
            break;
        case K::Text:
        case K::Choice:
        case K::Path: {
            const QString t = v.toString().trimmed();
            if (!t.isEmpty()) {
                args << QStringLiteral("%1=%2").arg(flag, t);
            }
            break;
        }
        }
    }

    // Specially-handled options.
    if (mode == ModeDisplay && video && o.value(QStringLiteral("newDisplay")).toBool()) {
        const QString size = text(o, QStringLiteral("newDisplaySize"));
        args << (size.isEmpty() ? QStringLiteral("--new-display")
                                : QStringLiteral("--new-display=%1").arg(size));
    }
    if (mode != ModeOtg && o.value(QStringLiteral("record")).toBool()) {
        const QString path = text(o, QStringLiteral("recordPath"));
        if (!path.isEmpty()) {
            args << QStringLiteral("--record=%1").arg(path);
        }
    }
    args << QProcess::splitCommand(text(o, QStringLiteral("extraArgs")));
    return args;
}

QStringList validateOptions(const QVariantMap& options) {
    const QVariantMap o = merged(options);
    QStringList warnings;
    const bool otg = o.value(QStringLiteral("otg")).toBool();
    if (o.value(QStringLiteral("record")).toBool() && text(o, QStringLiteral("recordPath")).isEmpty()) {
        warnings << QStringLiteral("Recording is enabled but no output file is set.");
    }
    const QString vbr = text(o, QStringLiteral("videoBitRate"));
    if (!vbr.isEmpty() && !validBitrate(vbr)) {
        warnings << QStringLiteral("Video bit rate must look like 8M, 500K or 8000000.");
    }
    if (!otg && !o.value(QStringLiteral("video")).toBool() && !o.value(QStringLiteral("audio")).toBool()) {
        warnings << QStringLiteral("Both video and audio are disabled; nothing will be shown.");
    }
    if (!otg && text(o, QStringLiteral("videoSource")) == QStringLiteral("camera") &&
        o.value(QStringLiteral("newDisplay")).toBool()) {
        warnings << QStringLiteral("Virtual display is ignored in camera mode.");
    }
    if (otg && o.value(QStringLiteral("record")).toBool()) {
        warnings << QStringLiteral("OTG mode cannot record.");
    }
    return warnings;
}

QString commandLinePreview(const QString& program, const QStringList& args) {
    auto quote = [](const QString& a) {
        static const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9_@%+=:,./-]+$"));
        if (safe.match(a).hasMatch()) {
            return a;
        }
        QString e = a;
        e.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        return QLatin1Char('\'') + e + QLatin1Char('\'');
    };
    QStringList parts{quote(program)};
    for (const QString& a : args) {
        parts << quote(a);
    }
    return parts.join(QLatin1Char(' '));
}

} // namespace polyscope::scrcpy
