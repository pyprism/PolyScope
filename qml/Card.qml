import QtQuick
import QtQuick.Layouts
import PolyScope

// Bordered surface. Children are laid out in a column by default.
Rectangle {
    id: card

    default property alias content: column.data
    property int padding: 14

    color: Theme.surface
    radius: Theme.radius
    border.color: Theme.border
    border.width: 1
    implicitHeight: column.implicitHeight + padding * 2
    implicitWidth: column.implicitWidth + padding * 2

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: card.padding
        spacing: 8
    }
}
