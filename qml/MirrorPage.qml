import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// scrcpy launcher: presets, every option from the generated schema, command preview, log.
ColumnLayout {
    id: root

    property var targets: []
    property var options: ({})
    property var schema: Scrcpy.schema()
    property var groups: Scrcpy.groups()
    property string group: groups.length ? groups[0] : ""
    readonly property string serial: targets.length ? targets[0] : ""
    readonly property bool running: Scrcpy.runningSerials.indexOf(serial) >= 0

    signal result(string text, bool ok)

    spacing: 10

    function load() {
        // Stored options only hold what the user changed; show defaults for the rest.
        options = Object.assign({}, Scrcpy.defaults(), serial ? AppSettings.scrcpyOptions(serial) : {})
    }
    onSerialChanged: load()
    Component.onCompleted: load()

    // ---- Presets: show which one the current options correspond to ("Custom" when none).
    readonly property var builtinPresets: Scrcpy.presetNames()
    property var presetNames: builtinPresets.concat(AppSettings.userPresetNames())

    function presetOptions(name) {
        const builtin = builtinPresets.indexOf(name) >= 0
        return Object.assign({}, Scrcpy.defaults(), builtin ? Scrcpy.preset(name) : AppSettings.userPreset(name))
    }
    function sameOptions(a, b) {
        for (const k of Object.keys(Object.assign({}, a, b))) {
            const x = a[k], y = b[k]
            // Unset and empty/false are the same thing to scrcpy.
            if ((x === undefined || x === "" || x === false || x === 0 ? "" : String(x)) !==
                (y === undefined || y === "" || y === false || y === 0 ? "" : String(y)))
                return false
        }
        return true
    }
    function activePreset() {
        for (const name of presetNames)
            if (sameOptions(options, presetOptions(name)))
                return name
        return ""
    }
    function syncPreset() { presetBox.currentIndex = presetNames.indexOf(activePreset()) }
    onOptionsChanged: syncPreset()
    Connections {
        target: AppSettings
        function onUserPresetsChanged() {
            root.presetNames = root.builtinPresets.concat(AppSettings.userPresetNames())
            root.syncPreset()
        }
    }

    function setOption(key, value) {
        const copy = Object.assign({}, options)
        copy[key] = value
        options = copy
        if (serial)
            AppSettings.setScrcpyOptions(serial, copy)
    }

    Connections {
        target: Scrcpy
        function onFinished(s, code, message) {
            if (targets.indexOf(s) >= 0 && code !== 0)
                root.result(qsTr("scrcpy exited: %1").arg(message), false)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 8

        AppButton {
            text: root.running ? qsTr("Stop mirroring") : qsTr("Start mirroring")
            primary: !root.running
            danger: root.running
            enabled: Tools.scrcpyReady && root.targets.length > 0
            onClicked: {
                for (const s of root.targets) {
                    if (root.running) {
                        Scrcpy.stop(s)
                    } else {
                        const err = Scrcpy.start(s, AppSettings.scrcpyOptions(s))
                        if (err)
                            root.result(err, false)
                    }
                }
            }
        }
        Label { text: qsTr("Preset"); color: Theme.textMuted }
        ComboBox {
            id: presetBox
            model: root.presetNames
            currentIndex: -1
            displayText: currentIndex >= 0 ? currentText : qsTr("Custom")
            onActivated: {
                const merged = root.presetOptions(root.presetNames[currentIndex])
                root.options = merged
                if (root.serial)
                    AppSettings.setScrcpyOptions(root.serial, merged)
            }
        }
        AppButton {
            visible: root.builtinPresets.indexOf(root.activePreset()) < 0 && root.activePreset() !== ""
            text: qsTr("Delete preset")
            onClicked: AppSettings.deleteUserPreset(root.activePreset())
        }
        AppButton {
            text: qsTr("Save preset…")
            onClicked: savePreset.open()
        }
        AppButton {
            text: qsTr("Reset")
            onClicked: {
                if (root.serial)
                    AppSettings.resetScrcpyOptions(root.serial)
                root.load()
            }
        }
        Item { Layout.fillWidth: true }
        AppButton { text: qsTr("Shortcuts"); onClicked: shortcuts.open() }
    }

    Label {
        Layout.fillWidth: true
        visible: !Tools.scrcpyReady
        text: qsTr("scrcpy not available: %1").arg(Tools.scrcpyError)
        color: Theme.danger
        wrapMode: Text.WordWrap
    }
    Repeater {
        model: Scrcpy.warnings(root.options)
        delegate: Label {
            required property string modelData
            Layout.fillWidth: true
            text: "⚠ " + modelData
            color: Theme.warning
            wrapMode: Text.WordWrap
        }
    }

    // Scrolls sideways when the window is too narrow for every group tab.
    Flickable {
        Layout.fillWidth: true
        implicitHeight: tabs.implicitHeight
        contentWidth: tabs.implicitWidth
        contentHeight: tabs.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { height: 4 }

        TabBar {
            id: tabs
            // Arrow keys change currentIndex without a click; keep the shown group in step.
            onCurrentIndexChanged: if (currentIndex >= 0 && currentIndex < root.groups.length) root.group = root.groups[currentIndex]
            background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border } }
            Repeater {
                model: root.groups
                AppTabButton {
                    required property string modelData
                    text: modelData
                    width: implicitWidth
                    onClicked: root.group = modelData
                }
            }
        }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        clip: true
        contentWidth: availableWidth

        GridLayout {
            width: parent.width - 16
            columns: Math.max(1, Math.floor(width / 280))
            columnSpacing: 16
            rowSpacing: 10

            Repeater {
                model: root.schema.filter(s => s.group === root.group)
                delegate: ParamField {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignTop
                    spec: modelData
                    value: root.options[modelData.key]
                    onEdited: v => root.setOption(modelData.key, v)
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        text: Scrcpy.commandLine(root.serial, root.options)
        color: Theme.textMuted
        font.family: AppSettings.monoFamily
        font.pixelSize: 11
        wrapMode: Text.WrapAnywhere
    }

    Card {
        Layout.fillWidth: true
        Layout.preferredHeight: 110
        visible: logText.text.length > 0
        padding: 8
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: logText
                readOnly: true
                font.family: AppSettings.monoFamily
                font.pixelSize: 11
                color: Theme.text
                background: null
                wrapMode: TextArea.WrapAnywhere
                // Assigned (not bound) so appends do not reset it; refreshed on device switch too.
                Component.onCompleted: text = root.serial ? Scrcpy.log(root.serial) : ""
                Connections {
                    target: root
                    function onSerialChanged() { logText.text = root.serial ? Scrcpy.log(root.serial) : "" }
                }
                Connections {
                    target: Scrcpy
                    function onLogAppended(s, t) {
                        if (s === root.serial)
                            logText.text = Scrcpy.log(s)
                    }
                }
            }
        }
    }

    AppDialog {
        id: savePreset
        title: qsTr("Save preset")
        standardButtons: Dialog.Save | Dialog.Cancel
        contentItem: TextField { id: presetName; placeholderText: qsTr("Preset name"); selectByMouse: true }
        onAccepted: {
            if (presetName.text.trim() !== "")
                AppSettings.saveUserPreset(presetName.text.trim(), root.options)
            presetName.text = ""
        }
    }

    AppDialog {
        id: shortcuts
        title: qsTr("scrcpy window shortcuts")
        standardButtons: Dialog.Close
        contentItem: Label {
            color: Theme.text
            font.family: AppSettings.monoFamily
            text: qsTr("MOD = Alt (Win/Linux) or Cmd (macOS), left side\n\n"
                + "MOD+f   fullscreen\n"
                + "MOD+r   rotate display\n"
                + "MOD+g   resize to 1:1\n"
                + "MOD+w   resize to remove black borders\n"
                + "MOD+h   Home        MOD+b   Back\n"
                + "MOD+s   App switch  MOD+n   Notifications\n"
                + "MOD+p   Power       MOD+o   Screen off\n"
                + "MOD+Up/Down   volume\n"
                + "MOD+c / MOD+v   copy / paste\n"
                + "MOD+i   toggle FPS counter\n"
                + "Drag APK onto window to install; other files are pushed.")
        }
    }
}
