import QtQuick
import QtQuick.Layouts
import PolyScope

ColumnLayout {
    id: root

    property string title
    property string hint
    property alias actions: actionRow.data

    spacing: 8

    Item { Layout.fillHeight: true }
    Text {
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.maximumWidth: 420
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: root.title
        color: Theme.text
        font.pixelSize: 17
        font.weight: Font.DemiBold
    }
    Text {
        Layout.alignment: Qt.AlignHCenter
        // fillWidth so the hint wraps to the pane instead of overflowing it off-centre.
        Layout.fillWidth: true
        Layout.maximumWidth: 420
        text: root.hint
        color: Theme.textMuted
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        visible: text.length > 0
    }
    RowLayout {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        spacing: 8
    }
    Item { Layout.fillHeight: true }
}
