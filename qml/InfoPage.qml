import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyScope

// Device overview (props, display, storage, memory, battery, network) with report export.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""
    property var sections: []
    property bool loading: false

    signal result(string text, bool ok)

    spacing: 10

    function reload() {
        if (serial === "")
            return
        loading = true
        Actions.overview(serial)
    }
    onSerialChanged: { sections = []; reload() }
    Component.onCompleted: reload()

    function reportText() {
        let out = "PolyScope device report: " + serial + "\n"
        for (const s of sections) {
            out += "\n== " + s.title + " ==\n"
            if (s.rows)
                out += s.rows.map(r => r.key + ": " + r.value).join("\n") + "\n"
            else
                out += s.text + "\n"
        }
        return out
    }

    Connections {
        target: Actions
        function onOverviewReady(s, list) {
            if (s !== root.serial)
                return
            root.sections = list
            root.loading = false
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        AppButton { text: qsTr("Refresh"); enabled: root.serial !== ""; onClicked: root.reload() }
        AppButton { text: qsTr("Screenshot"); enabled: root.serial !== ""; onClicked: Actions.screenshot(root.serial) }
        AppButton { text: qsTr("Export report…"); enabled: root.sections.length > 0; onClicked: exportDialog.open() }
        BusyIndicator { visible: root.loading; running: visible; Layout.preferredHeight: 24 }
    }

    EmptyState {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.serial === ""
        title: qsTr("No device selected")
        hint: qsTr("Pick an online device in the sidebar.")
    }

    ScrollView {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: root.serial !== ""
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: parent.width - 16
            spacing: 10
            Repeater {
                model: root.sections
                delegate: Card {
                    required property var modelData
                    Layout.fillWidth: true
                    SectionTitle { text: modelData.title }
                    Repeater {
                        model: modelData.rows || []
                        delegate: RowLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Label { text: modelData.key; color: Theme.textMuted; Layout.preferredWidth: 130 }
                            Label { text: modelData.value; color: Theme.text; Layout.fillWidth: true; wrapMode: Text.WrapAnywhere }
                        }
                    }
                    Label {
                        visible: !!modelData.text
                        Layout.fillWidth: true
                        text: modelData.text || ""
                        color: Theme.text
                        font.family: AppSettings.monoFamily
                        font.pixelSize: 11
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }

    FileDialog {
        id: exportDialog
        title: qsTr("Export device report")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "txt"
        nameFilters: ["Text (*.txt)", "JSON (*.json)"]
        onAccepted: {
            const path = decodeURIComponent(selectedFile.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, ""))
            const body = path.endsWith(".json") ? JSON.stringify({ serial: root.serial, sections: root.sections }, null, 2)
                                                : root.reportText()
            const saved = Actions.writeText(path, body)
            root.result(saved ? qsTr("Saved %1").arg(path) : qsTr("Could not write %1").arg(path), saved)
        }
    }
}
