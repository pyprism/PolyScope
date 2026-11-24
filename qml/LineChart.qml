import QtQuick
import PolyScope

// Time-series chart. series = [{values: [numbers], color, label}]. Newest sample at the right.
// `maxValue` > 0 fixes the Y range (e.g. 100 for percent); 0 scales to the data.
Item {
    id: root

    property var series: []
    property real maxValue: 0
    property int samples: 60
    property var formatValue: v => Math.round(v)
    property bool legend: true
    // Fit the Y axis to the data instead of starting at 0 (battery temperature / voltage barely move).
    property bool zoom: false
    property real minSpan: 1

    implicitHeight: 150

    readonly property real yMin: {
        if (!zoom) return 0
        let m = Infinity
        for (const s of series) for (const v of s.values) m = Math.min(m, v)
        if (!isFinite(m)) return 0
        return Math.floor((m - root.minSpan / 4) * 10) / 10
    }
    readonly property real yMax: {
        if (zoom) {
            let hi = -Infinity
            for (const s of series) for (const v of s.values) hi = Math.max(hi, v)
            if (!isFinite(hi)) return 1
            return Math.max(yMin + minSpan, Math.ceil((hi + minSpan / 4) * 10) / 10)
        }
        if (maxValue > 0) return maxValue
        let m = 0
        for (const s of series) for (const v of s.values) m = Math.max(m, v)
        return m > 0 ? m * 1.15 : 1
    }

    Row {
        id: legendRow
        visible: root.legend
        spacing: 14
        Repeater {
            model: root.series
            delegate: Row {
                required property var modelData
                spacing: 5
                Rectangle { width: 10; height: 10; radius: 2; color: modelData.color; anchors.verticalCenter: parent.verticalCenter }
                Text { text: modelData.label; color: Theme.textMuted; font.pixelSize: 11 }
            }
        }
    }

    Canvas {
        id: canvas
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: legendRow.visible ? legendRow.bottom : parent.top
        anchors.topMargin: legendRow.visible ? 6 : 0
        anchors.bottom: parent.bottom
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const w = width, h = height
            const padL = 44, padB = 4, padT = 6
            const pw = w - padL, ph = h - padB - padT
            ctx.font = "10px sans-serif"
            ctx.fillStyle = Theme.textMuted
            ctx.strokeStyle = Theme.border
            ctx.lineWidth = 1
            ctx.textBaseline = "middle"
            for (let i = 0; i <= 4; i++) {
                const y = padT + ph * i / 4
                ctx.beginPath(); ctx.moveTo(padL, Math.round(y) + 0.5); ctx.lineTo(w, Math.round(y) + 0.5); ctx.stroke()
                ctx.fillText(String(root.formatValue(root.yMin + (root.yMax - root.yMin) * (1 - i / 4))), 0, y)
            }
            const step = pw / Math.max(1, root.samples - 1)
            for (const s of root.series) {
                const n = s.values.length
                if (n < 2) continue
                const x0 = padL + pw - (n - 1) * step
                const pt = i => [x0 + i * step, padT + ph * (1 - Math.max(0, Math.min(1, (s.values[i] - root.yMin) / (root.yMax - root.yMin))))]
                // area
                ctx.beginPath()
                ctx.moveTo(x0, padT + ph)
                for (let i = 0; i < n; i++) { const p = pt(i); ctx.lineTo(p[0], p[1]) }
                ctx.lineTo(x0 + (n - 1) * step, padT + ph)
                ctx.closePath()
                ctx.globalAlpha = 0.14
                ctx.fillStyle = s.color
                ctx.fill()
                ctx.globalAlpha = 1
                // line
                ctx.beginPath()
                for (let i = 0; i < n; i++) { const p = pt(i); i === 0 ? ctx.moveTo(p[0], p[1]) : ctx.lineTo(p[0], p[1]) }
                ctx.strokeStyle = s.color
                ctx.lineWidth = 2
                ctx.lineJoin = "round"
                ctx.stroke()
            }
        }
    }

    onSeriesChanged: canvas.requestPaint()
    onMaxValueChanged: canvas.requestPaint()
    Connections { target: Theme; function onDarkChanged() { canvas.requestPaint() } }
}
