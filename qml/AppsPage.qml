import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PolyScope

// Installed packages: filter, install APKs, and per-package actions.
ColumnLayout {
    id: root

    property var targets: []
    readonly property string serial: targets.length ? targets[0] : ""
    property string confirmOp
    property string confirmPackage

    signal result(string text, bool ok)

    spacing: 8

    PackageModel {
        id: packages
        serial: root.serial
        onOperationFinished: (op, ok, message) => root.result(op + ": " + message, ok)
        onInfoReady: (pkg, text) => infoWindow.present(pkg, text)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6
        TextField {
            Layout.fillWidth: true
            placeholderText: qsTr("Filter packages")
            selectByMouse: true
            onTextChanged: packages.filter = text
        }
        ComboBox {
            model: ["user", "system", "all", "enabled", "disabled"]
            onActivated: packages.scope = model[currentIndex]
        }
        Label { text: qsTr("%1 total").arg(packages.totalCount); color: Theme.textMuted }
        AppButton { text: qsTr("Refresh"); onClicked: packages.refresh() }
        AppButton { text: qsTr("Install APK…"); primary: true; onClicked: apkPicker.open() }
    }

    BusyIndicator { visible: packages.busy; running: visible; Layout.preferredHeight: 24 }

    Card {
        Layout.fillWidth: true
        Layout.fillHeight: true
        padding: 4

        EmptyState {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial === ""
            title: qsTr("No device selected")
            hint: qsTr("Pick an online device in the sidebar.")
        }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.serial !== ""
            clip: true
            model: packages
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property string packageName
                width: ListView.view.width
                height: 32
                contentItem: Text {
                    text: row.packageName
                    color: Theme.text
                    elide: Text.ElideMiddle
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: menu.popup()
                Menu {
                    id: menu
                    MenuItem { text: qsTr("Launch"); onTriggered: packages.launch(row.packageName) }
                    MenuItem { text: qsTr("Details"); onTriggered: packages.info(row.packageName) }
                    MenuItem { text: qsTr("Force stop"); onTriggered: packages.forceStop(row.packageName) }
                    MenuItem { text: qsTr("Enable"); onTriggered: packages.enable(row.packageName) }
                    MenuItem { text: qsTr("Disable"); onTriggered: root.ask("disable", row.packageName) }
                    MenuItem { text: qsTr("Clear data…"); onTriggered: root.ask("clear", row.packageName) }
                    MenuItem { text: qsTr("Extract APK…"); onTriggered: { extractPkg = row.packageName; extractDialog.open() } }
                    MenuSeparator {}
                    MenuItem { text: qsTr("Uninstall for current user…"); onTriggered: root.ask("uninstallUser", row.packageName) }
                    MenuItem { text: qsTr("Restore (existing)"); onTriggered: packages.restore(row.packageName) }
                    MenuItem { text: qsTr("Uninstall…"); onTriggered: root.ask("uninstall", row.packageName) }
                }
            }
        }
    }

    property string extractPkg

    function ask(op, pkg) {
        confirmOp = op
        confirmPackage = pkg
        confirm.open()
    }

    FileDialog {
        id: apkPicker
        title: qsTr("Choose APK files")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["APK (*.apk *.apks)", "All files (*)"]
        onAccepted: {
            const paths = selectedFiles.map(u => decodeURIComponent(u.toString().replace(/^file:\/\/(\/(?=[A-Za-z]:))?/, "")))
            // install-multiple is only right for the pieces of ONE app (base.apk / split_*.apk);
            // unrelated APKs are installed one by one.
            const isSplit = paths.length > 1 && paths.some(p => /(^|[\/\\])(base|split_[^\/\\]*)\.apk$/.test(p))
            packages.install(paths, isSplit, false)
        }
    }

    AppDialog {
        id: confirm
        title: qsTr("Confirm")
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: WrappedLabel {
            text: {
                const labels = {
                    disable: qsTr("Disable %1?"),
                    clear: qsTr("Clear all data of %1? This cannot be undone."),
                    uninstall: qsTr("Uninstall %1 and delete its data?"),
                    uninstallUser: qsTr("Uninstall %1 for the current user only?")
                }
                return (labels[root.confirmOp] || qsTr("%1?")).arg(root.confirmPackage)
            }
        }
        onAccepted: {
            const p = root.confirmPackage
            switch (root.confirmOp) {
            case "disable": packages.disable(p); break
            case "clear": packages.clearData(p); break
            case "uninstall": packages.uninstall(p, false); break
            case "uninstallUser": packages.uninstallForUser(p); break
            }
        }
    }

    AppDialog {
        id: extractDialog
        title: qsTr("Extract APK to…")
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel
        contentItem: PathField { id: extractDir; folder: true; text: AppSettings.screenshotDir }
        onAccepted: packages.extractApk(root.extractPkg, extractDir.text)
    }

    TextWindow { id: infoWindow }
}
