import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Browse catalogue commands of the given groups and run them. This is the
// generic surface that exposes the whole adb feature set.
SplitView {
    id: root

    property var groupNames: []
    property var targets: []
    property string initialCommand
    property string selectedId
    property string query

    signal result(string text, bool ok)

    readonly property var rows: {
        const out = []
        const q = query.trim().toLowerCase()
        for (const g of Commands.groups()) {
            if (groupNames.length > 0 && groupNames.indexOf(g.name) < 0)
                continue
            for (const c of g.commands) {
                if (q === "" || (c.title + " " + c.id).toLowerCase().indexOf(q) >= 0)
                    out.push({ section: g.name, id: c.id, title: c.title, dangerous: c.dangerous })
            }
        }
        return out
    }

    onInitialCommandChanged: if (initialCommand) selectedId = initialCommand
    Component.onCompleted: selectedId = initialCommand || (rows.length ? rows[0].id : "")
    orientation: Qt.Horizontal
    handle: Rectangle {
        implicitWidth: 9
        color: "transparent"
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 3
            height: parent.height
            radius: 1.5
            color: SplitHandle.pressed ? Theme.accent : SplitHandle.hovered ? Theme.textMuted : Theme.border
        }
    }

    Item {
        SplitView.preferredWidth: 270
        SplitView.minimumWidth: 160
        SplitView.maximumWidth: 480

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: 6
        spacing: 8

        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Filter commands")
            selectByMouse: true
            onTextChanged: root.query = text
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.rows
            ScrollBar.vertical: ScrollBar {}
            section.property: "modelData.section"
            section.delegate: Label {
                required property string section
                text: section
                color: Theme.textMuted
                font.pixelSize: 11
                font.weight: Font.DemiBold
                topPadding: 8
                bottomPadding: 2
            }
            delegate: ItemDelegate {
                required property var modelData
                width: ListView.view.width
                height: 32
                highlighted: modelData.id === root.selectedId
                onClicked: root.selectedId = modelData.id
                contentItem: Text {
                    text: modelData.title
                    color: modelData.dangerous ? Theme.danger : Theme.text
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 6
                    color: highlighted ? Theme.surfaceAlt : "transparent"
                }
            }
        }
    }

    }

    Flickable {
        SplitView.fillWidth: true
        SplitView.minimumWidth: 240
        contentHeight: form.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        CommandForm {
            id: form
            width: parent.width - 16
            x: 6
            commandId: root.selectedId
            targets: root.targets
            onResult: (text, ok) => root.result(text, ok)
        }
    }
}
