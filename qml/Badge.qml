import QtQuick
import PolyScope

Rectangle {
    id: badge

    property alias text: label.text
    property color tone: Theme.textMuted

    implicitWidth: label.implicitWidth + 14
    implicitHeight: 20
    radius: 10
    color: Qt.rgba(tone.r, tone.g, tone.b, 0.18)
    border.color: Qt.rgba(tone.r, tone.g, tone.b, 0.5)

    Text {
        id: label
        anchors.centerIn: parent
        color: badge.tone
        font.pixelSize: 11
        font.weight: Font.DemiBold
    }
}
