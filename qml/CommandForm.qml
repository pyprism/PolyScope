import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Form for one catalogue command: params, live command preview, Run, output.
// Runs on every serial in `targets`; device-less (Host) commands run once.
ColumnLayout {
    id: root

    property string commandId
    property var targets: []
    property var spec: Commands.command(commandId)
    property var values: ({})
    property string output
    property bool running: false
    property var pending: ({})

    readonly property var firstTarget: targets.length > 0 ? targets[0] : ""
    readonly property bool canRun: spec.id !== undefined && !running && Tools.adbReady
                                   && (!spec.needsDevice || targets.length > 0)

    signal result(string text, bool ok)

    spacing: 10

    function resetValues() {
        const v = {}
        for (const p of (spec.params || []))
            v[p.key] = p.default
        values = v
        output = ""
        pending = ({}) // results of the previous command must not land in this one
        running = false
    }

    onCommandIdChanged: Qt.callLater(resetValues)
    Component.onCompleted: resetValues()

    function run() {
        const serials = spec.needsDevice ? targets : [""]
        if (spec.dangerous) {
            confirm.open()
            return
        }
        execute(serials)
    }

    function execute(serials) {
        running = true
        output = ""
        pending = {}
        let count = 0
        for (const s of serials) {
            const id = Commands.run(commandId, s, values)
            if (id > 0) {
                pending[id] = s
                count++
            }
        }
        if (count === 0)
            running = false
    }

    Connections {
        target: Commands
        function onFinished(requestId, commandId, serial, ok, text, exitCode) {
            if (!(requestId in root.pending) || commandId !== root.commandId)
                return
            delete root.pending[requestId]
            const prefix = root.targets.length > 1 && serial ? "[" + serial + "] " : ""
            root.output += prefix + text.trim() + "\n"
            // Say which command ran and what it printed; a bare "OK" tells the user nothing.
            const firstLine = text.trim().split("\n")[0].slice(0, 120)
            const status = ok ? qsTr("done") : qsTr("failed (exit %1)").arg(exitCode)
            root.result(prefix + root.spec.title + ": " + status + (firstLine ? " — " + firstLine : ""), ok)
            if (Object.keys(root.pending).length === 0)
                root.running = false
        }
    }

    Label {
        Layout.fillWidth: true
        text: root.spec.title || ""
        color: Theme.text
        font.pixelSize: 16
        font.weight: Font.DemiBold
        wrapMode: Text.WordWrap
    }
    RowLayout {
        spacing: 6
        Badge { text: root.spec.group || ""; tone: Theme.accent }
        Badge { visible: !!root.spec.dangerous; text: qsTr("Dangerous"); tone: Theme.danger }
        Badge { visible: root.spec.needsDevice === false; text: qsTr("Host"); tone: Theme.textMuted }
    }

    Repeater {
        model: root.spec.params || []
        delegate: ParamField {
            required property var modelData
            Layout.fillWidth: true
            spec: modelData
            value: root.values[modelData.key]
            onEdited: v => {
                const copy = Object.assign({}, root.values)
                copy[modelData.key] = v
                root.values = copy
            }
        }
    }

    Label {
        Layout.fillWidth: true
        text: root.spec.id !== undefined ? Commands.preview(root.commandId, root.firstTarget, root.values) : ""
        color: Theme.textMuted
        font.family: AppSettings.monoFamily
        font.pixelSize: 12
        wrapMode: Text.WrapAnywhere
    }

    RowLayout {
        AppButton {
            text: root.running ? qsTr("Running…") : (root.targets.length > 1 && root.spec.needsDevice
                                                      ? qsTr("Run on %1 devices").arg(root.targets.length) : qsTr("Run"))
            primary: !root.spec.dangerous
            danger: !!root.spec.dangerous
            enabled: root.canRun
            onClicked: root.run()
        }
        Label {
            visible: root.spec.needsDevice && root.targets.length === 0
            text: qsTr("Select a device first.")
            color: Theme.warning
        }
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 240
        visible: root.output.length > 0

        TextArea {
            readOnly: true
            text: root.output
            selectByMouse: true
            font.family: AppSettings.monoFamily
            font.pixelSize: 12
            wrapMode: TextArea.WrapAnywhere
            color: Theme.text
            background: Rectangle { color: Theme.window; border.color: Theme.border; radius: 6 }
        }
    }

    AppDialog {
        id: confirm
        title: qsTr("Confirm")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: WrappedLabel {
            text: qsTr("“%1” can change or erase data on %2. Continue?")
                .arg(root.spec.title || "")
                .arg(root.targets.length > 1 ? qsTr("%1 devices").arg(root.targets.length) : root.firstTarget || qsTr("the host"))
        }
        onAccepted: root.execute(root.spec.needsDevice ? root.targets : [""])
    }
}
