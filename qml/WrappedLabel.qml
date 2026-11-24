import QtQuick
import QtQuick.Controls
import PolyScope

// Word-wrapped label with a fixed implicit width, safe as a Dialog contentItem
// (a wrapping Label's own implicitWidth depends on width and loops inside dialogs).
Item {
    id: root

    property alias text: label.text

    implicitWidth: 380
    implicitHeight: label.implicitHeight

    Label {
        id: label
        width: root.width
        wrapMode: Text.WordWrap
        color: Theme.text
    }
}
