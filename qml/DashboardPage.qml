import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// Live device overview: gauges, history charts, storage breakdown and top processes.
Item {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""
    // Device details (name, Android version, ...) arrive after the row is created; re-read on updates.
    property var device: ({})
    function refreshDevice() { device = serial ? Devices.get(serial) : ({}) }
    onSerialChanged: refreshDevice()
    Component.onCompleted: refreshDevice()
    Connections { target: Devices; function onDataChanged() { root.refreshDevice() } }

    signal result(string text, bool ok)

    function formatUptime(seconds) {
        const d = Math.floor(seconds / 86400), h = Math.floor(seconds % 86400 / 3600), m = Math.floor(seconds % 3600 / 60)
        return d > 0 ? d + "d " + h + "h" : h > 0 ? h + "h " + m + "m" : m + "m"
    }
    // This button always means "show the screen": keep the saved tuning (size, bit rate, ...)
    // but never inherit camera, OTG or audio-only mode from an earlier session on the Mirror page.
    function screenMirrorOptions(s) {
        return Object.assign({}, AppSettings.scrcpyOptions(s), { videoSource: "display", otg: false, video: true })
    }
    function rate(bytesPerSecond) { return monitor.formatBytes(bytesPerSecond) + "/s" }

    DeviceMonitor {
        id: monitor
        serial: root.serial
        active: root.visible && root.serial !== ""
        onActiveChanged: if (active && Object.keys(storage).length === 0) refreshStorage()
        onSerialChanged: if (active && serial !== "") Qt.callLater(refreshStorage)
    }

    // Process / package actions: remembered across the confirm dialog.
    property int stopPid: 0
    property string stopName
    property string detailPackage
    property var detailInfo: ({})
    property var detailApp: ({})
    property bool showAllApps: false
    property string pendingUninstall

    Connections {
        target: monitor
        function onActionFinished(action, ok, message) {
            root.result(message, ok)
            if (ok && action === "uninstall") {
                detailDialog.close()
                monitor.refreshStorage()
            }
        }
        function onPackageDetailsReady(package, details) {
            if (package === root.detailPackage)
                root.detailInfo = details
        }
    }

    function openApp(app) {
        detailApp = app
        detailPackage = app.package
        detailInfo = ({})
        monitor.loadPackageDetails(app.package)
        detailDialog.open()
    }

    EmptyState {
        anchors.fill: parent
        visible: root.serial === ""
        title: qsTr("No device selected")
        hint: qsTr("Connect a phone over USB or Wi-Fi, then pick it in the sidebar.")
    }

    ScrollView {
        anchors.fill: parent
        visible: root.serial !== ""
        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            width: parent.width - 12
            spacing: 12

            // ---- Header
            Card {
                Layout.fillWidth: true
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 16
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: root.device.name || root.serial; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text {
                            text: [root.device.manufacturer, root.device.androidVersion ? "Android " + root.device.androidVersion : "", root.device.sdk ? "API " + root.device.sdk : ""]
                                  .filter(s => s).join(" · ")
                            color: Theme.textMuted
                            font.pixelSize: 12
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 6
                            Badge { text: root.device.wireless ? "Wi-Fi" : "USB"; tone: Theme.accent }
                            Badge { text: qsTr("Up %1").arg(root.formatUptime(monitor.uptimeSeconds)); visible: monitor.uptimeSeconds > 0 }
                            Badge { text: monitor.batteryStatus; visible: monitor.batteryStatus !== ""; tone: monitor.power !== "" ? Theme.success : Theme.textMuted }
                            Badge { text: monitor.error; visible: monitor.error !== ""; tone: Theme.danger }
                        }
                    }
                    AppButton { text: qsTr("Screenshot"); onClicked: Actions.screenshot(root.serial) }
                    AppButton {
                        text: Scrcpy.runningSerials.indexOf(root.serial) >= 0 ? qsTr("Stop mirror") : qsTr("Mirror")
                        primary: Scrcpy.runningSerials.indexOf(root.serial) < 0
                        enabled: Tools.scrcpyReady
                        onClicked: Scrcpy.runningSerials.indexOf(root.serial) >= 0
                                   ? Scrcpy.stop(root.serial)
                                   : root.result(Scrcpy.start(root.serial, root.screenMirrorOptions(root.serial)) || qsTr("Mirroring started"), true)
                    }
                }
            }

            // ---- Gauges
            GridLayout {
                Layout.fillWidth: true
                columns: Math.max(1, Math.min(4, Math.floor(width / 230)))
                columnSpacing: 12
                rowSpacing: 12
                uniformCellWidths: true

                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("CPU")
                    subtitle: monitor.coreUsage.length + qsTr(" cores")
                    RowLayout {
                        Layout.fillWidth: true
                        Gauge { Layout.preferredWidth: 110; Layout.preferredHeight: 110; value: monitor.cpuPercent }
                        Text {
                            Layout.fillWidth: true
                            text: qsTr("Overall load across all cores, sampled every 2 s.")
                            color: Theme.textMuted; font.pixelSize: 11; wrapMode: Text.WordWrap
                        }
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Memory")
                    subtitle: monitor.formatBytes(monitor.memTotalKb * 1024)
                    RowLayout {
                        Layout.fillWidth: true
                        Gauge { Layout.preferredWidth: 110; Layout.preferredHeight: 110; value: monitor.memUsedPercent }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { Layout.fillWidth: true; text: qsTr("Used %1").arg(monitor.formatBytes(monitor.memUsedKb * 1024)); color: Theme.text; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            Text {
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                                visible: monitor.swapTotalKb > 0
                                text: qsTr("Swap %1 / %2").arg(monitor.formatBytes(monitor.swapUsedKb * 1024)).arg(monitor.formatBytes(monitor.swapTotalKb * 1024))
                                color: Theme.textMuted; font.pixelSize: 11
                            }
                        }
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Battery")
                    subtitle: monitor.batteryTempC >= 0 ? monitor.batteryTempC.toFixed(1) + " °C" : ""
                    RowLayout {
                        Layout.fillWidth: true
                        Gauge {
                            Layout.preferredWidth: 110; Layout.preferredHeight: 110
                            value: Math.max(0, monitor.batteryLevel)
                            // Low charge is the bad end here, so invert the usual colouring.
                            tone: monitor.batteryLevel < 0 ? Theme.textMuted : monitor.batteryLevel <= 15 ? Theme.danger : monitor.batteryLevel <= 30 ? Theme.warning : Theme.success
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { Layout.fillWidth: true; text: monitor.batteryStatus || "—"; color: Theme.text; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: monitor.power !== ""; text: qsTr("Power: %1").arg(monitor.power); color: Theme.textMuted; font.pixelSize: 11 }
                            Text { visible: monitor.batteryVoltageMv > 0; text: (monitor.batteryVoltageMv / 1000).toFixed(2) + " V"; color: Theme.textMuted; font.pixelSize: 11 }
                        }
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Network")
                    subtitle: qsTr("all interfaces")
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 10
                        RowLayout {
                            Text { text: "↓"; color: Theme.chart[1]; font.pixelSize: 22; font.bold: true }
                            ColumnLayout {
                                spacing: 0
                                Text { text: root.rate(monitor.rxRate); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                                Text { text: qsTr("download"); color: Theme.textMuted; font.pixelSize: 11 }
                            }
                        }
                        RowLayout {
                            Text { text: "↑"; color: Theme.chart[0]; font.pixelSize: 22; font.bold: true }
                            ColumnLayout {
                                spacing: 0
                                Text { text: root.rate(monitor.txRate); color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold }
                                Text { text: qsTr("upload"); color: Theme.textMuted; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }

            // ---- History charts
            GridLayout {
                Layout.fillWidth: true
                columns: width > 760 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12
                uniformCellWidths: true

                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("CPU and memory · last 2 minutes")
                    LineChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 170
                        maxValue: 100
                        formatValue: v => Math.round(v) + "%"
                        series: [
                            { label: qsTr("CPU"), color: Theme.chart[0], values: monitor.cpuHistory },
                            { label: qsTr("Memory"), color: Theme.chart[2], values: monitor.memHistory }
                        ]
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Network throughput")
                    LineChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 170
                        formatValue: v => monitor.formatBytes(v)
                        series: [
                            { label: qsTr("Download"), color: Theme.chart[1], values: monitor.rxHistory },
                            { label: qsTr("Upload"), color: Theme.chart[0], values: monitor.txHistory }
                        ]
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Per-core load")
                    Repeater {
                        model: monitor.coreUsage
                        delegate: BarRow {
                            required property real modelData
                            required property int index
                            Layout.fillWidth: true
                            labelWidth: 50
                            label: "cpu" + index
                            fraction: modelData / 100
                            tone: Theme.levelColor(modelData)
                            valueText: Math.round(modelData) + "%"
                        }
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Battery temperature")
                    subtitle: monitor.batteryTempC >= 0 ? monitor.batteryTempC.toFixed(1) + " °C" : ""
                    LineChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        zoom: true
                        minSpan: 2
                        formatValue: v => v.toFixed(1) + " °C"
                        series: [{ label: qsTr("Temperature"), color: Theme.chart[3], values: monitor.batteryTempHistory }]
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Battery voltage")
                    subtitle: monitor.batteryVoltageMv > 0 ? (monitor.batteryVoltageMv / 1000).toFixed(2) + " V" : ""
                    LineChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        zoom: true
                        minSpan: 0.1
                        formatValue: v => v.toFixed(2) + " V"
                        series: [{ label: qsTr("Voltage"), color: Theme.chart[4], values: monitor.batteryVoltageHistory }]
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Battery level history")
                    LineChart {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 150
                        maxValue: 100
                        formatValue: v => Math.round(v) + "%"
                        series: [{ label: qsTr("Charge"), color: Theme.chart[1], values: monitor.batteryHistory }]
                    }
                }
            }

            // ---- Storage
            StatTile {
                id: storageCard
                Layout.fillWidth: true
                title: qsTr("Storage (user data partition)")
                subtitle: monitor.storageBusy ? qsTr("scanning…") : ""

                readonly property var st: monitor.storage
                readonly property var cats: st.categories || []
                readonly property var colors: Theme.chart
                readonly property var slices: {
                    const out = []
                    for (let i = 0; i < cats.length; i++)
                        out.push({ label: cats[i].label, value: cats[i].bytes, color: cats[i].label === qsTr("Unattributed") ? Theme.border : colors[i % colors.length] })
                    if ((st.free || 0) > 0)
                        out.push({ label: qsTr("Free"), value: st.free, color: Theme.surfaceAlt })
                    return out
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Item { Layout.fillWidth: true }
                    AppButton { text: qsTr("Rescan"); enabled: !monitor.storageBusy; onClicked: monitor.refreshStorage() }
                }
                Text { visible: !!storageCard.st.error; text: storageCard.st.error || ""; color: Theme.danger; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 24
                    visible: (storageCard.st.total || 0) > 0

                    DonutChart {
                        Layout.preferredWidth: 190
                        Layout.preferredHeight: 190
                        slices: storageCard.slices
                        centerText: Math.round(100 * (storageCard.st.used || 0) / Math.max(1, storageCard.st.total || 1)) + "%"
                        centerCaption: qsTr("%1 of %2").arg(monitor.formatBytes(storageCard.st.used || 0)).arg(monitor.formatBytes(storageCard.st.total || 0))
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: storageCard.slices
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 8
                                Rectangle { width: 12; height: 12; radius: 3; color: modelData.color; border.color: Theme.border }
                                Text { text: modelData.label; color: Theme.text; Layout.fillWidth: true; font.pixelSize: 12 }
                                Text { text: monitor.formatBytes(modelData.value); color: Theme.textMuted; font.pixelSize: 12 }
                            }
                        }
                    }
                }

                SectionTitle { text: qsTr("Largest apps"); visible: (storageCard.st.apps || []).length > 0; Layout.topMargin: 8 }
                RowLayout {
                    visible: (storageCard.st.apps || []).length > 0
                    spacing: 14
                    Rectangle { width: 10; height: 10; radius: 2; color: Theme.chart[0] }
                    Text { text: qsTr("App"); color: Theme.textMuted; font.pixelSize: 11 }
                    Rectangle { width: 10; height: 10; radius: 2; color: Theme.chart[2] }
                    Text { text: qsTr("Data"); color: Theme.textMuted; font.pixelSize: 11 }
                    Rectangle { width: 10; height: 10; radius: 2; color: Theme.chart[4] }
                    Text { text: qsTr("Cache"); color: Theme.textMuted; font.pixelSize: 11 }
                }
                Repeater {
                    model: root.showAllApps ? (storageCard.st.apps || []) : (storageCard.st.apps || []).slice(0, 15)
                    delegate: BarRow {
                        required property var modelData
                        required property int index
                        readonly property real biggest: (storageCard.st.apps && storageCard.st.apps.length) ? storageCard.st.apps[0].total : 1
                        Layout.fillWidth: true
                        labelWidth: 230
                        label: modelData.package
                        clickable: true
                        onClicked: root.openApp(modelData)
                        valueText: monitor.formatBytes(modelData.total)
                        segments: [
                            { fraction: modelData.app / biggest, color: Theme.chart[0] },
                            { fraction: modelData.data / biggest, color: Theme.chart[2] },
                            { fraction: modelData.cache / biggest, color: Theme.chart[4] }
                        ]
                    }
                }

                AppButton {
                    visible: (storageCard.st.apps || []).length > 15
                    text: root.showAllApps ? qsTr("Show fewer") : qsTr("Show all %1 apps").arg((storageCard.st.apps || []).length)
                    onClicked: root.showAllApps = !root.showAllApps
                }
                Text {
                    visible: (storageCard.st.apps || []).length > 0
                    text: qsTr("Click an app for details and actions.")
                    color: Theme.textMuted; font.pixelSize: 11
                }

                SectionTitle { text: qsTr("Partitions"); visible: (storageCard.st.partitions || []).length > 0; Layout.topMargin: 8 }
                Repeater {
                    model: storageCard.st.partitions || []
                    delegate: BarRow {
                        required property var modelData
                        Layout.fillWidth: true
                        labelWidth: 150
                        valueWidth: 150
                        label: modelData.mount
                        fraction: modelData.percent / 100
                        // System partitions are read-only and always full; only user storage can run out.
                        tone: modelData.mount.indexOf("/data") === 0 || modelData.mount.indexOf("/storage") === 0
                              ? Theme.levelColor(modelData.percent) : Theme.textMuted
                        valueText: monitor.formatBytes(modelData.used) + " / " + monitor.formatBytes(modelData.total)
                    }
                }
            }

            // ---- Processes
            GridLayout {
                Layout.fillWidth: true
                columns: width > 760 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12
                uniformCellWidths: true

                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Top by CPU")
                    subtitle: qsTr("snapshot")
                    Repeater {
                        model: monitor.topCpu
                        delegate: BarRow {
                            required property var modelData
                            Layout.fillWidth: true
                            labelWidth: 190
                            label: modelData.name
                            fraction: modelData.cpu / 100
                            tone: Theme.chart[0]
                            valueText: modelData.cpu.toFixed(1) + "%"
                            actionText: qsTr("Stop")
                            onActionClicked: { root.stopPid = modelData.pid; root.stopName = modelData.name; stopDialog.open() }
                        }
                    }
                }
                StatTile {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: qsTr("Top by memory")
                    subtitle: qsTr("resident")
                    Repeater {
                        model: monitor.topMem
                        delegate: BarRow {
                            required property var modelData
                            readonly property real biggest: monitor.topMem.length ? monitor.topMem[0].mem : 1
                            Layout.fillWidth: true
                            labelWidth: 190
                            label: modelData.name
                            fraction: modelData.mem / Math.max(1, biggest)
                            tone: Theme.chart[2]
                            valueText: monitor.formatBytes(modelData.mem)
                            actionText: qsTr("Stop")
                            onActionClicked: { root.stopPid = modelData.pid; root.stopName = modelData.name; stopDialog.open() }
                        }
                    }
                }
            }
        }
    }

    AppDialog {
        id: stopDialog
        title: qsTr("Stop process")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: WrappedLabel {
            text: qsTr("Stop %1 (pid %2)? Apps are force-stopped; other processes are killed and may be restarted by the system. Protected system processes will refuse.")
                  .arg(root.stopName).arg(root.stopPid)
        }
        onAccepted: monitor.stopProcess(root.stopPid, root.stopName)
    }

    AppDialog {
        id: uninstallDialog
        title: qsTr("Uninstall app")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: WrappedLabel { text: qsTr("Uninstall %1 and delete its data?").arg(root.detailPackage) }
        onAccepted: monitor.packageAction(root.detailPackage, "uninstall")
    }

    AppDialog {
        id: detailDialog
        title: root.detailPackage
        width: Math.min(520, parent ? parent.width - 40 : 520)

        readonly property var info: root.detailInfo
        readonly property var app: root.detailApp

        contentItem: ColumnLayout {
            spacing: 10

            BarRow {
                Layout.fillWidth: true
                labelWidth: 60
                valueWidth: 90
                label: qsTr("Total")
                valueText: monitor.formatBytes(detailDialog.app.total || 0)
                segments: {
                    const t = Math.max(1, detailDialog.app.total || 1)
                    return [
                        { fraction: (detailDialog.app.app || 0) / t, color: Theme.chart[0] },
                        { fraction: (detailDialog.app.data || 0) / t, color: Theme.chart[2] },
                        { fraction: (detailDialog.app.cache || 0) / t, color: Theme.chart[4] }
                    ]
                }
            }
            GridLayout {
                columns: 3
                columnSpacing: 10
                rowSpacing: 4
                Layout.fillWidth: true
                Repeater {
                    model: [
                        { name: qsTr("App (code)"), bytes: detailDialog.app.app || 0, color: Theme.chart[0] },
                        { name: qsTr("Data"), bytes: detailDialog.app.data || 0, color: Theme.chart[2] },
                        { name: qsTr("Cache"), bytes: detailDialog.app.cache || 0, color: Theme.chart[4] }
                    ]
                    delegate: Item {
                        required property var modelData
                        Layout.columnSpan: 3
                        Layout.fillWidth: true
                        implicitHeight: 18
                        Rectangle { id: sw; width: 10; height: 10; radius: 2; color: parent.modelData.color; anchors.verticalCenter: parent.verticalCenter }
                        Text { anchors.left: sw.right; anchors.leftMargin: 8; anchors.verticalCenter: parent.verticalCenter; text: parent.modelData.name; color: Theme.text; font.pixelSize: 12 }
                        Text {
                            anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                            text: monitor.formatBytes(parent.modelData.bytes) + (storageCard.st.total ? "  ·  " + (100 * parent.modelData.bytes / storageCard.st.total).toFixed(2) + "% of storage" : "")
                            color: Theme.textMuted; font.pixelSize: 12
                        }
                    }
                }
            }

            SectionTitle { text: qsTr("Details") }
            Text { visible: !detailDialog.info.found && detailDialog.info.found !== undefined; text: qsTr("Package details not available."); color: Theme.textMuted; font.pixelSize: 12 }
            Text { visible: detailDialog.info.found === undefined; text: qsTr("Loading…"); color: Theme.textMuted; font.pixelSize: 12 }
            GridLayout {
                visible: !!detailDialog.info.found
                columns: 2
                columnSpacing: 12
                rowSpacing: 3
                Layout.fillWidth: true
                Repeater {
                    model: [
                        { k: qsTr("SDK"), v: detailDialog.info.targetSdk ? qsTr("min %1 · target %2").arg(detailDialog.info.minSdk).arg(detailDialog.info.targetSdk) : "" },
                        { k: qsTr("Type"), v: detailDialog.info.system ? qsTr("System app") : qsTr("User app") },
                        { k: qsTr("Installed"), v: detailDialog.info.firstInstall },
                        { k: qsTr("Updated"), v: detailDialog.info.lastUpdate },
                        { k: qsTr("Installer"), v: detailDialog.info.installer }
                    ].filter(r => r.v)
                    delegate: Item {
                        required property var modelData
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        implicitHeight: 18
                        Text { text: parent.modelData.k; color: Theme.textMuted; font.pixelSize: 12; width: 80 }
                        Text { x: 90; width: parent.width - 90; text: parent.modelData.v; color: Theme.text; font.pixelSize: 12; elide: Text.ElideRight }
                    }
                }
            }

            RowLayout {
                spacing: 8
                Layout.topMargin: 6
                AppButton { text: qsTr("Force stop"); onClicked: monitor.packageAction(root.detailPackage, "force-stop") }
                AppButton { text: qsTr("Open on device"); onClicked: monitor.packageAction(root.detailPackage, "open-settings") }
                Item { Layout.fillWidth: true }
                AppButton { text: qsTr("Uninstall"); danger: true; onClicked: uninstallDialog.open() }
            }
        }
    }
}
