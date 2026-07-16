import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
    // Keep the conditional slice controls in step with a C++ controller that is
    // attached after the Loader creates this panel.
    property bool sliceOnlyValue: false
    property string sliceDisplayValue: "filled-outline"
    signal closeRequested()
    signal disableRequested()
    signal normalViewRequested()
    signal oppositeViewRequested()
    implicitWidth: 320
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: 10
    color: root.theme.surfaceRaised
    border.color: root.theme.border

    function syncSectionState() {
        if (!root.taskController) {
            root.sliceOnlyValue = false
            root.sliceDisplayValue = "filled-outline"
            return
        }
        root.sliceOnlyValue = root.taskController.sliceOnly
        root.sliceDisplayValue = root.taskController.sliceDisplay
    }

    function selectSliceDisplay(sliceDisplay) {
        if (!root.taskController) return
        root.sliceDisplayValue = sliceDisplay
        root.taskController.sliceDisplay = sliceDisplay
    }

    onTaskControllerChanged: root.syncSectionState()

    Connections {
        target: root.taskController
        function onChanged() { root.syncSectionState() }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Section"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            ThemedToolButton { theme: root.theme; text: "×"; Accessible.name: qsTr("Close section"); onClicked: root.closeRequested() }
        }
        Label { text: qsTr("Plane"); color: root.subduedForeground }
        RowLayout {
            Layout.fillWidth: true
            Repeater {
                model: ["X", "Y", "Z"]
                delegate: ThemedButton {
                    required property string modelData
                    theme: root.theme
                    text: modelData
                    checkable: true
                    checked: root.taskController && root.taskController.axisName === modelData
                    Layout.fillWidth: true
                    onClicked: root.taskController.axisName = modelData
                }
            }
        }
        ThemedButton {
            theme: root.theme
            Layout.fillWidth: true
            text: qsTr("Use selected face plane")
            enabled: root.taskController && root.taskController.hasSelectedPlane
            onClicked: root.taskController.useSelectedPlane()
            ToolTip.visible: hovered
            ToolTip.text: enabled ? qsTr("Use the normal from the latest canvas face pick") : qsTr("Pick a face in the canvas first")
        }
        ThemedButton {
            theme: root.theme
            Layout.fillWidth: true
            text: qsTr("View normal to section")
            onClicked: root.normalViewRequested()
        }
        ThemedButton {
            theme: root.theme
            Layout.fillWidth: true
            text: qsTr("View opposite side")
            onClicked: root.oppositeViewRequested()
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
        Switch {
            text: qsTr("Cap cut faces")
            checked: root.taskController && root.taskController.capEnabled
            onToggled: if (root.taskController) root.taskController.capEnabled = checked
        }
        Switch {
            id: sliceOnlySwitch
            text: qsTr("2D slice only")
            checked: root.sliceOnlyValue
            onToggled: if (root.taskController) {
                root.sliceOnlyValue = checked
                root.taskController.sliceOnly = checked
                if (checked) root.taskController.capEnabled = true
                if (checked) root.normalViewRequested()
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: sliceOnlySwitch.checked
            Label {
                text: qsTr("2D slice display")
                color: root.subduedForeground
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 4
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Outline")
                    selectedAppearance: root.sliceDisplayValue === "outline"
                    Layout.fillWidth: true
                    onClicked: root.selectSliceDisplay("outline")
                }
                ThemedButton {
                    theme: root.theme
                    text: qsTr("Filled")
                    selectedAppearance: root.sliceDisplayValue === "filled"
                    Layout.fillWidth: true
                    onClicked: root.selectSliceDisplay("filled")
                }
            }
            ThemedButton {
                theme: root.theme
                text: qsTr("Filled + outline")
                selectedAppearance: root.sliceDisplayValue === "filled-outline"
                Layout.fillWidth: true
                onClicked: root.selectSliceDisplay("filled-outline")
            }
        }
        Label { text: qsTr("Sectioning affects only the Inspect view; export geometry remains unchanged."); Layout.fillWidth: true; wrapMode: Text.WordWrap; color: root.subduedForeground }
        ThemedButton {
            theme: root.theme
            Layout.fillWidth: true
            text: qsTr("Turn off section")
            onClicked: root.disableRequested()
        }
    }
}
