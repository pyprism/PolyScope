import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyScope

// Live logcat with level/text/regex filters, pause, clear and save.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""

    signal result(string text, bool ok)

    spacing: 8

    LogcatModel {
        id: logcat
        serial: root.serial
    }

    function levelColor(level) {
        return level >= 4 ? Theme.danger : level === 3 ? Theme.warning : level <= 1 ? Theme.textMuted : Theme.text
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        AppButton {
            text: logcat.running ? qsTr("Stop") : qsTr("Start")
            primary: !logcat.running
            danger: logcat.running
            enabled: root.serial !== ""
            onClicked: logcat.running ? logcat.stop() : logcat.start()
        }
        AppButton {
            text: logcat.paused ? qsTr("Resume") : qsTr("Pause")
            enabled: logcat.running
            onClicked: logcat.paused = !logcat.paused
        }
        AppButton { text: qsTr("Clear view"); onClicked: logcat.clear() }
        AppButton { text: qsTr("Clear device"); enabled: root.serial !== ""; onClicked: logcat.clearDevice() }
        AppButton { text: qsTr("Save…"); onClicked: saveDialog.open() }
        Item { Layout.fillWidth: true }
        Label { text: qsTr("%1 lines").arg(list.count); color: Theme.textMuted }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Filter text")
            selectByMouse: true
            onTextChanged: logcat.filterText = text
        }
        AppCheckBox { text: qsTr("Regex"); onToggled: logcat.useRegex = checked }
        ComboBox {
            model: ["V", "D", "I", "W", "E", "F"]
            ToolTip.text: qsTr("Minimum level")
            ToolTip.visible: hovered
            onActivated: logcat.minLevel = model[currentIndex]
        }
        ComboBox {
            model: ["default", "main,system,crash", "events", "radio", "all"]
            onActivated: logcat.buffers = currentIndex === 0 ? "" : model[currentIndex]
        }
    }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 4
        color: Theme.window

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial === ""
            title: qsTr("No device selected")
            hint: qsTr("Pick an online device in the sidebar.")
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial !== ""
            clip: true
            model: logcat
            ScrollBar.vertical: ScrollBar {}
            property bool follow: true
            onMovementEnded: follow = atYEnd
            onCountChanged: if (follow) Qt.callLater(positionViewAtEnd)
            delegate: Text {
                required property string line
                required property int level
                width: ListView.view.width
                text: line
                color: root.levelColor(level)
                font.family: AppSettings.monoFamily
                font.pixelSize: 11
                wrapMode: Text.WrapAnywhere
            }
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Save visible log")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        onAccepted: {
            const path = decodeURIComponent(selectedFile.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, ""))
            const saved = logcat.save(path)
            root.result(saved ? qsTr("Saved %1").arg(path) : qsTr("Could not write %1").arg(path), saved)
        }
    }

    Component.onDestruction: logcat.stop()
}
