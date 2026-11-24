import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Ctrl/Cmd+K: fuzzy find any adb command and jump to it.
Popup {
    id: root

    signal chosen(string commandId, string group)

    modal: true
    focus: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 40 : 560)
    height: Math.min(440, parent ? parent.height - 40 : 440)
    padding: 12

    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }

    onOpened: { field.text = ""; field.forceActiveFocus() }

    contentItem: ColumnLayout {
        spacing: 8
        TextField {
            id: field
            Layout.fillWidth: true
            placeholderText: qsTr("Search all adb commands…")
            selectByMouse: true
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
            Keys.onReturnPressed: if (list.currentItem) list.currentItem.pick()
            Keys.onEnterPressed: if (list.currentItem) list.currentItem.pick()
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            currentIndex: 0
            model: Commands.search(field.text)
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: item
                required property var modelData
                required property int index
                width: ListView.view.width
                height: 34
                highlighted: ListView.isCurrentItem
                function pick() { root.chosen(modelData.id, modelData.group); root.close() }
                onClicked: pick()
                contentItem: RowLayout {
                    Label { text: item.modelData.title; color: item.modelData.dangerous ? Theme.danger : Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    Label { text: item.modelData.group; color: Theme.textMuted; font.pixelSize: 11 }
                }
                background: Rectangle { radius: 6; color: item.highlighted ? Theme.surfaceAlt : "transparent" }
            }
        }
    }
}
