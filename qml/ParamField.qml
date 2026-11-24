import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import PolyScope

// One labelled input for a catalogue/scrcpy parameter spec.
// spec: { key, label, type|kind, choices, placeholder, required }
// Types: bool, int, choice, file, files, dir, path, text, bitrate.
ColumnLayout {
    id: root

    property var spec: ({})
    property var value
    readonly property string kind: spec.type || spec.kind || "text"

    signal edited(var newValue)

    spacing: 4

    function commit(v) {
        value = v
        edited(v)
    }

    Label {
        visible: root.kind !== "bool"
        text: (root.spec.label || root.spec.key) + (root.spec.required ? " *" : "")
        color: Theme.textMuted
        font.pixelSize: 12
    }

    Loader {
        Layout.fillWidth: true
        sourceComponent: {
            switch (root.kind) {
            case "bool": return boolField
            case "choice": return choiceField
            case "file":
            case "files":
            case "path": return pathField
            case "dir": return dirField
            default: return textField
            }
        }
    }

    Component {
        id: boolField
        AppCheckBox {
            text: root.spec.label || root.spec.key
            checked: !!root.value
            onToggled: root.commit(checked)
        }
    }
    Component {
        id: choiceField
        ComboBox {
            textRole: "label"
            valueRole: "value"
            // The empty choice means "use the tool's default".
            model: (root.spec.choices || []).map(c => ({ label: c === "" ? qsTr("(default)") : c, value: c }))
            currentIndex: Math.max(0, indexOfValue(root.value === undefined ? "" : String(root.value)))
            onActivated: root.commit(currentValue)
        }
    }
    Component {
        id: textField
        TextField {
            text: root.value === undefined ? "" : String(root.value)
            placeholderText: root.spec.placeholder || ""
            placeholderTextColor: Qt.alpha(Theme.text, 0.4)
            selectByMouse: true
            inputMethodHints: root.kind === "int" ? Qt.ImhDigitsOnly : Qt.ImhNone
            onTextEdited: root.commit(root.kind === "int" && text !== "" && !isNaN(Number(text)) ? Number(text) : text)
        }
    }
    Component {
        id: pathField
        PathField {
            text: root.value === undefined ? "" : String(root.value)
            multiple: root.kind === "files"
            placeholder: root.spec.placeholder || ""
            onEdited: value => root.commit(value)
        }
    }
    Component {
        id: dirField
        PathField {
            folder: true
            text: root.value === undefined ? "" : String(root.value)
            placeholder: root.spec.placeholder || ""
            onEdited: value => root.commit(value)
        }
    }
}
