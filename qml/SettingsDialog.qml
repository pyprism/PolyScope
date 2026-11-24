import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

AppWindow {
    id: root

    function open() {
        show()
        raise()
        requestActivate()
    }

    title: qsTr("Settings")
    width: 640
    height: 640
    minimumWidth: 480
    minimumHeight: 320
    flags: Qt.Dialog

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 16 // leave room for the overlay scrollbar
            spacing: 14

            SectionTitle { text: qsTr("Appearance") }
            RowLayout {
                Label { text: qsTr("Theme"); color: Theme.textMuted; Layout.preferredWidth: 90 }
                ComboBox {
                    textRole: "label"
                    valueRole: "value"
                    model: [
                        { label: qsTr("System"), value: "system" },
                        { label: qsTr("Light"), value: "light" },
                        { label: qsTr("Dark"), value: "dark" }
                    ]
                    currentIndex: Math.max(0, indexOfValue(AppSettings.themeMode))
                    onActivated: AppSettings.themeMode = currentValue
                }
            }

            SectionTitle { text: qsTr("Tools") }
            Repeater {
                // Static model: the live values are read inside the delegate. A model rebuilt on every
                // Tools change would destroy the focused path field while the user is typing.
                model: ["adb", "scrcpy"]
                delegate: ColumnLayout {
                    id: toolRow
                    required property string modelData
                    readonly property bool isAdb: modelData === "adb"
                    readonly property string tName: modelData
                    readonly property string tPath: isAdb ? Tools.adbPath : Tools.scrcpyPath
                    readonly property bool tReady: isAdb ? Tools.adbReady : Tools.scrcpyReady
                    readonly property string tVersion: isAdb ? Tools.adbVersion : Tools.scrcpyVersion
                    readonly property string tError: isAdb ? Tools.adbError : Tools.scrcpyError
                    readonly property bool tOver: isAdb ? Tools.adbOverridden : Tools.scrcpyOverridden
                    Layout.fillWidth: true
                    spacing: 4
                    RowLayout {
                        Label { text: toolRow.tName; color: Theme.text; font.weight: Font.DemiBold; Layout.preferredWidth: 90 }
                        Badge {
                            text: toolRow.tReady ? qsTr("Ready") : qsTr("Not available")
                            tone: toolRow.tReady ? Theme.success : Theme.danger
                        }
                        Badge { visible: toolRow.tOver; text: qsTr("Custom path"); tone: Theme.accent }
                    }
                    PathField {
                        Layout.fillWidth: true
                        text: toolRow.tPath
                        placeholder: qsTr("Auto-detected from PATH")
                        // Applied on Enter / focus-out / Browse so a half-typed path is never saved.
                        onCommitted: v => toolRow.isAdb ? Tools.setAdbPath(v) : Tools.setScrcpyPath(v)
                    }
                    Label {
                        visible: !toolRow.tReady && toolRow.tError !== ""
                        text: toolRow.tError
                        color: Theme.danger
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                        font.pixelSize: 12
                    }
                }
            }
            RowLayout {
                AppButton { text: qsTr("Re-check"); primary: true; enabled: !Tools.checking; onClicked: Tools.refresh() }
                Label { text: qsTr("Clear a path field and press Enter to return to auto-detect."); color: Theme.textMuted; font.pixelSize: 12 }
            }

            SectionTitle { text: qsTr("Output folders") }
            RowLayout {
                Label { text: qsTr("Screenshots"); color: Theme.textMuted; Layout.preferredWidth: 90 }
                PathField { Layout.fillWidth: true; folder: true; text: AppSettings.screenshotDir; onEdited: v => AppSettings.screenshotDir = v }
            }
            RowLayout {
                Label { text: qsTr("Recordings"); color: Theme.textMuted; Layout.preferredWidth: 90 }
                PathField { Layout.fillWidth: true; folder: true; text: AppSettings.recordDir; onEdited: v => AppSettings.recordDir = v }
            }

            Label { text: qsTr("MIT licence · unsigned build"); color: Theme.textMuted; font.pixelSize: 11 }
        }
    }
}
