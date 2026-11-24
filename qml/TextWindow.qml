import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Resizable viewer for long command output (package details, service dumps).
AppWindow {
    id: root

    property string body

    function present(heading, text) {
        title = heading
        body = text
        show()
        raise()
        requestActivate()
    }

    width: 760
    height: 560
    minimumWidth: 360
    minimumHeight: 240
    flags: Qt.Dialog

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label { text: root.title; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideMiddle; Layout.fillWidth: true }
            AppButton { text: qsTr("Copy"); onClicked: { area.selectAll(); area.copy(); area.deselect() } }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            TextArea {
                id: area
                readOnly: true
                text: root.body
                font.family: AppSettings.monoFamily
                font.pixelSize: 11
                color: Theme.text
                wrapMode: TextArea.WrapAnywhere
                selectByMouse: true
                background: Rectangle { color: Theme.window; border.color: Theme.border; radius: 6 }
            }
        }
    }
}
