import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Interactive `adb shell` session plus a raw adb command line.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""
    property string transcript
    property int historyIndex: -1
    property var history: AppSettings.commandHistory()

    signal result(string text, bool ok)

    spacing: 8

    ShellConsole {
        id: shell
        serial: root.serial
        onOutput: text => { root.transcript += text; Qt.callLater(() => view.cursorPosition = view.length) }
    }

    Connections {
        target: Commands
        function onFinished(requestId, commandId, s, ok, text, exitCode) {
            if (commandId === "raw" && requestId === root.rawId) {
                root.transcript += text + "\n"
                root.rawId = -1
            }
        }
    }
    property int rawId: -1

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        ComboBox {
            id: modeBox
            model: [qsTr("Interactive shell"), qsTr("Raw adb command")]
            onActivated: root.transcript = ""
        }
        AppButton {
            visible: modeBox.currentIndex === 0
            text: shell.running ? qsTr("Disconnect") : qsTr("Connect")
            primary: !shell.running
            enabled: root.serial !== ""
            onClicked: shell.running ? shell.stop() : shell.start()
        }
        AppButton { text: qsTr("Clear"); onClicked: root.transcript = "" }
        Item { Layout.fillWidth: true }
        ComboBox {
            id: snippetBox
            model: [qsTr("Snippets…")].concat(AppSettings.snippets())
            onActivated: if (currentIndex > 0) input.text = model[currentIndex]
        }
        AppButton {
            text: qsTr("Save as snippet")
            enabled: input.text.trim() !== ""
            onClicked: {
                const list = AppSettings.snippets()
                if (list.indexOf(input.text) < 0)
                    list.push(input.text)
                AppSettings.setSnippets(list)
                snippetBox.model = [qsTr("Snippets…")].concat(list)
            }
        }
    }

    Label {
        visible: modeBox.currentIndex === 1
        Layout.fillWidth: true
        text: qsTr("Enter adb arguments, e.g. “shell getprop ro.product.model” or “devices -l”. No shell expansion; quotes group words.")
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        font.pixelSize: 12
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        TextArea {
            id: view
            readOnly: true
            text: root.transcript
            font.family: AppSettings.monoFamily
            font.pixelSize: 12
            color: Theme.text
            selectByMouse: true
            wrapMode: TextArea.WrapAnywhere
            background: Rectangle { color: Theme.window; border.color: Theme.border; radius: 6 }
        }
    }

    TextField {
        id: input
        Layout.fillWidth: true
        font.family: AppSettings.monoFamily
        placeholderText: modeBox.currentIndex === 0 ? qsTr("Type a command and press Enter") : qsTr("adb arguments")
        selectByMouse: true
        enabled: root.serial !== "" || modeBox.currentIndex === 1
        Keys.onUpPressed: recall(1)
        Keys.onDownPressed: recall(-1)

        function recall(step) {
            const h = root.history
            if (h.length === 0)
                return
            root.historyIndex = Math.max(-1, Math.min(h.length - 1, root.historyIndex + step))
            text = root.historyIndex < 0 ? "" : h[root.historyIndex]
        }
        onAccepted: {
            const line = text.trim()
            if (line === "")
                return
            AppSettings.addCommandHistory(line)
            root.history = AppSettings.commandHistory()
            root.historyIndex = -1
            if (modeBox.currentIndex === 0) {
                if (!shell.running)
                    shell.start()
                shell.send(line)
            } else {
                root.transcript += "$ adb " + line + "\n"
                root.rawId = Commands.runRaw(root.serial, line)
            }
            text = ""
        }
    }
}
