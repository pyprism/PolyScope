pragma Singleton
import QtQuick
import PolyScope

QtObject {
    readonly property bool dark: AppSettings.dark

    readonly property color window: dark ? "#2E3035" : "#F3F4F6"
    readonly property color surface: dark ? "#383A40" : "#FFFFFF"
    readonly property color surfaceAlt: dark ? "#42454C" : "#E9EBEF"
    readonly property color border: dark ? "#4E5159" : "#D3D6DC"
    readonly property color text: dark ? "#E3E5E8" : "#1F2328"
    readonly property color textMuted: dark ? "#A7ABB2" : "#5F6672"
    readonly property color accent: dark ? "#6C97F0" : "#3B6FD8"
    readonly property color accentText: dark ? "#14161A" : "#FFFFFF" // contrast on accent/danger fills
    readonly property color success: dark ? "#52B788" : "#2E9461"
    readonly property color warning: dark ? "#E0A93C" : "#B97A12"
    readonly property color danger: dark ? "#E26D6A" : "#C4423E"

    // Categorical palette for charts: distinguishable in both themes and for common colour-vision types.
    readonly property var chart: dark
        ? ["#6C97F0", "#52B788", "#E0A93C", "#E26D6A", "#B48EF0", "#4FC3D9", "#F08FB5", "#9AA3B2"]
        : ["#3B6FD8", "#2E9461", "#B97A12", "#C4423E", "#7C4DCC", "#1C8FA6", "#C2477C", "#6B7280"]

    // Green -> amber -> red as a load climbs.
    function levelColor(percent) {
        return percent >= 90 ? danger : percent >= 70 ? warning : success
    }

    readonly property int radius: 8
    readonly property int spacing: 12
}
