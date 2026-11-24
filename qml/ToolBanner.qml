import QtQuick
import QtQuick.Layouts
import PolyScope

// First-run / missing-tool notice. Hidden when both adb and scrcpy are usable.
Rectangle {
    id: banner

    signal openSettings

    readonly property bool adbMissing: !Tools.adbReady && !Tools.checking
    readonly property bool scrcpyMissing: !Tools.scrcpyReady && !Tools.checking

    visible: adbMissing || scrcpyMissing
    color: Qt.rgba(Theme.warning.r, Theme.warning.g, Theme.warning.b, 0.16)
    border.color: Theme.warning
    implicitHeight: visible ? row.implicitHeight + 16 : 0

    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 8
        spacing: 12

        Text {
            Layout.fillWidth: true
            color: Theme.text
            wrapMode: Text.WordWrap
            text: {
                const missing = []
                if (banner.adbMissing)
                    missing.push("adb")
                if (banner.scrcpyMissing)
                    missing.push("scrcpy")
                return qsTr("%1 not found. PolyScope uses your own installs: put them on PATH or choose the binary in Settings.")
                    .arg(missing.join(" + "))
            }
        }
        AppButton {
            text: qsTr("Settings")
            primary: true
            onClicked: banner.openSettings()
        }
    }
}
