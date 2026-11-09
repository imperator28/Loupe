import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    // Keep the conditional slice controls in step with a C++ controller that is
    // attached after the Loader creates this panel.
    property bool sliceOnlyValue: false
    property string sliceDisplayValue: "filled-outline"
    property real sliceBorderWidthValue: 1.5
    readonly property color resolvedSliceBorderColor: root.taskController
            && root.taskController.sliceBorderColor.length > 0
            ? root.taskController.sliceBorderColor
            : root.theme.edge
    signal closeRequested()
    signal disableRequested()
    signal normalViewRequested()
    signal oppositeViewRequested()
    signal borderColorRequested(color currentColor)
    implicitWidth: 320
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: root.theme.radius4
    // Inside a floating tool window, which draws the frame and the title bar with its own
    // close and minimise controls.
    property bool embedded: false
    color: embedded ? "transparent" : root.theme.surfaceRaised
    border.color: embedded ? "transparent" : root.theme.border

    function syncSectionState() {
        if (!root.taskController) {
            root.sliceOnlyValue = false
            root.sliceDisplayValue = "filled-outline"
            root.sliceBorderWidthValue = 1.5
            return
        }
        root.sliceOnlyValue = root.taskController.sliceOnly
        root.sliceDisplayValue = root.taskController.sliceDisplay
        root.sliceBorderWidthValue = root.taskController.sliceBorderWidth
    }

    function selectSliceDisplay(sliceDisplay) {
        if (!root.taskController) return
        root.sliceDisplayValue = sliceDisplay
        root.taskController.sliceDisplay = sliceDisplay
    }

    function setSliceBorderWidth(width) {
        if (!root.taskController) return
        root.sliceBorderWidthValue = width
        root.taskController.sliceBorderWidth = width
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
            visible: !root.embedded
            Label { text: qsTr("Section"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            IconButton { theme: root.theme; glyph: "close"; Accessible.name: qsTr("Close section"); onClicked: root.closeRequested() }
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
                    checked: root.taskController !== null && !root.taskController.usingSelectedPlane
                             && root.taskController.axisName === modelData
                    Layout.fillWidth: true
                    onClicked: root.taskController.axisName = modelData
                }
            }
        }
        // A face's plane: the button asks for a face in the view, which previews its plane
        // under the pointer; the face picked is named here, and stays lit in the view.
        ThemedButton {
            id: pickPlaneButton
            objectName: "sectionPickFacePlane"
            theme: root.theme
            Layout.fillWidth: true
            readonly property bool picking: root.taskController !== null && root.taskController.pickingPlane
            text: picking ? qsTr("Click a flat face or a hole in the view…") : qsTr("Pick face plane or axis")
            selectedAppearance: picking || (root.taskController !== null && root.taskController.usingSelectedPlane)
            onClicked: {
                if (!root.taskController) return
                if (picking) root.taskController.cancelPlanePick()
                else root.taskController.beginPlanePick()
            }
            ThemedToolTip {
                theme: root.theme
                text: pickPlaneButton.picking ? qsTr("Esc cancels")
                                              : qsTr("Section along a flat face you click, or about the axis of a round one")
                visible: pickPlaneButton.hovered
            }
        }
        Label {
            objectName: "sectionPlaneSource"
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.taskController && root.taskController.planeLabel.length > 0
                  ? (root.taskController.usingAxis ? qsTr("About: %1") : qsTr("Plane of: %1")).arg(root.taskController.planeLabel) : ""
            elide: Text.ElideRight
            color: root.subduedForeground
            font.pixelSize: root.theme ? root.theme.fontCaption : 11
        }
        // About an axis: across it (the default, placed where it was picked; the position slider
        // moves it along the axis), or through it, turned to any angle about it.
        ColumnLayout {
            objectName: "sectionAxisControls"
            Layout.fillWidth: true
            visible: root.taskController !== null && root.taskController.usingAxis
            spacing: root.theme ? root.theme.spacing2 : 8
            ThemedSegmentedControl {
                objectName: "sectionAxisMode"
                theme: root.theme
                Layout.fillWidth: true
                model: [qsTr("Across the axis"), qsTr("Through the axis")]
                currentIndex: root.taskController && root.taskController.axisMode === "through" ? 1 : 0
                onActivated: function(index) { root.taskController.axisMode = index === 1 ? "through" : "across" }
                Accessible.name: qsTr("Section about the axis")
            }
            RowLayout {
                Layout.fillWidth: true
                visible: root.taskController !== null && root.taskController.axisMode === "through"
                spacing: root.theme ? root.theme.spacing2 : 8
                Label { text: qsTr("Angle"); color: root.foreground }
                ThemedSlider {
                    objectName: "sectionAxisAngleSlider"
                    theme: root.theme
                    Layout.fillWidth: true
                    from: 0
                    to: 360
                    stepSize: 1
                    value: root.taskController ? root.taskController.axisAngle : 0
                    Accessible.name: qsTr("Angle about the axis")
                    onMoved: root.taskController.axisAngle = value
                }
                ThemedSpinBox {
                    objectName: "sectionAxisAngleField"
                    theme: root.theme
                    editable: true
                    from: 0
                    to: 359
                    stepSize: 15
                    wrap: true
                    value: root.taskController ? Math.round(root.taskController.axisAngle) % 360 : 0
                    textFromValue: function(value) { return qsTr("%1°").arg(value) }
                    valueFromText: function(text) { return Number(text.replace("°", "")) }
                    onValueModified: root.taskController.axisAngle = value
                }
            }
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
            Label { text: qsTr("Position"); Layout.fillWidth: true; color: root.foreground }
            ThemedSpinBox {
                id: position
                theme: root.theme
                from: -100000
                to: 100000
                value: root.taskController ? Math.round(root.taskController.position) : 0
                editable: true
                onValueModified: root.taskController.position = value
            }
        }
        ThemedSwitch { theme: root.theme; text: qsTr("Flip section"); checked: root.taskController && root.taskController.flipped; onToggled: root.taskController.flipped = checked }
        ThemedSwitch {
            theme: root.theme
            text: qsTr("Cap cut faces")
            checked: root.taskController && root.taskController.capEnabled
            onToggled: if (root.taskController) root.taskController.capEnabled = checked
        }
        ThemedSwitch {
            id: sliceOnlySwitch
            theme: root.theme
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
            ColumnLayout {
                Layout.fillWidth: true
                visible: root.sliceDisplayValue !== "filled"
                spacing: root.theme ? root.theme.spacing2 : 8

                Label {
                    text: qsTr("Section border")
                    color: root.subduedForeground
                    Layout.fillWidth: true
                }
                ThemedColorButton {
                    theme: root.theme
                    actionText: qsTr("Border color")
                    selectedColor: root.resolvedSliceBorderColor
                    Layout.fillWidth: true
                    onClicked: root.borderColorRequested(root.resolvedSliceBorderColor)
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.theme ? root.theme.spacing2 : 8

                    Basic.Slider {
                        id: borderWidthSlider
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        implicitHeight: root.theme && root.theme.controlHeight !== undefined
                                        ? root.theme.controlHeight + 2 : 34
                        from: 0.5
                        to: 8.0
                        stepSize: 0.1
                        value: root.sliceBorderWidthValue
                        Accessible.name: qsTr("Section border thickness")
                        onMoved: root.setSliceBorderWidth(value)

                        background: Rectangle {
                            x: borderWidthSlider.leftPadding
                            y: borderWidthSlider.topPadding
                                    + borderWidthSlider.availableHeight / 2 - height / 2
                            implicitWidth: 160
                            implicitHeight: 4
                            width: borderWidthSlider.availableWidth
                            height: 4
                            radius: 2
                            color: root.theme.control

                            Rectangle {
                                width: parent.width * borderWidthSlider.visualPosition
                                height: parent.height
                                radius: parent.radius
                                color: root.theme.accent
                            }
                        }
                        handle: Rectangle {
                            x: borderWidthSlider.leftPadding + borderWidthSlider.visualPosition
                                    * (borderWidthSlider.availableWidth - width)
                            y: borderWidthSlider.topPadding
                                    + borderWidthSlider.availableHeight / 2 - height / 2
                            implicitWidth: 18
                            implicitHeight: 18
                            radius: 9
                            color: borderWidthSlider.pressed ? Qt.darker(root.theme.accent, 1.08)
                                                             : root.theme.accent
                            border.color: root.theme.surfaceRaised
                            border.width: 2
                        }
                    }
                    ThemedSpinBox {
                        theme: root.theme
                        Layout.preferredWidth: 92
                        editable: true
                        from: 5
                        to: 80
                        stepSize: 1
                        value: Math.round(root.sliceBorderWidthValue * 10)
                        textFromValue: function(value) { return qsTr("%1 px").arg((value / 10).toFixed(1)) }
                        valueFromText: function(text) { return Number(text.replace("px", "").trim()) * 10 }
                        onValueModified: root.setSliceBorderWidth(value / 10)
                    }
                }
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
