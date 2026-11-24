import QtQuick
import PolyScope

// Transient result message. Call show(text, ok).
Rectangle {
    id: toast

    property bool ok: true

    function show(message, success) {
        label.text = message
        ok = success !== false
        opacity = 1
        hideTimer.restart()
    }

    z: 100
    opacity: 0
    visible: opacity > 0
    radius: Theme.radius
    color: Theme.surfaceAlt
    border.color: ok ? Theme.success : Theme.danger
    border.width: 1
    width: Math.min(label.implicitWidth + 32, parent ? parent.width - 32 : 480)
    height: label.implicitHeight + 20

    Behavior on opacity { NumberAnimation { duration: 150 } }

    Text {
        id: label
        anchors.centerIn: parent
        width: toast.width - 32
        color: Theme.text
        wrapMode: Text.Wrap
        elide: Text.ElideRight
        maximumLineCount: 4
        horizontalAlignment: Text.AlignHCenter
    }

    Timer {
        id: hideTimer
        interval: 4500
        onTriggered: toast.opacity = 0
    }
}
