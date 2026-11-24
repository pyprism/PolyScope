import QtQuick
import QtQuick.Layouts
import PolyScope

// One labelled horizontal bar. Either a single `fraction`, or `segments` = [{fraction, color}].
RowLayout {
    id: root

    property string label
    property string valueText
    property real fraction: 0
    property color tone: Theme.accent
    property var segments: []
    property int labelWidth: 150
    property int valueWidth: 78
    // Optional: whole label/bar is clickable, and/or a trailing action button.
    property bool clickable: false
    property string actionText
    signal clicked()
    signal actionClicked()

    spacing: 10
    implicitHeight: 22

    Text {
        // Shrink on narrow tiles so the bar keeps some room instead of the row overflowing.
        Layout.preferredWidth: root.labelWidth
        Layout.minimumWidth: 0
        text: root.label
        color: Theme.text
        elide: Text.ElideMiddle
        font.pixelSize: 12
        font.underline: root.clickable && labelHover.hovered
        TapHandler { enabled: root.clickable; onTapped: root.clicked() }
        HoverHandler { id: labelHover; enabled: root.clickable; cursorShape: Qt.PointingHandCursor }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.minimumWidth: 30
        Layout.preferredHeight: 10
        radius: 5
        color: Theme.surfaceAlt
        clip: true
        TapHandler { enabled: root.clickable; onTapped: root.clicked() }

        Rectangle {
            visible: root.segments.length === 0
            width: parent.width * Math.max(0, Math.min(1, root.fraction))
            height: parent.height
            radius: 5
            color: root.tone
            Behavior on width { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        }
        Row {
            visible: root.segments.length > 0
            height: parent.height
            Repeater {
                model: root.segments
                delegate: Rectangle {
                    required property var modelData
                    width: root.width > 0 ? Math.max(0, modelData.fraction) * parent.parent.width : 0
                    height: parent.height
                    color: modelData.color
                }
            }
        }
    }
    Text {
        Layout.preferredWidth: root.valueWidth
        Layout.minimumWidth: 0
        elide: Text.ElideRight
        text: root.valueText
        color: Theme.textMuted
        horizontalAlignment: Text.AlignRight
        font.pixelSize: 12
    }
    AppButton {
        visible: root.actionText !== ""
        text: root.actionText
        implicitHeight: 24
        leftPadding: 8
        rightPadding: 8
        font.pixelSize: 11
        onClicked: root.actionClicked()
    }
}
