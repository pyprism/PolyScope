import QtQuick
import QtQuick.Controls
import PolyScope

// Themed top-level window. Used for the main window and for tool windows
// (Settings, Wireless, text viewers) so they get native move/resize/close controls.
ApplicationWindow {
    color: Theme.window

    palette.window: Theme.window
    palette.windowText: Theme.text
    palette.base: Theme.surface
    palette.text: Theme.text
    palette.button: Theme.surfaceAlt
    palette.buttonText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.placeholderText: Theme.textMuted
    palette.mid: Theme.border
    palette.light: Theme.surface
    palette.dark: Theme.border
    palette.toolTipBase: Theme.surfaceAlt
    palette.toolTipText: Theme.text
}
