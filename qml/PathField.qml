import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyScope

// Text field with a Browse button. `folder` picks directories, otherwise files.
RowLayout {
    id: root

    property string text
    property bool folder: false
    property bool multiple: false
    property string placeholder
    property string title: folder ? qsTr("Choose folder") : qsTr("Choose file")

    signal edited(string value)
    // Fired when the user finishes typing (Enter / focus out) or picks from the dialog. Use it for
    // values that are expensive or unsafe to apply on every keystroke, such as tool paths.
    signal committed(string value)

    spacing: 6

    TextField {
        Layout.fillWidth: true
        text: root.text
        placeholderText: root.placeholder
        selectByMouse: true
        onTextEdited: {
            root.text = text
            root.edited(text)
        }
        onEditingFinished: root.committed(text)
    }
    AppButton {
        text: qsTr("Browse…")
        onClicked: root.folder ? folderDialog.open() : fileDialog.open()
    }

    FolderDialog {
        id: folderDialog
        title: root.title
        onAccepted: {
            root.text = decodeURIComponent(selectedFolder.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, ""))
            root.edited(root.text)
            root.committed(root.text)
        }
    }
    FileDialog {
        id: fileDialog
        title: root.title
        fileMode: root.multiple ? FileDialog.OpenFiles : FileDialog.OpenFile
        onAccepted: {
            const urls = root.multiple ? selectedFiles : [selectedFile]
            const paths = urls.map(u => decodeURIComponent(u.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, "")))
            root.text = paths.join("\n")
            root.edited(root.text)
            root.committed(root.text)
        }
    }
}
