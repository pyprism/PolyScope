import QtQuick
import QtQuick.Controls
import PolyScope

// Flat tab with an accent underline for the selected one.
TabButton {
    id: control

    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    width: implicitWidth

    contentItem: Text {
        text: control.text
        color: control.checked ? Theme.text : Theme.textMuted
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    background: Rectangle {
        color: control.hovered && !control.checked ? Theme.surfaceAlt : "transparent"
        radius: 4
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 2
            color: control.checked ? Theme.accent : "transparent"
        }
    }
}
