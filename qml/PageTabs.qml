import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Tab strip over a StackLayout. Declare pages as children; `tabs` supplies the titles.
ColumnLayout {
    id: root

    property var tabs: []
    property alias currentIndex: bar.currentIndex
    default property alias pages: stack.data

    spacing: 10

    TabBar {
        id: bar
        Layout.fillWidth: true
        background: Rectangle { color: "transparent"; Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border } }
        Repeater {
            model: root.tabs
            AppTabButton {
                required property string modelData
                text: modelData
            }
        }
    }
    StackLayout {
        id: stack
        Layout.fillWidth: true
        Layout.fillHeight: true
        currentIndex: bar.currentIndex
    }
}
