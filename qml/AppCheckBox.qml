import QtQuick
import QtQuick.Controls
import PolyScope

// CheckBox with an explicit, theme-coloured indicator (the Basic tick is hard to see on light surfaces).
CheckBox {
    id: control

    spacing: 8

    indicator: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: 4
        color: control.checked ? Theme.accent : Theme.surface
        border.color: control.checked ? Theme.accent : Theme.textMuted
        opacity: control.enabled ? 1 : 0.5

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "✓"
            color: Theme.accentText
            font.pixelSize: 13
            font.bold: true
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: Theme.text
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.5
    }
}
