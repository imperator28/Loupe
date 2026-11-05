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
        { key: "point", label: qsTr("Point to point"), short: qsTr("Points"), icon: "measurePoints", picks: 2 },
        { key: "edge", label: qsTr("Edge / curve length"), short: qsTr("Edge length"), icon: "measureEdge", picks: 1 },
        { key: "surface-distance", label: qsTr("Surface to surface"), short: qsTr("Surfaces"), icon: "measureSurfaces", picks: 2 },
        { key: "radius", label: qsTr("Radius / diameter"), short: qsTr("Radius"), icon: "measureRadius", picks: 1 },
        { key: "angle", label: qsTr("Angle"), short: qsTr("Angle"), icon: "measureAngle", picks: 2 },
        { key: "area", label: qsTr("Surface area"), short: qsTr("Area"), icon: "measureArea", picks: 1 },
        { key: "volume", label: qsTr("Volume"), short: qsTr("Volume"), icon: "measureVolume", picks: 0 },
        { key: "bounds", label: qsTr("Selected bounds"), short: qsTr("Bounds"), icon: "measureBounds", picks: 0 }
    ]
    readonly property var groups: [
        { title: qsTr("Between two picks"), keys: ["point", "surface-distance", "angle"] },
        { title: qsTr("One pick"), keys: ["edge", "radius", "area"] },
        { title: qsTr("Whole part"), keys: ["volume", "bounds"] }
    ]
    function modeFor(key) {
        for (const mode of modes) if (mode.key === key) return mode
        return modes[0]
    }

    readonly property string activeKey: taskController ? taskController.modeKey : "point"
    readonly property var activeMode: modeFor(activeKey)
    readonly property int requiredPicks: taskController ? taskController.requiredPickCount : 0
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

    // A mode: its glyph over its short name, the active one tinted and outlined in accent.
    component ModeTile: AbstractButton {
        id: tile
        required property var mode
        readonly property bool active: root.activeKey === mode.key
        objectName: "measureMode-" + mode.key
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        implicitHeight: 52
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        checkable: false
        Accessible.role: Accessible.RadioButton
        Accessible.name: mode.label
        Accessible.checked: active
        onClicked: if (root.taskController) root.taskController.setModeName(mode.key)

        background: Rectangle {
            radius: root.theme.radius2
            color: tile.active ? root.theme.accentTint
                 : tile.down ? Qt.darker(root.theme.control, root.theme.dark ? 0.82 : 1.08)
                 : tile.hovered ? root.theme.control : "transparent"
            border.color: tile.active ? root.theme.accent : tile.hovered ? root.theme.border : "transparent"
            border.width: 1
            Behavior on color { ColorAnimation { duration: root.theme.durInstant } }
            // The focus ring, outside the tile.
            Rectangle {
                anchors.fill: parent
                anchors.margins: -3
                radius: parent.radius + 3
                color: "transparent"
                border.color: root.theme.accent
                border.width: root.theme.focusRingWidth
                visible: tile.visualFocus
            }
        }
        contentItem: Column {
            spacing: 3
            topPadding: 7
            ToolIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 20
                height: 20
                name: tile.mode.icon
                color: tile.active ? root.theme.accent : root.foreground
            }
            Label {
                width: tile.width - 6
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

        // 1. What to measure.
        Repeater {
            model: root.groups
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 4
                Label {
                    text: modelData.title
                    color: root.subduedForeground
                    font.pixelSize: root.theme.fontCaption
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 4
                    Repeater {
                        model: modelData.keys
                        delegate: ModeTile {
                            required property string modelData
                            mode: root.modeFor(modelData)
                        }
                    }
                    // The short row keeps its tiles the width of the others.
                    Item { visible: modelData.keys.length < 3; Layout.fillWidth: true; Layout.preferredWidth: 1 }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.theme.border }

        // 2. What has been picked.
        Label {
            objectName: "measureInstruction"
            Layout.fillWidth: true
            visible: root.requiredPicks === 0
            wrapMode: Text.WordWrap
            color: root.subduedForeground
            text: root.activeKey === "volume" ? qsTr("Select a closed solid in the view or the tree.")
                                              : qsTr("Select a part in the view or the tree.")
        }
        Repeater {
            model: root.requiredPicks
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
    }
}
