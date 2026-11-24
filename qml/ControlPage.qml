import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Quick hardware keys + text input, then the Input/Display catalogue.
ColumnLayout {
    id: root

    property var targets: []
    property string initialCommand
    readonly property string serial: targets.length ? targets[0] : ""

    signal result(string text, bool ok)

    spacing: 10

    Card {
        Layout.fillWidth: true
        enabled: root.targets.length > 0

        SectionTitle { text: root.targets.length > 1 ? qsTr("Quick controls (%1 devices)").arg(root.targets.length) : qsTr("Quick controls") }
        Flow {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [
                    ["Home", "KEYCODE_HOME"], ["Back", "KEYCODE_BACK"], ["Recents", "KEYCODE_APP_SWITCH"],
                    ["Power", "KEYCODE_POWER"], ["Vol +", "KEYCODE_VOLUME_UP"], ["Vol −", "KEYCODE_VOLUME_DOWN"],
                    ["Mute", "KEYCODE_VOLUME_MUTE"], ["Wake", "KEYCODE_WAKEUP"], ["Sleep", "KEYCODE_SLEEP"],
                    ["Play/Pause", "KEYCODE_MEDIA_PLAY_PAUSE"], ["Menu", "KEYCODE_MENU"]
                ]
                delegate: AppButton {
                    required property var modelData
                    text: modelData[0]
                    onClicked: { for (const s of root.targets) Actions.keyEvent(s, modelData[1]) }
                }
            }
            AppButton { text: qsTr("Screenshot"); onClicked: { for (const s of root.targets) Actions.screenshot(s) } }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField { id: typeText; Layout.fillWidth: true; placeholderText: qsTr("Type text on device"); selectByMouse: true; onAccepted: send.clicked() }
            AppButton {
                id: send
                text: qsTr("Send")
                enabled: typeText.text !== ""
                onClicked: { for (const s of root.targets) Actions.inputText(s, typeText.text); typeText.text = "" }
            }
        }
    }

    CommandGroupPage {
        Layout.fillWidth: true
        Layout.fillHeight: true
        groupNames: ["Input", "Display"]
        targets: root.targets
        initialCommand: root.initialCommand
        onResult: (text, ok) => root.result(text, ok)
    }
}
