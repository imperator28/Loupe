import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    signal closeRequested()
    implicitWidth: 336
    implicitHeight: content.implicitHeight + 32
    radius: 10
    color: "#182027"
    border.color: "#34414b"

    readonly property var modes: [
        { key: "point", label: qsTr("Point to point") },
        { key: "edge", label: qsTr("Edge / curve length") },
        { key: "surface-distance", label: qsTr("Surface to surface") },
        { key: "radius", label: qsTr("Radius / diameter") },
        { key: "angle", label: qsTr("Angle") },
        { key: "area", label: qsTr("Surface area") },
        { key: "volume", label: qsTr("Volume") },
        { key: "bounds", label: qsTr("Selected bounds") }
    ]

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Measure"); font.bold: true; Layout.fillWidth: true }
            ToolButton { text: "×"; Accessible.name: qsTr("Close measurement"); onClicked: root.closeRequested() }
        }
        Label {
            Layout.fillWidth: true
            text: root.taskController ? root.taskController.pickInstruction : qsTr("Choose a measurement operation")
            wrapMode: Text.WordWrap
            color: "#b6c2cc"
        }
        GridLayout {
            columns: 2
            Layout.fillWidth: true
            columnSpacing: 8
            rowSpacing: 8
            Repeater {
                model: root.modes
                delegate: Button {
                    required property var modelData
                    text: modelData.label
                    Layout.fillWidth: true
                    Accessible.name: modelData.label
                    onClicked: root.taskController.setModeName(modelData.key)
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Pick geometry in the canvas. Esc clears the active measurement.")
            wrapMode: Text.WordWrap
            color: "#7e8d99"
        }
    }
}
