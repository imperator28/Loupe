import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    signal closeRequested()
    signal captureRequested()
    implicitWidth: 320
    implicitHeight: content.implicitHeight + 32
    radius: 10
    color: "#182027"
    border.color: "#34414b"

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Capture PNG"); font.bold: true; Layout.fillWidth: true }
            ToolButton { text: "×"; Accessible.name: qsTr("Close capture"); onClicked: root.closeRequested() }
        }
        Label { text: qsTr("Background"); color: "#b6c2cc" }
        RowLayout {
            Layout.fillWidth: true
            RadioButton { text: qsTr("Transparent"); checked: !root.taskController || root.taskController.transparentBackground; onToggled: if (checked) root.taskController.transparentBackground = true }
            RadioButton { text: qsTr("Viewport solid"); checked: root.taskController && !root.taskController.transparentBackground; onToggled: if (checked) root.taskController.transparentBackground = false }
        }
        Label { text: qsTr("Scale"); color: "#b6c2cc" }
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: [1, 2, 3, 4]
                delegate: Button {
                    required property int modelData
                    text: qsTr("%1×").arg(modelData)
                    checkable: true
                    checked: root.taskController && root.taskController.scale === modelData
                    Layout.fillWidth: true
                    onClicked: root.taskController.scale = modelData
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Custom"); Layout.fillWidth: true }
            SpinBox {
                from: 100
                to: 400
                stepSize: 25
                value: root.taskController ? Math.round(root.taskController.scale * 100) : 100
                textFromValue: function(value) { return qsTr("%1×").arg((value / 100).toFixed(2)) }
                valueFromText: function(text) { return Number(text.replace("×", "")) * 100 }
                onValueModified: root.taskController.scale = value / 100
            }
        }
        Switch {
            text: qsTr("Include measurements (not yet available)")
            enabled: false
            checked: false
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Measurement overlays will be enabled when topology picking is available.")
        }
        Switch {
            text: qsTr("Include section caps (not yet available)")
            enabled: false
            checked: false
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Section caps will be enabled when cut-face generation is available.")
        }
        Label {
            text: root.taskController ? qsTr("Output: %1 × %2 PNG").arg(root.taskController.resolvedWidth).arg(root.taskController.resolvedHeight)
                                      : qsTr("Resolved output dimensions update from the active viewport.")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#7e8d99"
        }
        Button {
            text: qsTr("Save PNG…")
            Layout.fillWidth: true
            enabled: root.taskController && root.taskController.resolvedWidth > 0 && root.taskController.resolvedHeight > 0
            onClicked: root.captureRequested()
        }
    }
}
