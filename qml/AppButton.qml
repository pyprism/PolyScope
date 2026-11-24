import QtQuick
import QtQuick.Controls
import PolyScope

Button {
    id: control

    property bool primary: false
    property bool danger: false

    implicitHeight: 32
    leftPadding: 14
    rightPadding: 14
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.textMuted
               : (control.primary || control.danger) ? Theme.accentText : Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.6
    }

    background: Rectangle {
        radius: Theme.radius - 2
        color: {
            if (!control.enabled)
                return Theme.surfaceAlt
            const base = control.danger ? Theme.danger : control.primary ? Theme.accent : Theme.surfaceAlt
            return control.down ? Qt.darker(base, 1.15) : control.hovered ? Qt.lighter(base, 1.08) : base
        }
        border.width: (control.primary || control.danger) ? 0 : 1
        border.color: Theme.border
        opacity: control.enabled ? 1 : 0.6
    }
}
