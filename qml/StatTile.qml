import QtQuick
import QtQuick.Layouts
import PolyScope

// Card with a small title, content below. Content goes in as children.
Card {
    id: root

    property string title
    property string subtitle
    default property alias body: bodyColumn.data

    padding: 14

    RowLayout {
        Layout.fillWidth: true
        Text { text: root.title; color: Theme.textMuted; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.fillWidth: true; elide: Text.ElideRight }
        Text { text: root.subtitle; color: Theme.textMuted; font.pixelSize: 11; visible: text.length > 0 }
    }
    ColumnLayout {
        id: bodyColumn
        Layout.fillWidth: true
        spacing: 8
    }
}
