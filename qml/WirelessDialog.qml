import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Wireless adb: pair (Android 11+), connect, switch a USB device to Wi-Fi, discovered devices.
AppWindow {
    id: root

    function open() {
        show()
        raise()
        requestActivate()
    }

    property var usbSerials: []
    signal reported(string text, bool ok)

    title: qsTr("Wireless (Wi-Fi) debugging")
    width: 700
    height: 620
    minimumWidth: 480
    minimumHeight: 320
    flags: Qt.Dialog

    onVisibleChanged: if (visible && Tools.adbReady) mdns.refresh()

    MdnsModel { id: mdns }

    Connections {
        target: Actions
        function onOperationFinished(op, ok, message) {
            if (["pair", "connect", "disconnect", "usbToWifi"].indexOf(op) >= 0)
                root.reported(message, ok)
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 16
            spacing: 14

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                text: qsTr("adb has no Bluetooth transport; “wireless” means adb over Wi-Fi.")
            }

            Card {
                Layout.fillWidth: true
                SectionTitle { text: qsTr("Pair (Android 11+)") }
                Label { text: qsTr("On the phone: Developer options → Wireless debugging → Pair device with pairing code."); color: Theme.textMuted; wrapMode: Text.WordWrap; Layout.fillWidth: true; font.pixelSize: 12 }
                RowLayout {
                    TextField { id: pairAddr; Layout.fillWidth: true; placeholderText: "192.168.1.20:37099"; selectByMouse: true }
                    TextField { id: pairCode; Layout.preferredWidth: 110; placeholderText: qsTr("Code"); selectByMouse: true }
                    AppButton { text: qsTr("Pair"); primary: true; enabled: pairAddr.text !== "" && pairCode.text !== ""; onClicked: Actions.pair(pairAddr.text.trim(), pairCode.text.trim()) }
                }
            }

            Card {
                Layout.fillWidth: true
                SectionTitle { text: qsTr("Connect") }
                RowLayout {
                    TextField { id: connectAddr; Layout.fillWidth: true; placeholderText: "192.168.1.20:5555"; selectByMouse: true; onAccepted: Actions.connectDevice(text.trim()) }
                    AppButton { text: qsTr("Connect"); primary: true; enabled: connectAddr.text !== ""; onClicked: Actions.connectDevice(connectAddr.text.trim()) }
                    AppButton { text: qsTr("Disconnect"); enabled: connectAddr.text !== ""; onClicked: Actions.disconnectDevice(connectAddr.text.trim()) }
                }
            }

            Card {
                Layout.fillWidth: true
                SectionTitle { text: qsTr("USB → Wi-Fi") }
                Label { text: qsTr("Switch a USB-connected device to TCP/IP and connect to it."); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    ComboBox { id: usbBox; Layout.fillWidth: true; model: root.usbSerials }
                    SpinBox { id: tcpPort; from: 1024; to: 65535; value: 5555; editable: true }
                    AppButton { text: qsTr("Switch"); primary: true; enabled: usbBox.currentIndex >= 0 && root.usbSerials.length > 0; onClicked: Actions.usbToWifi(usbBox.currentText, tcpPort.value) }
                }
                Label { visible: root.usbSerials.length === 0; text: qsTr("No USB device online."); color: Theme.warning }
            }

            Card {
                Layout.fillWidth: true
                RowLayout {
                    SectionTitle { text: qsTr("Discovered on network"); Layout.fillWidth: true }
                    AppButton { text: qsTr("Scan"); enabled: !mdns.busy; onClicked: mdns.refresh() }
                }
                Label { visible: mdns.error !== ""; text: mdns.error; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Label { visible: mdnsList.count === 0 && !mdns.busy && mdns.error === ""; text: qsTr("Nothing found."); color: Theme.textMuted }
                Repeater {
                    id: mdnsList
                    model: mdns
                    delegate: RowLayout {
                        id: svc
                        required property string name
                        required property string address
                        required property int port
                        required property bool isPairing
                        Layout.fillWidth: true
                        Label { text: svc.name; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideMiddle }
                        Badge { text: svc.isPairing ? qsTr("pairing") : qsTr("connect"); tone: svc.isPairing ? Theme.warning : Theme.success }
                        Label { text: svc.address + ":" + svc.port; color: Theme.textMuted; font.family: AppSettings.monoFamily }
                        AppButton {
                            text: svc.isPairing ? qsTr("Use for pairing") : qsTr("Connect")
                            onClicked: svc.isPairing ? pairAddr.text = svc.address + ":" + svc.port
                                                     : Actions.connectDevice(svc.address + ":" + svc.port)
                        }
                    }
                }
            }
        }
    }
}
