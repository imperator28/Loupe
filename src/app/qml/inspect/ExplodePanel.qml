import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The exploded view's panel (#17). Laid out for someone who has never used a CAD package:
// one switch between the assembly and its exploded layout, one slider to spread the parts,
// a picture of what each handle does, and everything precise tucked under Advanced.
Rectangle {
    id: root
    property QtObject taskController
    property QtObject documentController
    // The Inspect viewport, for the Assembled / Exploded switch and the handle's pivot.
    property Item viewport: null
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    signal closeRequested()
    signal autoExplodeRequested(string scopeId, real factor)
    signal resetAllRequested()
    signal exportRequested()

    readonly property var selectedNodeIds: documentController && documentController.selectedNodeIds
                                           ? documentController.selectedNodeIds : []
    readonly property string selectedNodeId: selectedNodeIds.length === 1 ? selectedNodeIds[0] : ""
    readonly property string scopeId: taskController ? taskController.autoExplodeScope(selectedNodeIds) : ""
    readonly property int revision: taskController ? taskController.revision : 0

    // Where a picked pivot snapped to, as the pick's hover named it.
    function pivotDescription(snap) {
        switch (snap) {
        case "vertex": return qsTr("At a corner")
        case "midpoint": return qsTr("At the middle of an edge")
        case "centre": return qsTr("At a hole's or arc's centre, on its axis")
        case "edgePoint": return qsTr("At a point on an edge")
        case "surfacePoint": return qsTr("At a point on a face")
        default: return qsTr("At the point you picked")
        }
    }
    readonly property real scopeFactor: taskController && revision >= 0 ? taskController.autoFactor(scopeId) : 0
    readonly property bool hasLayout: taskController !== null && taskController.exploded
    // The view shows the layout (rather than easing to, or resting on, the assembly).
    readonly property bool showingLayout: hasLayout && taskController.blend > 0.5
    // On the assembly with a layout kept aside: what a drag there does is not obvious, so say.
    readonly property bool showingKeptAssembly: hasLayout && !showingLayout
    readonly property QtObject store: documentController ? documentController.savedViews : null
    readonly property var savedLayouts: store ? store.explodeLayouts : []
    readonly property vector3d selectedOffset: taskController && selectedNodeId.length > 0 && revision >= 0
                                               ? taskController.offsetOf(selectedNodeId) : Qt.vector3d(0, 0, 0)
    readonly property vector3d selectedAngles: taskController && selectedNodeId.length > 0 && revision >= 0
                                               ? taskController.anglesOf(selectedNodeId) : Qt.vector3d(0, 0, 0)
    readonly property var translationSteps: [1, 5, 10, 25]
    readonly property var rotationSteps: [5, 15, 45, 90]
    readonly property string modifierName: Qt.platform.os === "osx" ? "⌘" : "Ctrl"
    property bool advancedOpen: false
    // The node whose explode line the selected part's is (itself, or the moved sub-assembly
    // it goes with).
    readonly property string lineOwner: taskController && selectedNodeId.length > 0 && revision >= 0
                                        ? taskController.lineOwnerOf(selectedNodeId) : ""

    function nameOf(nodeId) {
        const name = documentController && nodeId.length > 0 ? documentController.nodeDisplayName(nodeId) : ""
        return name.length > 0 ? name : qsTr("Component")
    }

    function formatted(value) {
        const rounded = Math.round(value * 100) / 100
        return Number.isFinite(rounded) ? String(rounded) : "0"
    }

    // Exploded: show the layout, or make one by spreading the parts if there is none yet.
    function showExploded() {
        if (hasLayout) {
            if (viewport) viewport.showExploded()
            return
        }
        autoExplodeRequested(scopeId, Math.max(1, scopeFactor))
    }

    function showAssembled() {
        if (viewport) viewport.showAssembled()
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

    implicitWidth: 340
    implicitHeight: content.implicitHeight + 32
    height: implicitHeight
    radius: root.theme.radius4
    // Inside a floating tool window, which draws the frame and the title bar with its own
    // close and minimise controls.
    property bool embedded: false
    color: embedded ? "transparent" : root.theme.surfaceRaised
    border.color: embedded ? "transparent" : root.theme.border

    component SectionTitle: Label {
        Layout.fillWidth: true
        Layout.topMargin: 4
        font.bold: true
        color: root.foreground
    }

    component Legend: RowLayout {
        property string icon
        property string text
        property color tint: root.foreground
        Layout.fillWidth: true
        spacing: root.theme.spacing2
        ToolIcon { name: parent.icon; color: parent.tint }
        Label {
            Layout.fillWidth: true
            text: parent.text
            wrapMode: Text.WordWrap
            color: root.foreground
        }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            visible: !root.embedded
            Label { text: qsTr("Exploded view"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            IconButton { theme: root.theme; glyph: "close"; Accessible.name: qsTr("Close exploded view"); onClicked: root.closeRequested() }
        }

        // 1. The two states, one click apart. The layout is kept either way.
        RowLayout {
            Layout.fillWidth: true
            spacing: 4
            // One switch between the two states, the design's mode picker.
            ThemedSegmentedControl {
                id: stateSwitch
                objectName: "explodeStateSwitch"
                theme: root.theme
                Layout.fillWidth: true
                model: [qsTr("Assembled"), qsTr("Exploded")]
                currentIndex: root.showingLayout ? 1 : 0
                Accessible.name: qsTr("Assembled or exploded")
                onActivated: function(index) {
                    if (index === 1) root.showExploded()
                    else if (root.hasLayout) root.showAssembled()
                }
            }
        }
        Label {
            Layout.fillWidth: true
            visible: !root.hasLayout
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            text: qsTr("Exploded spreads the parts apart; switch back any time to compare.")
        }
        Label {
            objectName: "explodeAssemblyHint"
            Layout.fillWidth: true
            visible: root.showingKeptAssembly
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            text: qsTr("Showing the assembly. Exploded brings your layout back; moving a part here starts a new one, and your layout is saved below first.")
        }

        // Layouts to come back to: a spread, a hinge turned open, a part pulled out.
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing2
            SectionTitle { text: qsTr("Saved layouts") }
            ThemedButton {
                objectName: "explodeSaveLayoutButton"
                theme: root.theme
                text: qsTr("Save")
                leftPadding: 28
                enabled: root.viewport !== null && root.showingLayout
                onClicked: {
                    const id = root.viewport.saveExplodeLayout("")
                    if (id.length > 0) layoutList.editingId = id
                }
                ToolIcon {
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    name: "add"
                    color: parent.enabled ? root.foreground : root.subduedForeground
                }
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Keep where every part is now, to come back to in one click")
                    visible: parent.hovered
                }
            }
        }
        SavedViewList {
            id: layoutList
            objectName: "explodeLayoutList"
            theme: root.theme
            store: root.store
            Layout.fillWidth: true
            Layout.preferredHeight: implicitHeight
            entries: root.savedLayouts
            activeId: root.viewport ? root.viewport.activeExplodeLayoutId : ""
            visibleRows: 3
            placeholderIcon: "explode"
            emptyText: qsTr("Save a layout to come back to it in one click.")
            canSaveOver: true
            saveOverEnabled: root.showingLayout
            onActivated: function(id) { if (root.viewport) root.viewport.goToExplodeLayout(id) }
            onSaveOverRequested: function(id) { if (root.viewport) root.viewport.updateExplodeLayout(id) }
        }

        // 2. How far apart.
        SectionTitle { text: qsTr("Spread") }
        Label {
            objectName: "explodeScopeLabel"
            Layout.fillWidth: true
            color: root.subduedForeground
            wrapMode: Text.WordWrap
            text: root.scopeId.length > 0 ? qsTr("Sub-assembly: %1").arg(root.nameOf(root.scopeId))
                                          : qsTr("Whole model (select a sub-assembly to spread just its parts)")
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing2
            Label { text: qsTr("Together"); color: root.subduedForeground; font.pixelSize: root.theme.fontCaption }
            ThemedSlider {
                id: factorSlider
                objectName: "explodeFactorSlider"
                Layout.fillWidth: true
                theme: root.theme
                from: 0
                to: 3
                stepSize: 0.05
                value: root.scopeFactor
                Accessible.name: qsTr("How far apart")
                onPressedChanged: {
                    if (!root.taskController) return
                    if (pressed) root.taskController.beginAutoExplode(root.scopeId)
                    else root.taskController.commitInteraction()
                }
                onMoved: if (root.taskController && root.taskController.interacting) root.taskController.previewAutoExplode(value)
            }
            Label { text: qsTr("Apart"); color: root.subduedForeground; font.pixelSize: root.theme.fontCaption }
        }

        // 3. Moving parts by hand.
        SectionTitle { text: qsTr("Move parts") }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.foreground
            text: root.selectedNodeIds.length > 1 ? qsTr("%1 parts selected: they move together.").arg(root.selectedNodeIds.length)
                : root.selectedNodeId.length > 0 ? qsTr("Selected: %1").arg(root.nameOf(root.selectedNodeId))
                : qsTr("Click a part to move it.")
        }
        Legend { icon: "handleArrow"; text: qsTr("Drag an arrow to slide along it") }
        Legend { icon: "handleBall"; text: qsTr("Drag the ball to move anywhere") }
        Legend { icon: "handleRing"; text: qsTr("Drag a ring to turn") }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            font.pixelSize: root.theme.fontCaption
            text: qsTr("Shift-click or %1-click to add parts. Hold Shift while dragging to move without snapping.").arg(root.modifierName)
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
                text: qsTr("Put back")
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
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Return the selected parts to where they belong")
                    visible: parent.hovered
                }
            }
        }

        // 4. Explode lines: from where each moved part seats to where it is.
        SectionTitle { text: qsTr("Explode lines") }
        ThemedSwitch {
            theme: root.theme
            objectName: "explodeLinesSwitch"
            Layout.fillWidth: true
            text: qsTr("Show lines to where parts go")
            checked: root.taskController !== null && root.taskController.linesShown
            onToggled: if (root.taskController) root.taskController.linesShown = checked
        }
        ThemedSegmentedControl {
            objectName: "explodeLineStyle"
            theme: root.theme
            Layout.fillWidth: true
            visible: root.taskController !== null && root.taskController.linesShown
            model: [qsTr("Dashed"), qsTr("Centerline")]
            currentIndex: root.taskController && root.taskController.lineStyle === "centerline" ? 1 : 0
            onActivated: function(index) { root.taskController.lineStyle = index === 1 ? "centerline" : "dashed" }
            Accessible.name: qsTr("Line style")
        }
        Label {
            Layout.fillWidth: true
            visible: root.taskController !== null && root.taskController.linesShown
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            font.pixelSize: root.theme.fontCaption
            text: qsTr("A line runs through the middle of each part along the way it moved; a pin or screw in its hole follows the hole's axis.")
        }
        // The selected part's own line: where it starts, and whether it is drawn.
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.lineOwner.length > 0 && root.taskController !== null && root.taskController.linesShown
            spacing: root.theme.spacing1
            ThemedButton {
                theme: root.theme
                objectName: "explodePickAnchorButton"
                Layout.fillWidth: true
                text: qsTr("Pick where its line starts")
                enabled: root.viewport !== null
                selectedAppearance: root.viewport !== null && root.viewport.pickingPivot && root.viewport.pivotPickPurpose === "anchor"
                onClicked: root.viewport.pickingPivot ? root.viewport.cancelPivotPick() : root.viewport.beginAnchorPick(root.selectedNodeId)
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Click a corner, a hole's centre or an axis for this part's line to start from")
                    visible: parent.hovered
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: root.theme.spacing1
                ThemedButton {
                    theme: root.theme
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: qsTr("From its middle")
                    enabled: {
                        const revision = root.revision
                        const owner = root.lineOwner
                        return root.taskController !== null && revision >= 0 && owner.length > 0 && root.taskController.hasLineAnchor(owner)
                    }
                    onClicked: root.taskController.clearLineAnchor(root.lineOwner)
                }
                ThemedButton {
                    theme: root.theme
                    objectName: "explodeHideLineButton"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    readonly property bool hiddenLine: {
                        const revision = root.revision
                        const owner = root.lineOwner
                        return root.taskController !== null && revision >= 0 && owner.length > 0 && root.taskController.lineHidden(owner)
                    }
                    text: hiddenLine ? qsTr("Show its line") : qsTr("Hide its line")
                    onClicked: root.taskController.setLineHidden(root.lineOwner, !hiddenLine)
                }
            }
        }

        // 5. Everything precise, for when it is needed.
        ThemedButton {
            theme: root.theme
            objectName: "explodeAdvancedButton"
            Layout.fillWidth: true
            flat: true
            text: qsTr("Advanced: exact moves, pivot, snapping")
            leftPadding: 30
            ToolIcon {
                x: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 16
                height: 16
                name: root.advancedOpen ? "chevronDown" : "chevronRight"
                color: root.foreground
            }
            onClicked: root.advancedOpen = !root.advancedOpen
        }
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.advancedOpen
            spacing: 8

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

            // Where the handle sits and turns about.
            Label { text: qsTr("Pivot"); color: root.foreground; font.bold: true }
            RowLayout {
                Layout.fillWidth: true
                ToolIcon { name: "pivot"; color: root.foreground }
                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: root.subduedForeground
                    objectName: "explodePivotDescription"
                    text: root.viewport && root.viewport.pickingPivot ? qsTr("Click a point on a part: it snaps to corners, midpoints and hole centres…")
                        : root.viewport && root.viewport.handlePivot ? root.pivotDescription(root.viewport.handlePivot.snap)
                        : qsTr("At the centre of the selection")
                }
            }
            RowLayout {
                Layout.fillWidth: true
                ThemedButton {
                    theme: root.theme
                    objectName: "explodePickPivotButton"
                    Layout.fillWidth: true
                    text: qsTr("Pick a point")
                    enabled: root.viewport !== null && root.selectedNodeIds.length > 0
                    selectedAppearance: root.viewport !== null && root.viewport.pickingPivot
                    onClicked: root.viewport.pickingPivot ? root.viewport.cancelPivotPick() : root.viewport.beginPivotPick()
                    ThemedToolTip {
                        theme: root.theme
                        text: qsTr("Click a point on a part to move and turn about it, such as a hinge")
                        visible: parent.hovered
                    }
                }
                ThemedButton {
                    theme: root.theme
                    Layout.fillWidth: true
                    text: qsTr("Use centre")
                    enabled: root.viewport !== null && root.viewport.handlePivot !== null
                    onClicked: root.viewport.clearPivot()
                }
            }
            ThemedSwitch {
                theme: root.theme
                objectName: "explodeAlignToFaceSwitch"
                text: qsTr("Line the handle up with the picked face or hole")
                enabled: root.viewport !== null && root.viewport.handlePivot !== null
                checked: root.viewport !== null && root.viewport.handleAlignToFace
                onToggled: if (root.viewport) root.viewport.handleAlignToFace = checked
            }

            Label { text: qsTr("Snapping"); color: root.foreground; font.bold: true }
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
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
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
                enabled: root.hasLayout
                onClicked: root.exportRequested()
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Save the exploded layout as STEP or STL, or as a drawing of this view")
                    visible: parent.hovered
                }
            }
            ThemedButton {
                theme: root.theme
                objectName: "explodeResetAllButton"
                text: qsTr("Reset")
                destructive: true
                enabled: root.hasLayout
                onClicked: root.resetAllRequested()
                ThemedToolTip {
                    theme: root.theme
                    text: qsTr("Put every part back and forget the layout")
                    visible: parent.hovered
                }
            }
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            font.pixelSize: root.theme.fontCaption
            text: qsTr("Only this view changes: the file is never touched. Measuring works in Assembled.")
        }
    }
}
