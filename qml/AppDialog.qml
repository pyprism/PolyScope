import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Modal dialog centred in the window, with a title bar and a close button (✕ / Esc).
// An explicit width avoids the Basic style's implicitWidth binding loop inside layouts.
Dialog {
    id: root

    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - 40 : 440)
    closePolicy: Popup.CloseOnEscape

    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }

    header: RowLayout {
        visible: root.title.length > 0
        Label {
            Layout.fillWidth: true
            Layout.margins: 14
            text: root.title
            color: Theme.text
            font.pixelSize: 16
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        AppButton {
            Layout.rightMargin: 10
            text: "✕"
            onClicked: root.reject()
            Accessible.name: qsTr("Close")
        }
    }
}
