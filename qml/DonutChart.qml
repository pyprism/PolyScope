import QtQuick
import PolyScope

// Donut of `slices` = [{label, value, color}]. Centre shows a title and subtitle.
Item {
    id: root

    property var slices: []
    property string centerText
    property string centerCaption

    implicitWidth: 180
    implicitHeight: 180

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true
        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height
            const lw = Math.max(14, Math.min(w, h) * 0.17)
            const r = Math.min(w, h) / 2 - lw / 2 - 1
            const cx = w / 2, cy = h / 2
            let total = 0
            for (const s of root.slices) total += Math.max(0, s.value)
            ctx.lineWidth = lw
            ctx.lineCap = "butt"
            if (total <= 0) {
                ctx.strokeStyle = Theme.surfaceAlt
                ctx.beginPath(); ctx.arc(cx, cy, r, 0, Math.PI * 2); ctx.stroke()
                return
            }
            let angle = -Math.PI / 2
            const gap = root.slices.length > 1 ? 0.012 : 0
            for (const s of root.slices) {
                const v = Math.max(0, s.value)
                if (v <= 0) continue
                const sweep = v / total * Math.PI * 2
                ctx.strokeStyle = s.color
                ctx.beginPath()
                ctx.arc(cx, cy, r, angle + gap, angle + Math.max(gap, sweep - gap))
                ctx.stroke()
                angle += sweep
            }
        }
    }
    Column {
        anchors.centerIn: parent
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.centerText; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: root.centerCaption; color: Theme.textMuted; font.pixelSize: 11 }
    }

    onSlicesChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    Connections { target: Theme; function onDarkChanged() { canvas.requestPaint() } }
}
