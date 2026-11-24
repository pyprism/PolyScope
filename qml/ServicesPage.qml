import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// System services (binder / dumpsys) and running app services; dump, stop, force-stop, disable.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""

    signal result(string text, bool ok)

    spacing: 8

    ServicesModel {
        id: services
        serial: root.serial
        onOperationFinished: (op, ok, message) => root.result(op + ": " + message, ok)
        onDumpReady: (name, text) => dumpWindow.present(name, text)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        ComboBox {
            Layout.preferredWidth: 250
            textRole: "label"
            valueRole: "value"
            model: [
                { label: qsTr("Binder services (service list)"), value: "binder" },
                { label: qsTr("Dumpsys services"), value: "dumpsys" },
                { label: qsTr("Running app services"), value: "running" }
            ]
            onActivated: services.mode = currentValue
        }
        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Filter")
            selectByMouse: true
            onTextChanged: services.filter = text
        }
        Label { text: qsTr("%1 total").arg(services.totalCount); color: Theme.textMuted }
        AppButton { text: qsTr("Refresh"); primary: true; onClicked: services.refresh() }
    }
    BusyIndicator { visible: services.busy; running: visible; Layout.preferredHeight: 24 }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 4

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial === ""
            title: qsTr("No device selected")
            hint: qsTr("Pick an online device in the sidebar.")
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial !== ""
            clip: true
            model: services
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property string name
                required property string detail
                required property string packageName
                required property string component
                width: ListView.view.width
                height: 36
                contentItem: RowLayout {
                    Label { Layout.fillWidth: true; text: row.name; color: Theme.text; elide: Text.ElideMiddle }
                    Label { text: row.detail; color: Theme.textMuted; elide: Text.ElideRight; Layout.maximumWidth: 280 }
                }
                onClicked: menu.popup()
                Menu {
                    id: menu
                    MenuItem { text: qsTr("Dump"); onTriggered: services.dump(row.name) }
                    MenuItem { enabled: row.component !== ""; text: qsTr("Stop service"); onTriggered: services.stopService(row.component) }
                    MenuItem { enabled: row.packageName !== ""; text: qsTr("Force stop package"); onTriggered: services.forceStopPackage(row.packageName) }
                    MenuItem { enabled: row.packageName !== ""; text: qsTr("Disable package…"); onTriggered: { disablePkg = row.packageName; disableDialog.open() } }
                }
            }
        }
    }

    property string disablePkg

    AppDialog {
        id: disableDialog
        title: qsTr("Disable package")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: WrappedLabel { text: qsTr("Disable %1 for the current user?").arg(root.disablePkg) }
        onAccepted: services.disablePackage(root.disablePkg)
    }

    TextWindow { id: dumpWindow }
}
