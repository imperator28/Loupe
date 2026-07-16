import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
    signal closeRequested()
    implicitWidth: 336
    implicitHeight: content.implicitHeight + 32
    radius: 10
    color: root.theme.surfaceRaised
    border.color: root.theme.border

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
            Label { text: qsTr("Measure"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            ThemedToolButton { theme: root.theme; text: "×"; Accessible.name: qsTr("Close measurement"); onClicked: root.closeRequested() }
        }
        Label {
            Layout.fillWidth: true
            text: root.taskController ? root.taskController.pickInstruction : qsTr("Choose a measurement operation")
            wrapMode: Text.WordWrap
            color: root.subduedForeground
        }
        Label {
            Layout.fillWidth: true
            visible: root.taskController && root.taskController.firstPickDescription.length > 0
            text: root.taskController ? qsTr("First: %1").arg(root.taskController.firstPickDescription) : ""
            wrapMode: Text.WordWrap
            color: root.foreground
        }
        Label {
            Layout.fillWidth: true
            visible: root.taskController && root.taskController.secondPickDescription.length > 0
            text: root.taskController ? qsTr("Second: %1").arg(root.taskController.secondPickDescription) : ""
            wrapMode: Text.WordWrap
            color: root.foreground
        }
        ThemedButton {
            theme: root.theme
            text: qsTr("Clear picks")
            Layout.fillWidth: true
            enabled: root.taskController && (root.taskController.mode === 0 || root.taskController.mode === 2 || root.taskController.mode === 4)
            onClicked: root.taskController.clearPicks()
        }
        Label {
            Layout.fillWidth: true
            visible: root.taskController && root.taskController.resultLabel.length > 0
            text: root.taskController ? root.taskController.resultLabel : ""
            font.bold: true
            color: root.theme.accent
        }
        GridLayout {
            columns: 2
            Layout.fillWidth: true
            columnSpacing: 8
            rowSpacing: 8
            Repeater {
                model: root.modes
                delegate: ThemedButton {
                    required property var modelData
                    theme: root.theme
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
            color: root.subduedForeground
        }
    }
}
