import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

AppWindow {
    id: root

    // Focused device drives single-device pages; ticked devices form the batch for command forms.
    property string focusSerial
    property var checked: ({})
    property string jumpId
    property int jumpPage: -1

    readonly property var checkedList: Object.keys(checked).filter(s => checked[s] && Devices.indexOfSerial(s) >= 0)
    readonly property var batchTargets: checkedList.length > 0 ? checkedList : (focusSerial ? [focusSerial] : [])
    readonly property var focusTargets: focusSerial ? [focusSerial] : []
    readonly property var usbSerials: {
        Devices.count
        return Devices.onlineSerials().filter(s => !Devices.get(s).wireless)
    }

    readonly property var navItems: [
        { title: qsTr("Dashboard"), icon: "◔" },
        { title: qsTr("Mirror"), icon: "▶" },
        { title: qsTr("Control & Input"), icon: "⌨" },
        { title: qsTr("Files"), icon: "🗂" },
        { title: qsTr("Apps"), icon: "▦" },
        { title: qsTr("System & Services"), icon: "⚙" },
        { title: qsTr("Network & Power"), icon: "⏻" },
        { title: qsTr("Logs"), icon: "≣" },
        { title: qsTr("Info"), icon: "ⓘ" },
        { title: qsTr("Advanced"), icon: "›_" }
    ]

    // Where each catalogue group lives: [page, tab]
    readonly property var groupHome: ({
        "Input": [2, 0], "Display": [2, 0],
        "System & services": [5, 1],
        "Network & power": [6, 0], "Power & recovery": [6, 0],
        "Logs & diagnostics": [7, 1],
        "Device info": [8, 1]
    })

    width: 1180
    height: 760
    minimumWidth: 860
    minimumHeight: 560
    visible: true
    title: "PolyScope"

    function notify(text, ok) { toast.show(text, ok) }

    function ensureFocus() {
        if (focusSerial && Devices.indexOfSerial(focusSerial) >= 0)
            return
        const online = Devices.onlineSerials()
        focusSerial = online.length ? online[0] : ""
    }

    function jump(commandId, group) {
        const home = groupHome[group] || [9, 0]
        jumpId = ""
        nav.currentIndex = home[0]
        if (home[0] === 5) systemTabs.currentIndex = home[1]
        if (home[0] === 7) logTabs.currentIndex = home[1]
        if (home[0] === 8) infoTabs.currentIndex = home[1]
        if (home[0] === 9) advTabs.currentIndex = 1
        jumpPage = home[0]
        Qt.callLater(() => jumpId = commandId)
    }

    Connections {
        target: Devices
        function onCountChanged() { root.ensureFocus() }
    }
    Connections {
        target: Actions
        function onOperationFinished(op, ok, message) {
            if (op !== "pair" && op !== "connect" && op !== "disconnect" && op !== "usbToWifi")
                root.notify(message, ok)
        }
        function onScreenshotSaved(serial, path) { root.notify(qsTr("Screenshot saved: %1").arg(path), true) }
    }
    Component.onCompleted: ensureFocus()

    Shortcut { sequences: [StandardKey.Find, "Ctrl+K"]; onActivated: palette.open() }

    header: Rectangle {
        height: 48
        color: Theme.surface
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 8
            Text { text: "PolyScope"; color: Theme.text; font.pixelSize: 17; font.weight: Font.Bold }
            Badge { text: Tools.adbReady ? Tools.adbVersion.replace(/^Android Debug Bridge version /, "adb ") : "adb ✕"; tone: Tools.adbReady ? Theme.success : Theme.danger }
            Badge { text: Tools.scrcpyReady ? "scrcpy " + Tools.scrcpyVersion.replace(/^scrcpy /, "") : "scrcpy ✕"; tone: Tools.scrcpyReady ? Theme.success : Theme.danger }
            Item { Layout.fillWidth: true }
            AppButton { text: qsTr("Find command  %1").arg(Qt.platform.os === "osx" ? "⌘K" : "Ctrl+K"); onClicked: palette.open() }
            AppButton { text: qsTr("Wireless"); onClicked: wireless.open() }
            AppButton { text: qsTr("Settings"); onClicked: settings.open() }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ToolBanner {
            Layout.fillWidth: true
            onOpenSettings: settings.open()
        }

        // Drag the thin handles to resize the device list and the navigation column.
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 5
                color: SplitHandle.pressed ? Theme.accent : SplitHandle.hovered ? Theme.textMuted : Theme.border
            }

            // ---- Devices sidebar
            Rectangle {
                SplitView.preferredWidth: 290
                SplitView.minimumWidth: 220
                SplitView.maximumWidth: 520
                color: Theme.surface

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    RowLayout {
                        SectionTitle { text: qsTr("Devices"); Layout.fillWidth: true }
                        Badge { text: Devices.trackerConnected ? qsTr("live") : qsTr("offline"); tone: Devices.trackerConnected ? Theme.success : Theme.warning }
                    }

                    ListView {
                        id: deviceList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 6
                        model: Devices
                        ScrollBar.vertical: ScrollBar {}

                        delegate: Rectangle {
                            id: card
                            required property string serial
                            required property string name
                            required property string state
                            required property bool online
                            required property bool wireless
                            required property string androidVersion
                            required property int batteryLevel
                            required property string power
                            readonly property bool selected: serial === root.focusSerial

                            width: ListView.view.width
                            height: info.implicitHeight + 20
                            radius: Theme.radius
                            color: selected ? Theme.surfaceAlt : "transparent"
                            border.color: selected ? Theme.accent : Theme.border

                            MouseArea {
                                anchors.fill: parent
                                enabled: card.online
                                onClicked: root.focusSerial = card.serial
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 8
                                AppCheckBox {
                                    enabled: card.online
                                    checked: !!root.checked[card.serial]
                                    onToggled: {
                                        const c = Object.assign({}, root.checked)
                                        c[card.serial] = checked
                                        root.checked = c
                                    }
                                    ToolTip.text: qsTr("Include in batch")
                                    ToolTip.visible: hovered
                                }
                                ColumnLayout {
                                    id: info
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text { text: card.name; color: Theme.text; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Text { text: card.serial; color: Theme.textMuted; font.pixelSize: 10; elide: Text.ElideMiddle; Layout.fillWidth: true }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Badge {
                                            text: card.state
                                            tone: card.online ? Theme.success : card.state === "unauthorized" ? Theme.warning : Theme.danger
                                        }
                                        Badge { text: card.wireless ? "Wi-Fi" : "USB"; tone: Theme.accent }
                                        Badge { visible: card.androidVersion !== ""; text: "Android " + card.androidVersion }
                                        Badge { visible: card.batteryLevel >= 0; text: card.batteryLevel + "%"; tone: card.batteryLevel <= 15 ? Theme.danger : Theme.textMuted }
                                    }
                                    Text {
                                        visible: card.state === "unauthorized" || card.state === "offline"
                                        Layout.fillWidth: true
                                        wrapMode: Text.WordWrap
                                        font.pixelSize: 11
                                        color: Theme.warning
                                        text: card.state === "unauthorized"
                                              ? qsTr("Accept the “Allow USB debugging” prompt on the phone.")
                                              : qsTr("Device not responding. Replug, or reconnect from Wireless.")
                                    }
                                }
                            }
                        }

                        EmptyState {
                            anchors.fill: parent
                            visible: Devices.count === 0
                            title: qsTr("No devices")
                            hint: Tools.adbReady ? qsTr("Plug in a phone with USB debugging enabled, or connect over Wi-Fi.")
                                                 : qsTr("adb is not available yet. Open Settings to point to it.")
                            actions: [AppButton { text: qsTr("Wireless…"); primary: true; onClicked: wireless.open() }]
                        }
                    }

                    Label {
                        visible: root.checkedList.length > 0
                        Layout.fillWidth: true
                        text: qsTr("Batch: %1 devices selected. Command forms run on all of them.").arg(root.checkedList.length)
                        color: Theme.accent
                        wrapMode: Text.WordWrap
                        font.pixelSize: 12
                    }
                }
            }

            // ---- Navigation
            Item {
                SplitView.preferredWidth: 200
                SplitView.minimumWidth: 56
                SplitView.maximumWidth: 320

            ListView {
                id: nav
                objectName: "nav"
                anchors.fill: parent
                anchors.margins: 8
                clip: true
                model: root.navItems
                currentIndex: 0
                interactive: false
                spacing: 2
                delegate: ItemDelegate {
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    height: 38
                    highlighted: ListView.isCurrentItem
                    onClicked: nav.currentIndex = index
                    contentItem: RowLayout {
                        spacing: 10
                        Text { text: modelData.icon; color: Theme.textMuted; Layout.preferredWidth: 20; horizontalAlignment: Text.AlignHCenter }
                        Text { text: modelData.title; color: Theme.text; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    background: Rectangle { radius: 6; color: highlighted ? Theme.surfaceAlt : "transparent" }
                }
            }
            }

            // ---- Pages
            Item {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 420

            StackLayout {
                anchors.fill: parent
                anchors.margins: 16
                currentIndex: nav.currentIndex

                DashboardPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }

                MirrorPage { targets: root.batchTargets; onResult: (t, ok) => root.notify(t, ok) }

                ControlPage {
                    targets: root.batchTargets
                    initialCommand: root.jumpPage === 2 ? root.jumpId : ""
                    onResult: (t, ok) => root.notify(t, ok)
                }

                FilesPage { id: filesPage; targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }
                AppsPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }

                PageTabs {
                    id: systemTabs
                    tabs: [qsTr("Services"), qsTr("Commands")]
                    ServicesPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }
                    CommandGroupPage {
                        groupNames: ["System & services"]
                        targets: root.batchTargets
                        initialCommand: root.jumpPage === 5 ? root.jumpId : ""
                        onResult: (t, ok) => root.notify(t, ok)
                    }
                }

                CommandGroupPage {
                    groupNames: ["Network & power", "Power & recovery"]
                    targets: root.batchTargets
                    initialCommand: root.jumpPage === 6 ? root.jumpId : ""
                    onResult: (t, ok) => root.notify(t, ok)
                }

                PageTabs {
                    id: logTabs
                    tabs: [qsTr("Logcat"), qsTr("Diagnostics")]
                    LogsPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }
                    CommandGroupPage {
                        groupNames: ["Logs & diagnostics"]
                        targets: root.batchTargets
                        initialCommand: root.jumpPage === 7 ? root.jumpId : ""
                        onResult: (t, ok) => root.notify(t, ok)
                    }
                }

                PageTabs {
                    id: infoTabs
                    tabs: [qsTr("Overview"), qsTr("Commands")]
                    InfoPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }
                    CommandGroupPage {
                        groupNames: ["Device info"]
                        targets: root.batchTargets
                        initialCommand: root.jumpPage === 8 ? root.jumpId : ""
                        onResult: (t, ok) => root.notify(t, ok)
                    }
                }

                PageTabs {
                    id: advTabs
                    tabs: [qsTr("Shell"), qsTr("All other commands")]
                    ShellPage { targets: root.focusTargets; onResult: (t, ok) => root.notify(t, ok) }
                    CommandGroupPage {
                        groupNames: ["Server", "Connection", "Port forwarding", "Files", "Apps", "Privileged"]
                        targets: root.batchTargets
                        initialCommand: root.jumpPage === 9 ? root.jumpId : ""
                        onResult: (t, ok) => root.notify(t, ok)
                    }
                }
            }
            }
        }
    }

    DropArea {
        anchors.fill: parent
        enabled: nav.currentIndex === 3 && root.focusSerial !== ""
        onDropped: drop => { if (drop.hasUrls) filesPage.pushUrls(drop.urls) }
    }

    Toast {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
    }

    CommandPalette {
        id: palette
        onChosen: (id, group) => root.jump(id, group)
    }
    SettingsDialog { id: settings }
    WirelessDialog {
        id: wireless
        usbSerials: root.usbSerials
        onReported: (t, ok) => root.notify(t, ok)
    }
}
