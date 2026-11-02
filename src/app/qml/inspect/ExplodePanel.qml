import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts

// The exploded view's panel (#17): auto-explode, the selection's exact position and turn,
// snapping, undo, and reset. The handles in the viewport do the dragging.
Rectangle {
    id: root
    property QtObject taskController
    property QtObject documentController
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    signal closeRequested()
    signal autoExplodeRequested(string scopeId, real factor)
    signal resetAllRequested()

    readonly property var selectedNodeIds: documentController && documentController.selectedNodeIds
                                           ? documentController.selectedNodeIds : []
    readonly property string selectedNodeId: selectedNodeIds.length === 1 ? selectedNodeIds[0] : ""
    readonly property string scopeId: taskController ? taskController.autoExplodeScope(selectedNodeIds) : ""
    readonly property int revision: taskController ? taskController.revision : 0
    readonly property real scopeFactor: taskController && revision >= 0 ? taskController.autoFactor(scopeId) : 0
    readonly property vector3d selectedOffset: taskController && selectedNodeId.length > 0 && revision >= 0
                                               ? taskController.offsetOf(selectedNodeId) : Qt.vector3d(0, 0, 0)
    readonly property vector3d selectedAngles: taskController && selectedNodeId.length > 0 && revision >= 0
                                               ? taskController.anglesOf(selectedNodeId) : Qt.vector3d(0, 0, 0)
    readonly property var translationSteps: [1, 5, 10, 25]
    readonly property var rotationSteps: [5, 15, 45, 90]

    function nameOf(nodeId) {
        const name = documentController && nodeId.length > 0 ? documentController.nodeDisplayName(nodeId) : ""
        return name.length > 0 ? name : qsTr("Component")
    }

    function formatted(value) {
        const rounded = Math.round(value * 100) / 100
        return Number.isFinite(rounded) ? String(rounded) : "0"
    }

    function setOffsetAxis(axis, text) {
        const value = Number(text)
        if (!taskController || selectedNodeId.length === 0 || !Number.isFinite(value)) return
        const offset = selectedOffset
        taskController.setOffset(selectedNodeId, Qt.vector3d(axis === 0 ? value : offset.x,
                                                             axis === 1 ? value : offset.y,
                                                             axis === 2 ? value : offset.z))
    }

    function setAngleAxis(axis, text) {
        const value = Number(text)
        if (!taskController || selectedNodeId.length === 0 || !Number.isFinite(value)) return
        const angles = selectedAngles
        taskController.setAngles(selectedNodeId, Qt.vector3d(axis === 0 ? value : angles.x,
                                                             axis === 1 ? value : angles.y,
                                                             axis === 2 ? value : angles.z))
    }

    // Moves the selection by the X, Y and Z typed into "Move by" (mm; empty is 0).
    function applyMoveBy() {
        if (!taskController || selectedNodeIds.length === 0) return
        const values = []
        for (let axis = 0; axis < 3; ++axis) {
            const field = moveByFields.itemAt(axis)
            const value = field && field.text.length > 0 ? Number(field.text) : 0
            values.push(Number.isFinite(value) ? value : 0)
        }
        taskController.translateBy(selectedNodeIds, Qt.vector3d(values[0], values[1], values[2]))
    }

    signal exportRequested()

    implicitWidth: 340
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: root.theme.radius4
    color: root.theme.surfaceRaised
    border.color: root.theme.border

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Exploded view"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            ThemedToolButton { theme: root.theme; text: "×"; Accessible.name: qsTr("Close exploded view"); onClicked: root.closeRequested() }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            text: qsTr("Select a part, or a sub-assembly in the tree. Drag an arrow to move it along an axis, the ball in the middle to move it freely, or a ring to turn it. Hold Shift to skip snapping.")
        }

        Label { text: qsTr("Auto-explode"); color: root.subduedForeground }
        Label {
            objectName: "explodeScopeLabel"
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: root.foreground
            text: root.scopeId.length > 0 ? qsTr("Sub-assembly: %1").arg(root.nameOf(root.scopeId)) : qsTr("Whole model")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing2
            Basic.Slider {
                id: factorSlider
                objectName: "explodeFactorSlider"
                Layout.fillWidth: true
                implicitHeight: root.theme && root.theme.controlHeight !== undefined ? root.theme.controlHeight + 2 : 34
                from: 0
                to: 3
                stepSize: 0.05
                value: root.scopeFactor
                Accessible.name: qsTr("Explode distance")
                onPressedChanged: {
                    if (!root.taskController) return
                    if (pressed) root.taskController.beginAutoExplode(root.scopeId)
                    else root.taskController.commitInteraction()
                }
                onMoved: if (root.taskController && root.taskController.interacting) root.taskController.previewAutoExplode(value)

                background: Rectangle {
                    x: factorSlider.leftPadding
                    y: factorSlider.topPadding + factorSlider.availableHeight / 2 - height / 2
                    implicitWidth: 160
                    implicitHeight: 4
                    width: factorSlider.availableWidth
                    height: 4
                    radius: 2
                    color: root.theme.control
                    Rectangle {
                        width: parent.width * factorSlider.visualPosition
                        height: parent.height
                        radius: parent.radius
                        color: root.theme.accent
                    }
                }
                handle: Rectangle {
                    x: factorSlider.leftPadding + factorSlider.visualPosition * (factorSlider.availableWidth - width)
                    y: factorSlider.topPadding + factorSlider.availableHeight / 2 - height / 2
                    implicitWidth: 18
                    implicitHeight: 18
                    radius: 9
                    color: factorSlider.pressed ? Qt.darker(root.theme.accent, 1.08) : root.theme.accent
                    border.color: root.theme.surfaceRaised
                    border.width: 2
                }
            }
            Label {
                Layout.preferredWidth: 40
                horizontalAlignment: Text.AlignRight
                color: root.foreground
                text: qsTr("%1×").arg(factorSlider.value.toFixed(1))
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ThemedButton {
                theme: root.theme
                objectName: "explodeAutoButton"
                Layout.fillWidth: true
                text: qsTr("Explode")
                onClicked: root.autoExplodeRequested(root.scopeId, Math.max(1, root.scopeFactor + 0.5))
            }
            ThemedButton {
                theme: root.theme
                Layout.fillWidth: true
                text: qsTr("Collapse")
                enabled: root.scopeFactor > 0
                onClicked: root.autoExplodeRequested(root.scopeId, 0)
            }
        }

        Label {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: root.subduedForeground
            text: root.selectedNodeId.length > 0 ? qsTr("Selected: %1").arg(root.nameOf(root.selectedNodeId))
                : root.selectedNodeIds.length > 1 ? qsTr("%1 selected: the handles move them together").arg(root.selectedNodeIds.length)
                : qsTr("Nothing selected")
        }
        GridLayout {
            Layout.fillWidth: true
            columns: 4
            columnSpacing: root.theme.spacing1
            rowSpacing: root.theme.spacing1
            enabled: root.selectedNodeId.length > 0

            Item { Layout.preferredWidth: 1 }
            Repeater {
                model: ["X", "Y", "Z"]
                delegate: Label {
                    required property string modelData
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    color: root.subduedForeground
                }
            }
            Label { text: qsTr("Offset (mm)"); color: root.foreground }
            Repeater {
                model: 3
                delegate: ThemedTextField {
                    required property int index
                    theme: root.theme
                    objectName: "explodeOffsetField" + index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignRight
                    validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                    text: root.formatted(index === 0 ? root.selectedOffset.x : index === 1 ? root.selectedOffset.y : root.selectedOffset.z)
                    onEditingFinished: root.setOffsetAxis(index, text)
                }
            }
            Label { text: qsTr("Turn (°)"); color: root.foreground }
            Repeater {
                model: 3
                delegate: ThemedTextField {
                    required property int index
                    theme: root.theme
                    objectName: "explodeAngleField" + index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 60
                    horizontalAlignment: Text.AlignRight
                    validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                    text: root.formatted(index === 0 ? root.selectedAngles.x : index === 1 ? root.selectedAngles.y : root.selectedAngles.z)
                    onEditingFinished: root.setAngleAxis(index, text)
                }
            }
        }
        // An exact move, from wherever the selection is now: for several parts at once too.
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing1
            enabled: root.selectedNodeIds.length > 0
            Label { text: qsTr("Move by"); color: root.foreground; Layout.preferredWidth: 62 }
            Repeater {
                id: moveByFields
                model: 3
                delegate: ThemedTextField {
                    required property int index
                    theme: root.theme
                    objectName: "explodeMoveByField" + index
                    Layout.fillWidth: true
                    Layout.preferredWidth: 52
                    horizontalAlignment: Text.AlignRight
                    placeholderText: ["X", "Y", "Z"][index]
                    validator: DoubleValidator { notation: DoubleValidator.StandardNotation }
                    onAccepted: root.applyMoveBy()
                }
            }
            ThemedButton {
                theme: root.theme
                objectName: "explodeMoveByButton"
                text: qsTr("Move")
                onClicked: root.applyMoveBy()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            ThemedButton {
                theme: root.theme
                Layout.fillWidth: true
                text: qsTr("Select parent")
                enabled: root.taskController && root.selectedNodeId.length > 0
                         && root.taskController.parentOf(root.selectedNodeId).length > 0
                onClicked: root.documentController.selectNode(root.taskController.parentOf(root.selectedNodeId), false)
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Move the whole sub-assembly this part is in")
                    visible: parent.hovered
                }
            }
            ThemedButton {
                theme: root.theme
                Layout.fillWidth: true
                text: qsTr("Reset selected")
                enabled: {
                    const revision = root.revision
                    if (!root.taskController || revision < 0) return false
                    for (const nodeId of root.selectedNodeIds) {
                        if (root.taskController.canReset(nodeId)) return true
                    }
                    return false
                }
                onClicked: {
                    for (const nodeId of root.selectedNodeIds) root.taskController.resetNode(nodeId)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing2
            ThemedSwitch {
                theme: root.theme
                text: qsTr("Snap")
                checked: root.taskController && root.taskController.snapEnabled
                onToggled: if (root.taskController) root.taskController.snapEnabled = checked
            }
            Item { Layout.fillWidth: true }
            ThemedComboBox {
                theme: root.theme
                implicitWidth: 86
                enabled: root.taskController && root.taskController.snapEnabled
                model: root.translationSteps.map(function(step) { return qsTr("%1 mm").arg(step) })
                currentIndex: root.taskController ? Math.max(0, root.translationSteps.indexOf(root.taskController.translationStep)) : 1
                Accessible.name: qsTr("Move step")
                onActivated: function(index) { root.taskController.translationStep = root.translationSteps[index] }
            }
            ThemedComboBox {
                theme: root.theme
                implicitWidth: 76
                enabled: root.taskController && root.taskController.snapEnabled
                model: root.rotationSteps.map(function(step) { return qsTr("%1°").arg(step) })
                currentIndex: root.taskController ? Math.max(0, root.rotationSteps.indexOf(root.taskController.rotationStep)) : 1
                Accessible.name: qsTr("Turn step")
                onActivated: function(index) { root.taskController.rotationStep = root.rotationSteps[index] }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            ThemedButton {
                theme: root.theme
                text: qsTr("Undo")
                enabled: root.taskController && root.taskController.canUndo
                onClicked: root.taskController.undo()
            }
            ThemedButton {
                theme: root.theme
                text: qsTr("Redo")
                enabled: root.taskController && root.taskController.canRedo
                onClicked: root.taskController.redo()
            }
            Item { Layout.fillWidth: true }
            ThemedButton {
                theme: root.theme
                objectName: "explodeExportButton"
                text: qsTr("Export…")
                enabled: root.taskController && root.taskController.exploded
                onClicked: root.exportRequested()
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Save the exploded assembly as STEP or STL, or as a drawing of the current view")
                    visible: parent.hovered
                }
            }
            ThemedButton {
                theme: root.theme
                objectName: "explodeResetAllButton"
                text: qsTr("Reset all")
                enabled: root.taskController && root.taskController.exploded
                onClicked: root.resetAllRequested()
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            text: qsTr("Moving parts never changes the file or its other exports: Export… saves the exploded layout on its own. Measuring is off while parts are moved.")
        }
    }
}
