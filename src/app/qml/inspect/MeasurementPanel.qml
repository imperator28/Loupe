import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The Measure tool's panel. Top to bottom, the order a measurement is made in:
//   1. what to measure -- the eight modes, grouped by what each needs: two picks, one pick,
//      or a whole part; the active one is marked;
//   2. what has been picked -- a numbered slot per pick, matching the numbered markers in
//      the view, the next empty one saying what it wants; a pick the mode cannot use says
//      why here as well as by the pointer;
//   3. the result -- its value large, at fixed precision with its unit; the related values
//      (ΔX ΔY ΔZ, diameter, supplement, minimum); and how it was found, so a gap square to
//      two parallel faces is never mistaken for a distance between two clicks.
Rectangle {
    id: root
    property QtObject taskController
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    readonly property color tertiaryForeground: theme ? theme.textTertiary : "transparent"
    signal closeRequested()
    implicitWidth: 336
    implicitHeight: content.implicitHeight + 32
    radius: root.theme.radius4
    // Inside a floating tool window, which draws the frame and the title bar with its own
    // close and minimise controls.
    property bool embedded: false
    color: embedded ? "transparent" : root.theme.surfaceRaised
    border.color: embedded ? "transparent" : root.theme.border

    // Every mode the handoff names, in the controller's order. `picks` is how many it needs.
    readonly property var modes: [
        { key: "point", label: qsTr("Point to point"), short: qsTr("Points"), icon: "measurePoints", picks: 2,
          hint: qsTr("Between two points. Snaps to vertices, circle centres and edges.") },
        { key: "edge", label: qsTr("Edge / curve length"), short: qsTr("Edge"), icon: "measureEdge", picks: 1,
          hint: qsTr("Along an edge. Shift or %1-click more edges to add them up.").arg(root.modifierName) },
        { key: "surface-distance", label: qsTr("Surface to surface"), short: qsTr("Surfaces"), icon: "measureSurfaces", picks: 2,
          hint: qsTr("Between two faces: the gap when they are parallel, else the closest distance.") },
        { key: "radius", label: qsTr("Radius / diameter"), short: qsTr("Radius"), icon: "measureRadius", picks: 1,
          hint: qsTr("Of a circular edge, or a cylindrical or spherical face.") },
        { key: "angle", label: qsTr("Angle"), short: qsTr("Angle"), icon: "measureAngle", picks: 2,
          hint: qsTr("From one flat face or straight edge to another.") },
        { key: "area", label: qsTr("Surface area"), short: qsTr("Area"), icon: "measureArea", picks: 1,
          hint: qsTr("Of a face. Shift or %1-click more faces to add them up.").arg(root.modifierName) },
        { key: "volume", label: qsTr("Volume"), short: qsTr("Volume"), icon: "measureVolume", picks: 0,
          hint: qsTr("Of the selection, or of the whole model when nothing is selected.") },
        { key: "bounds", label: qsTr("Selected bounds"), short: qsTr("Bounds"), icon: "measureBounds", picks: 0,
          hint: qsTr("The box around the selection, or around the whole model when nothing is selected.") }
    ]
    // As they sit in the picker: two picks along the top, one pick and whole parts below.
    readonly property var order: ["point", "surface-distance", "angle", "radius", "edge", "area", "volume", "bounds"]
    readonly property string modifierName: Qt.platform.os === "osx" ? "⌘" : "Ctrl"
    function modeFor(key) {
        for (const mode of modes) if (mode.key === key) return mode
        return modes[0]
    }

    readonly property string activeKey: taskController ? taskController.modeKey : "point"
    readonly property var activeMode: modeFor(activeKey)
    readonly property int requiredPicks: taskController ? taskController.requiredPickCount : 0
    readonly property bool multiPick: taskController !== null && taskController.multiPick
    // A total shows a row per pick so far, and one asking for the first.
    readonly property int slotCount: multiPick ? Math.max(1, picked.length) : requiredPicks
    readonly property var pinned: taskController ? taskController.pinnedResults : []
    readonly property var picked: taskController ? taskController.pickedEntities : []
    readonly property bool hasResult: taskController !== null && taskController.resultText.length > 0
    readonly property string refusal: taskController ? taskController.refusal : ""

    // What the next empty slot asks for.
    function slotPrompt(index) {
        const key = root.activeKey
        if (key === "point") return index === 0 ? qsTr("First point") : qsTr("Second point")
        if (key === "surface-distance") return index === 0 ? qsTr("First face") : qsTr("Second face")
        if (key === "angle") return index === 0 ? qsTr("Flat face or straight edge") : qsTr("Second face or edge")
        if (key === "edge") return qsTr("An edge")
        if (key === "radius") return qsTr("A circular edge or round face")
        if (key === "area") return qsTr("A face")
        return ""
    }

    // A mode in the picker: its glyph over its short name. The picker is one segmented well,
    // as the design's mode pickers are; the active mode is its raised thumb, with the glyph in
    // accent and the name in weight, so it does not read by colour alone.
    component ModeTile: AbstractButton {
        id: tile
        required property var mode
        readonly property bool active: root.activeKey === mode.key
        objectName: "measureMode-" + mode.key
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: 42
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.role: Accessible.RadioButton
        Accessible.name: mode.label
        Accessible.checked: active
        onClicked: if (root.taskController) root.taskController.setModeName(mode.key)

        background: Rectangle {
            radius: root.theme.radius2
            color: tile.active ? root.theme.surface1
                 : tile.down ? root.theme.control
                 : tile.hovered ? Qt.rgba(root.theme.accentTint.r, root.theme.accentTint.g, root.theme.accentTint.b, root.theme.accentTint.a * 0.7)
                 : "transparent"
            border.color: tile.active ? root.theme.borderPanel : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: root.theme.durInstant } }
            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                radius: parent.radius + 2
                color: "transparent"
                border.color: root.theme.accent
                border.width: root.theme.focusRingWidth
                visible: tile.visualFocus
            }
        }
        contentItem: Column {
            spacing: 1
            topPadding: 5
            ToolIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 18
                height: 18
                name: tile.mode.icon
                color: tile.active ? root.theme.accent : root.subduedForeground
            }
            Label {
                width: tile.width - 4
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                text: tile.mode.short
                elide: Text.ElideRight
                color: tile.active ? root.foreground : root.subduedForeground
                font.pixelSize: root.theme.fontCaption
                font.weight: tile.active ? Font.DemiBold : Font.Normal
            }
        }
        ThemedToolTip { theme: root.theme; text: tile.mode.label; visible: tile.hovered && tile.mode.short !== tile.mode.label }
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            visible: !root.embedded
            Label { text: qsTr("Measure"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            IconButton { theme: root.theme; glyph: "close"; Accessible.name: qsTr("Close measurement"); onClicked: root.closeRequested() }
        }

        // 1. What to measure: one well of eight, and what the active one does.
        Rectangle {
            objectName: "measureModePicker"
            Layout.fillWidth: true
            implicitHeight: modeGrid.implicitHeight + 6
            radius: root.theme.radius3
            color: root.theme.surfaceSubtle
            GridLayout {
                id: modeGrid
                anchors.fill: parent
                anchors.margins: 3
                columns: 4
                columnSpacing: 2
                rowSpacing: 2
                Repeater {
                    model: root.order
                    delegate: ModeTile {
                        required property string modelData
                        mode: root.modeFor(modelData)
                    }
                }
            }
        }
        Label {
            objectName: "measureModeHint"
            Layout.fillWidth: true
            Layout.topMargin: -4
            text: root.activeMode.hint
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            font.pixelSize: root.theme.fontCaption
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.theme.border }

        // 2. What has been picked.
        Label {
            objectName: "measureInstruction"
            Layout.fillWidth: true
            visible: root.requiredPicks === 0
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            // What it is of now, and how to change that.
            text: {
                const scope = root.taskController ? root.taskController.selectionScope : ""
                const how = qsTr("Select parts or a sub-assembly to measure just them; Shift or %1-click adds.").arg(root.modifierName)
                return scope.length > 0 ? qsTr("Of: %1. %2").arg(scope).arg(how) : how
            }
        }
        Repeater {
            model: root.slotCount
            delegate: RowLayout {
                id: slot
                required property int index
                readonly property var entry: index < root.picked.length ? root.picked[index] : null
                // The slot the next click fills: empty, and every slot before it filled.
                readonly property bool next: !root.hasResult && index === root.picked.length
                objectName: "measureSlot" + index
                Layout.fillWidth: true
                spacing: 10

                // The same numbered amber marker as in the view; empty, an outline.
                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    Layout.topMargin: 1
                    implicitWidth: 20
                    implicitHeight: 20
                    radius: 10
                    color: slot.entry ? root.theme.selectedBody : "transparent"
                    border.color: slot.entry ? root.theme.selectedBody : slot.next ? root.theme.accent : root.theme.borderStrong
                    border.width: slot.next ? 2 : 1
                    Label {
                        anchors.centerIn: parent
                        text: slot.index + 1
                        color: slot.entry ? root.theme.accentForeground : root.subduedForeground
                        font.pixelSize: root.theme.fontCaption
                        font.weight: Font.Bold
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            Layout.fillWidth: !slot.entry || !slot.entry.detail
                            text: slot.entry ? slot.entry.title : root.slotPrompt(slot.index)
                            elide: Text.ElideRight
                            color: slot.entry ? root.foreground : slot.next ? root.foreground : root.tertiaryForeground
                            font.weight: slot.entry ? Font.Medium : Font.Normal
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: slot.entry !== null && slot.entry.detail !== undefined && String(slot.entry.detail).length > 0
                            text: slot.entry && slot.entry.detail ? slot.entry.detail : ""
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideLeft
                            color: root.subduedForeground
                            font.features: { "tnum": 1 }
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: slot.entry ? (slot.entry.part || "") : slot.next ? qsTr("Click it in the view") : ""
                        elide: Text.ElideRight
                        color: root.tertiaryForeground
                        font.pixelSize: root.theme.fontCaption
                    }
                }
                // One pick out of a total.
                IconButton {
                    objectName: "measureRemovePick" + slot.index
                    theme: root.theme
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: 22
                    implicitHeight: 22
                    iconSize: 12
                    visible: root.multiPick && slot.entry !== null && root.picked.length > 1
                    glyph: "close"
                    Accessible.name: qsTr("Take %1 out of the total").arg(slot.entry ? slot.entry.title : "")
                    onClicked: root.taskController.removePick(slot.index)
                    ThemedToolTip { theme: root.theme; text: qsTr("Take out of the total"); visible: parent.hovered }
                }
            }
        }
        // A pick the mode could not use: what was wrong, and what to pick instead.
        RowLayout {
            objectName: "measureRefusal"
            Layout.fillWidth: true
            visible: root.refusal.length > 0
            spacing: 8
            ToolIcon { Layout.alignment: Qt.AlignTop; width: 16; height: 16; name: "alert"; color: root.theme.warning }
            Label {
                Layout.fillWidth: true
                text: root.refusal
                wrapMode: Text.WordWrap
                color: root.foreground
                font.pixelSize: root.theme.fontCaption
            }
        }

        // 3. The result.
        Rectangle {
            objectName: "measureResult"
            Layout.fillWidth: true
            visible: root.hasResult
            implicitHeight: resultContent.implicitHeight + 20
            radius: root.theme.radius3
            color: root.theme.surfaceSubtle

            ColumnLayout {
                id: resultContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        Layout.fillWidth: true
                        text: root.taskController ? root.taskController.resultTitle : ""
                        color: root.subduedForeground
                        font.pixelSize: root.theme.fontSection
                        font.weight: Font.DemiBold
                    }
                    IconButton {
                        objectName: "measurePin"
                        theme: root.theme
                        implicitWidth: 24
                        implicitHeight: 24
                        iconSize: 14
                        glyph: "pin"
                        enabled: root.taskController && root.taskController.canPin
                        Accessible.name: qsTr("Pin the measurement to the view")
                        onClicked: root.taskController.pinResult()
                        ThemedToolTip {
                            theme: root.theme
                            text: qsTr("Pin: keep it in the view, pointing at what was measured, while you measure the next")
                            visible: parent.hovered
                        }
                    }
                    IconButton {
                        objectName: "measureCopy"
                        theme: root.theme
                        implicitWidth: 24
                        implicitHeight: 24
                        iconSize: 14
                        glyph: "copy"
                        Accessible.name: qsTr("Copy the measurement")
                        onClicked: root.taskController.copyResult()
                        ThemedToolTip { theme: root.theme; text: qsTr("Copy the result and picks"); visible: parent.hovered }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Label {
                        objectName: "measureValue"
                        text: root.taskController ? root.taskController.resultText : ""
                        color: root.foreground
                        font.pixelSize: root.theme.fontTitle
                        font.weight: Font.DemiBold
                        font.features: { "tnum": 1 }
                    }
                    Label {
                        Layout.alignment: Qt.AlignBaseline
                        Layout.fillWidth: true
                        text: root.taskController ? root.taskController.resultUnit : ""
                        color: root.subduedForeground
                        font.pixelSize: root.theme.fontBody
                    }
                }
                readonly property var details: root.taskController ? root.taskController.resultDetails : []
                readonly property bool components: details.length >= 3 && String(details[details.length - 1].label) === "ΔZ"
                // Each related value on a row of its own: its name, then the value right-aligned.
                Repeater {
                    id: detailRepeater
                    model: resultContent.details.filter(function(entry) { return !String(entry.label).startsWith("Δ") })
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 6
                        Label { text: modelData.label; color: root.subduedForeground; font.pixelSize: root.theme.fontCaption }
                        Label {
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignRight
                            text: modelData.value
                            elide: Text.ElideLeft
                            color: root.foreground
                            font.pixelSize: root.theme.fontSection
                            font.features: { "tnum": 1 }
                        }
                    }
                }
                // A distance's three components read across, name over value, as a CAD readout.
                RowLayout {
                    objectName: "measureComponents"
                    Layout.fillWidth: true
                    visible: resultContent.components
                    spacing: 8
                    Repeater {
                        model: resultContent.details.filter(function(entry) { return String(entry.label).startsWith("Δ") })
                        delegate: ColumnLayout {
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            spacing: 0
                            Label { text: modelData.label; color: root.subduedForeground; font.pixelSize: root.theme.fontCaption }
                            Label {
                                Layout.fillWidth: true
                                text: modelData.value
                                elide: Text.ElideRight
                                color: root.foreground
                                font.pixelSize: root.theme.fontSection
                                font.features: { "tnum": 1 }
                            }
                        }
                    }
                }
                Label {
                    objectName: "measureMethod"
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.taskController ? root.taskController.resultMethod : ""
                    wrapMode: Text.WordWrap
                    color: root.tertiaryForeground
                    font.pixelSize: root.theme.fontCaption
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.requiredPicks > 0
            spacing: 8
            Label {
                Layout.fillWidth: true
                text: qsTr("Esc clears the picks.")
                wrapMode: Text.WordWrap
                color: root.tertiaryForeground
                font.pixelSize: root.theme.fontCaption
            }
            ThemedButton {
                objectName: "measureClear"
                theme: root.theme
                text: qsTr("Clear")
                enabled: root.picked.length > 0 || root.refusal.length > 0
                onClicked: root.taskController.clearPicks()
            }
        }

        // 4. What stays in the view: pinned results, any kind, for a review or a capture.
        ColumnLayout {
            objectName: "measurePinned"
            Layout.fillWidth: true
            visible: root.pinned.length > 0
            spacing: 2
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; Layout.bottomMargin: 8; color: root.theme.border }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Pinned · %1").arg(root.pinned.length)
                    color: root.foreground
                    font.pixelSize: root.theme.fontSection
                    font.weight: Font.DemiBold
                }
                ThemedButton {
                    objectName: "measureClearPinned"
                    theme: root.theme
                    flat: true
                    text: qsTr("Unpin all")
                    onClicked: root.taskController.clearPinned()
                }
            }
            Repeater {
                model: root.pinned
                delegate: RowLayout {
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 8
                    ToolIcon { width: 14; height: 14; name: "pin"; color: root.theme.selectedBody }
                    Label {
                        text: modelData.title
                        color: root.subduedForeground
                        font.pixelSize: root.theme.fontCaption
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignRight
                        text: modelData.text
                        elide: Text.ElideRight
                        color: root.foreground
                        font.pixelSize: root.theme.fontSection
                        font.features: { "tnum": 1 }
                    }
                    IconButton {
                        theme: root.theme
                        implicitWidth: 22
                        implicitHeight: 22
                        iconSize: 12
                        glyph: "close"
                        Accessible.name: qsTr("Unpin %1").arg(modelData.title)
                        onClicked: root.taskController.unpin(modelData.id)
                        ThemedToolTip { theme: root.theme; text: qsTr("Unpin"); visible: parent.hovered }
                    }
                }
            }
        }
    }
}
