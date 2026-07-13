import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    signal closeRequested()
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
            Label { text: qsTr("Section"); font.bold: true; Layout.fillWidth: true }
            ToolButton { text: "×"; Accessible.name: qsTr("Close section"); onClicked: root.closeRequested() }
        }
        Label { text: qsTr("Plane"); color: "#b6c2cc" }
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: ["X", "Y", "Z"]
                delegate: Button {
                    required property string modelData
                    text: modelData
                    checkable: true
                    checked: root.taskController && root.taskController.axisName === modelData
                    Layout.fillWidth: true
                    onClicked: root.taskController.axisName = modelData
                }
            }
        }
        Button {
            Layout.fillWidth: true
            text: qsTr("Use selected planar face")
            enabled: false
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Select a planar face in the canvas to enable this option")
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Position"); Layout.fillWidth: true }
            SpinBox {
                id: position
                from: -100000
                to: 100000
                value: root.taskController ? Math.round(root.taskController.position) : 0
                editable: true
                onValueModified: root.taskController.position = value
            }
        }
        Switch { text: qsTr("Flip section"); checked: root.taskController && root.taskController.flipped; onToggled: root.taskController.flipped = checked }
        Switch { text: qsTr("Cap cut faces"); checked: !root.taskController || root.taskController.capEnabled; onToggled: root.taskController.capEnabled = checked }
        Switch { text: qsTr("2D slice only"); checked: root.taskController && root.taskController.sliceOnly; onToggled: root.taskController.sliceOnly = checked }
        Label { text: qsTr("Sectioning affects only the Inspect view; export geometry remains unchanged."); Layout.fillWidth: true; wrapMode: Text.WordWrap; color: "#7e8d99" }
    }
}
