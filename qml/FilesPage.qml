import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyScope

// File browser: navigate, pull, push (button or drag-and-drop), delete, mkdir, rename.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""

    signal result(string text, bool ok)

    spacing: 8

    FileBrowserModel {
        id: files
        serial: root.serial
        onOperationFinished: (ok, message) => root.result(message, ok)
    }

    function pullTo(row) {
        pullRow = row
        pullDir.text = AppSettings.screenshotDir
        pullDialog.open()
    }
    property int pullRow: -1

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        AppButton { text: "↑"; onClicked: files.up(); ToolTip.text: qsTr("Parent folder"); ToolTip.visible: hovered }
        TextField {
            id: pathEdit
            Layout.fillWidth: true
            text: files.path
            selectByMouse: true
            onAccepted: files.cd(text)
        }
        AppButton { text: qsTr("Refresh"); onClicked: files.refresh() }
        AppButton { text: qsTr("New folder"); onClicked: mkdirDialog.open() }
        AppButton { text: qsTr("Push files…"); primary: true; onClicked: pushPicker.open() }
    }

    Label {
        Layout.fillWidth: true
        visible: files.error.length > 0
        text: files.error
        color: Theme.danger
        wrapMode: Text.WordWrap
    }
    BusyIndicator { visible: files.busy; running: visible; Layout.preferredHeight: 24 }

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
            model: files
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: rowItem
                required property int index
                required property string name
                required property bool isDir
                required property bool isLink
                required property string sizeText
                required property string modified
                required property string permissions
                width: ListView.view.width
                height: 34
                onDoubleClicked: files.open(index)
                contentItem: RowLayout {
                    spacing: 10
                    Label { text: rowItem.isDir ? "📁" : "📄" }
                    Label {
                        Layout.fillWidth: true
                        text: rowItem.name + (rowItem.isLink ? "  →" : "")
                        color: Theme.text
                        elide: Text.ElideMiddle
                    }
                    Label { text: rowItem.isDir ? "" : rowItem.sizeText; color: Theme.textMuted; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight }
                    Label { text: rowItem.modified; color: Theme.textMuted; Layout.preferredWidth: 130 }
                    Label { text: rowItem.permissions; color: Theme.textMuted; font.family: AppSettings.monoFamily; Layout.preferredWidth: 90 }
                }
                TapHandler { acceptedButtons: Qt.RightButton; onTapped: menu.popup() }
                TapHandler { acceptedButtons: Qt.LeftButton; onTapped: rowItem.ListView.view.currentIndex = rowItem.index }
                Menu {
                    id: menu
                    MenuItem { text: rowItem.isDir ? qsTr("Open") : qsTr("Open / follow"); onTriggered: files.open(rowItem.index) }
                    MenuItem { text: qsTr("Pull to computer…"); onTriggered: { root.pullName = rowItem.name; root.pullTo(rowItem.index) } }
                    MenuItem { text: qsTr("Rename…"); onTriggered: { renameRow = rowItem.index; renameOld = rowItem.name; renameField.text = rowItem.name; renameDialog.open() } }
                    MenuItem { text: qsTr("Delete"); onTriggered: { deleteRow = rowItem.index; deleteName = rowItem.name; deleteDialog.open() } }
                }
            }
        }
    }

    Label {
        text: qsTr("Drop files here to push them into the current folder.")
        color: Theme.textMuted
        font.pixelSize: 11
    }

    // Called by the window-level DropArea in Main.qml.
    function pushUrls(urls) {
        files.push(urls.map(u => decodeURIComponent(u.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, ""))))
    }

    // The listing can refresh while a dialog is open; never act on a different file than the one shown.
    function rowStillShows(row, name) {
        if (files.nameAt(row) === name)
            return true
        root.result(qsTr("The folder changed while the dialog was open. Please try again."), false)
        return false
    }

    property int renameRow: -1
    property string renameOld
    property string pullName
    property int deleteRow: -1
    property string deleteName

    FileDialog {
        id: pushPicker
        title: qsTr("Choose files to push")
        fileMode: FileDialog.OpenFiles
        onAccepted: files.push(selectedFiles.map(u => decodeURIComponent(u.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, ""))))
    }

    AppDialog {
        id: pullDialog
        title: qsTr("Pull to…")
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: PathField { id: pullDir; folder: true }
        onAccepted: { if (root.rowStillShows(root.pullRow, root.pullName)) files.pull(root.pullRow, pullDir.text) }
    }
    AppDialog {
        id: mkdirDialog
        title: qsTr("New folder")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: TextField { id: mkdirName; placeholderText: qsTr("Folder name"); selectByMouse: true }
        onOpened: mkdirName.forceActiveFocus()
        onAccepted: { files.mkdir(mkdirName.text); mkdirName.text = "" }
    }
    AppDialog {
        id: renameDialog
        title: qsTr("Rename")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: TextField { id: renameField; selectByMouse: true }
        onOpened: { renameField.forceActiveFocus(); renameField.selectAll() }
        onAccepted: { if (root.rowStillShows(root.renameRow, root.renameOld)) files.rename(root.renameRow, renameField.text) }
    }
    AppDialog {
        id: deleteDialog
        title: qsTr("Delete")
        standardButtons: Dialog.Yes | Dialog.Cancel
        contentItem: WrappedLabel { text: qsTr("Delete “%1” from the device? This cannot be undone.").arg(root.deleteName) }
        onAccepted: { if (root.rowStillShows(root.deleteRow, root.deleteName)) files.remove(root.deleteRow) }
    }
}
