import QtQuick
import PolyScope

// Circular gauge: `value` 0..100 drawn as an arc with the figure in the middle.
Item {
    id: root

    property real value: 0
    property string text: Math.round(value) + "%"
    property string caption
    property color tone: Theme.levelColor(value)

    implicitWidth: 120
    implicitHeight: 120

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height
            const lw = Math.max(8, Math.min(w, h) * 0.11)
            const r = Math.min(w, h) / 2 - lw / 2 - 1
            const cx = w / 2, cy = h / 2
            const start = Math.PI * 0.75, sweep = Math.PI * 1.5
            ctx.lineWidth = lw
            ctx.lineCap = "round"
            ctx.strokeStyle = Theme.surfaceAlt
            ctx.beginPath(); ctx.arc(cx, cy, r, start, start + sweep); ctx.stroke()
            const frac = Math.max(0, Math.min(1, root.value / 100))
            if (frac > 0.001) {
                ctx.strokeStyle = root.tone
                ctx.beginPath(); ctx.arc(cx, cy, r, start, start + sweep * frac); ctx.stroke()
            }
        }
    }
    Column {
        anchors.centerIn: parent
        spacing: 0
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.text
            color: Theme.text
            font.pixelSize: Math.max(14, root.height * 0.2)
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.caption
            color: Theme.textMuted
            font.pixelSize: 11
            visible: text.length > 0
        }
    }

    onValueChanged: canvas.requestPaint()
    onToneChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    Connections { target: Theme; function onDarkChanged() { canvas.requestPaint() } }
}
