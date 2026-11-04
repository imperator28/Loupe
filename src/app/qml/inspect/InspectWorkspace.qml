import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Loupe.App

Item {
    id: root
    objectName: "inspectWorkspace"
    property QtObject controller
    property QtObject theme
    // Remembers the tree panel's width; optional, so tests can load this without one.
    property QtObject preferences: null
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    property string activeTask: ""
    property bool taskPanelVisible: false
    property string treeHoveredNodeId: ""
    // The row a Shift-click range runs from: the last row clicked without Shift. Kept as the
    // node, not the row number, which changes as branches open and close.
    property string treeSelectionAnchor: ""

    // A click on a tree row, the way file lists take one: alone it selects the row;
    // ⌘ (Ctrl on Windows and Linux) adds or removes it; Shift selects every row shown from
    // the anchor to it, and with ⌘ adds those rows to the selection.
    function treeClicked(row, stableId, modifiers) {
        const toggle = (modifiers & Qt.ControlModifier) || (modifiers & Qt.MetaModifier)
        const range = modifiers & Qt.ShiftModifier
        const model = root.controller ? root.controller.assemblyTree : null
        // No anchor clicked in the tree yet: the part selected in the view.
        const anchor = treeSelectionAnchor.length > 0 ? treeSelectionAnchor
                     : root.controller ? root.controller.activeNodeId : ""
        const anchorRow = range && model && anchor.length > 0
            ? assemblyTree.rowAtIndex(model.indexForStableId(anchor)) : -1
        if (range && anchorRow >= 0) {
            const ids = []
            const step = row >= anchorRow ? 1 : -1
            for (let r = anchorRow; ; r += step) {
                const id = model.stableIdAt(assemblyTree.index(r, 0))
                if (id.length > 0) ids.push(id)
                if (r === row) break
            }
            root.controller.selectNodes(ids, stableId, toggle)
            return
        }
        root.controller.selectNode(stableId, toggle || range)
        treeSelectionAnchor = stableId
    }
    signal openFileRequested()

    // -1: not enough to go on yet; 0: the projected finish has passed and work continues.
    function remainingText(seconds) {
        if (seconds < 0) return qsTr("estimating remaining time")
        if (seconds === 0) return qsTr("almost done")
        return qsTr("about %1 s remaining").arg(seconds)
    }

    // Tree search: every node whose name matches, in tree order, and the one shown.
    property var treeMatches: []
    property int treeMatchIndex: -1

    function runTreeSearch(text) {
        root.treeMatches = root.controller && root.controller.assemblyTree
            ? root.controller.assemblyTree.findNodes(text) : []
        root.treeMatchIndex = root.treeMatches.length > 0 ? 0 : -1
        root.showTreeMatch()
    }

    function stepTreeMatch(step) {
        if (root.treeMatches.length === 0) return
        root.treeMatchIndex = (root.treeMatchIndex + step + root.treeMatches.length) % root.treeMatches.length
        root.showTreeMatch()
    }

    // Selecting the match also expands its branch, scrolls to it (revealActiveNode) and
    // highlights it in the viewport, as clicking its row would.
    function showTreeMatch() {
        if (root.treeMatchIndex < 0 || !root.controller) return
        root.controller.selectNode(root.treeMatches[root.treeMatchIndex], false)
    }

    // Parts are out of place in the exploded view (#17).
    readonly property bool exploded: root.controller && root.controller.explode ? root.controller.explode.exploded : false
    // ... and on screen. In the Assembled view they are where the file has them, so measuring
    // is fine there.
    // Every selected part is transparent: Make transparent reads Make opaque.
    readonly property bool selectionTransparent: root.controller && root.controller.visibilityRevision >= 0
                                                 && root.controller.selectedNodeCount >= 0
                                                 && root.controller.isSelectionTransparent()
    readonly property bool explodedShowing: exploded && root.controller.explode.blend > 0.001

    function showTask(tool) {
        // Measuring works on the assembled model; the Measure button says so while parts
        // are moved, and its shortcut does nothing.
        if (tool === "measure" && root.explodedShowing) return
        if (root.activeTask === tool) {
            root.taskPanelVisible = !root.taskPanelVisible
            return
        }
        root.activeTask = tool
        root.taskPanelVisible = true
        if (tool === "section") root.controller.section.enabled = true
    }

    function revealActiveNode() {
        if (!root.controller || root.controller.activeNodeId === "") return
        const index = root.controller.assemblyTree.indexForStableId(root.controller.activeNodeId)
        if (!index.valid) return
        assemblyTree.expandToIndex(index)
        assemblyTree.positionViewAtIndex(index, TableView.AlignVCenter)
    }

    // Save the exploded layout (#17). The format follows the file's suffix; a drawing is seen
    // from the way the view faces now, with the screen's up as its up.
    function chooseExplodedExport() {
        if (!root.controller || !root.exploded) return
        if (root.controller.suggestedAssemblyExportFolderUrl.toString().length > 0)
            explodedExportDialog.currentFolder = root.controller.suggestedAssemblyExportFolderUrl
        explodedExportDialog.open()
    }

    Connections {
        target: root.controller
        function onExplodedExportRequested() { root.chooseExplodedExport() }
        function onSnapshotChanged() {
            root.activeTask = ""
            root.taskPanelVisible = false
            root.treeHoveredNodeId = ""
        }
        function onActiveNodeIdChanged() { Qt.callLater(root.revealActiveNode) }
    }

    // Both side panels can be dragged wider or narrower; the viewport keeps at least this.
    readonly property real viewportMinimumWidth: 360
    readonly property real sidePanelRoom: width - 4 * theme.spacing3 - viewportMinimumWidth

    RowLayout {
        id: workspaceRow
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing3

        ElevatedPanel {
            id: treePanel
            theme: root.theme
            // Draggable at its right edge, so a deep assembly's names get the room they need.
            Layout.preferredWidth: treeSplitter.effectiveWidth
            Layout.fillWidth: false
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.theme.spacing3
                spacing: root.theme.spacing2 + 2
                PanelTitle {
                    theme: root.theme
                    text: qsTr("Assembly tree")
                    IconButton {
                        theme: root.theme
                        glyph: "expandAll"
                        Accessible.name: qsTr("Expand all")
                        enabled: assemblyTree.rows > 0
                        onClicked: assemblyTree.expandRecursively()
                        ThemedToolTip { theme: root.theme; text: qsTr("Expand all"); visible: parent.hovered }
                    }
                    IconButton {
                        theme: root.theme
                        glyph: "collapseAll"
                        Accessible.name: qsTr("Collapse all")
                        enabled: assemblyTree.rows > 0
                        // Keep the top level open: collapsing the root would leave one row.
                        onClicked: { assemblyTree.collapseRecursively(); assemblyTree.expand(0) }
                        ThemedToolTip { theme: root.theme; text: qsTr("Collapse all"); visible: parent.hovered }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: root.theme.spacing1
                    ThemedTextField {
                        id: treeSearch
                        theme: root.theme
                        Layout.fillWidth: true
                        placeholderText: Qt.platform.os === "osx" ? qsTr("Find part (⌘F)") : qsTr("Find part (Ctrl+F)")
                        Accessible.name: qsTr("Find part by name")
                        enabled: root.controller && root.controller.documentState === AppState.TreeReady
                        onTextChanged: treeSearchDelay.restart()
                        // Enter steps to the next match, Shift+Enter back; a search still
                        // waiting on its typing delay runs first.
                        function step(event) {
                            if (treeSearchDelay.running) {
                                treeSearchDelay.stop()
                                root.runTreeSearch(text)
                            } else {
                                root.stepTreeMatch((event.modifiers & Qt.ShiftModifier) ? -1 : 1)
                            }
                        }
                        Keys.onReturnPressed: function(event) { step(event) }
                        Keys.onEnterPressed: function(event) { step(event) }
                        Keys.onEscapePressed: text = ""
                        Timer {
                            id: treeSearchDelay
                            interval: 150
                            onTriggered: root.runTreeSearch(treeSearch.text)
                        }
                    }
                    Label {
                        visible: treeSearch.text.trim().length > 0
                        text: root.treeMatches.length === 0 ? qsTr("None")
                              : qsTr("%1/%2").arg(root.treeMatchIndex + 1).arg(root.treeMatches.length)
                        color: root.treeMatches.length === 0 ? root.theme.error : root.subduedForeground
                    }
                    IconButton {
                        theme: root.theme
                        visible: root.treeMatches.length > 1
                        glyph: "chevronUp"
                        Accessible.name: qsTr("Previous match")
                        onClicked: root.stepTreeMatch(-1)
                    }
                    IconButton {
                        theme: root.theme
                        visible: root.treeMatches.length > 1
                        glyph: "chevronDown"
                        Accessible.name: qsTr("Next match")
                        onClicked: root.stepTreeMatch(1)
                    }
                }
                Shortcut {
                    sequences: [StandardKey.Find]
                    enabled: root.visible && treeSearch.enabled
                    onActivated: { treeSearch.forceActiveFocus(); treeSearch.selectAll() }
                }
                TreeView {
                    id: assemblyTree
                    model: root.controller ? root.controller.assemblyTree : null
                    clip: true
                    selectionBehavior: TableView.SelectRows
                    palette.text: root.theme.onSurface
                    palette.windowText: root.theme.onSurface
                    columnWidthProvider: function() { return width }
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    delegate: TreeViewDelegate {
                        id: treeDelegate
                        objectName: "treeRow-" + stableId
                        required property string name
                        required property string stableId
                        required property int definitionQuantity
                        // 0 shown, 1 partly hidden, 2 hidden (#14). Reads the revision so it
                        // updates whenever anything is hidden or shown.
                        readonly property int visibilityState: root.controller && root.controller.visibilityRevision >= 0
                            ? root.controller.nodeVisibilityState(stableId) : 0
                        // The same for transparency: 0 opaque, 1 partly, 2 transparent.
                        readonly property int transparencyState: root.controller && root.controller.visibilityRevision >= 0
                            ? root.controller.nodeTransparencyState(stableId) : 0
                        highlighted: root.controller && root.controller.selectedNodeIds.indexOf(stableId) >= 0
                        implicitHeight: root.theme.rowCompact
                        // Keep the label centered against the complete selection
                        // row. TreeViewDelegate otherwise offsets its content box
                        // for indentation, which makes selected labels look shifted.
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0
                        palette.buttonText: root.theme.onSurface
                        palette.text: root.theme.onSurface
                        palette.windowText: root.theme.onSurface
                        palette.highlightedText: root.theme.onSurface
                        palette.base: root.theme.surfaceRaised
                        palette.alternateBase: root.theme.surfaceSubtle
                        palette.highlight: "transparent"
                        // Qt's default indicator is a 40 px ColorImage sized for a
                        // standard Button row that clips/misaligns in our compact
                        // 28 px rows, and its click-to-toggle wiring is a C++-side
                        // TapHandler on the indicator item that competes with the
                        // row's own TapHandler below (needed for selection and the
                        // context menu) for the same tap — unreliably, in practice.
                        // Disabling it and rendering an explicit ThemedToolButton in
                        // contentItem instead (below) — the same real, interactive
                        // button already used for this in Export's component list —
                        // sidesteps that ambiguity entirely: a proper AbstractButton
                        // reliably claims its own press events ahead of a sibling
                        // TapHandler, exactly as it already does in Export.
                        indicator: null
                        background: Rectangle {
                            radius: root.theme.radius1
                            color: treeDelegate.highlighted ? root.theme.accentTint
                                   : treeHover.hovered ? root.theme.accentTint
                                                       : "transparent"
                            opacity: treeDelegate.highlighted ? 1 : (treeHover.hovered ? 0.6 : 1)

                            Rectangle {
                                visible: treeDelegate.highlighted
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 2
                                color: root.theme.accent
                            }

                            Behavior on color {
                                ColorAnimation { duration: root.theme.durInstant }
                            }
                        }
                        contentItem: RowLayout {
                            implicitHeight: root.theme.rowCompact
                            spacing: root.theme.spacing1

                            // 12 px a level for the first four, 4 px beyond: a deep branch keeps
                            // its names readable instead of being indented off the panel.
                            Item {
                                Layout.preferredWidth: root.theme.spacing2 + Math.min(treeDelegate.depth, 4) * 12
                                                       + Math.max(0, treeDelegate.depth - 4) * 4
                                Layout.preferredHeight: 1
                            }

                            IconButton {
                                id: disclosureButton
                                theme: root.theme
                                // A 20 px square with radius.1: 4 px clear of the 28 px row's
                                // edges, concentric with its highlight.
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                Layout.alignment: Qt.AlignVCenter
                                cornerRadius: root.theme.radius1
                                visible: treeDelegate.hasChildren
                                glyph: treeDelegate.expanded ? "chevronDown" : "chevronRight"
                                Accessible.name: treeDelegate.expanded
                                                 ? qsTr("Collapse %1").arg(treeDelegate.name)
                                                 : qsTr("Expand %1").arg(treeDelegate.name)
                                onClicked: assemblyTree.toggleExpanded(treeDelegate.row)
                            }
                            Item {
                                visible: !disclosureButton.visible
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 1
                            }

                            Label {
                                id: treeLabel
                                Layout.fillWidth: true
                                rightPadding: root.theme.spacing1
                                // A hidden part reads as absent; a partly hidden assembly as dimmed.
                                opacity: treeDelegate.visibilityState === 2 ? 0.42 : 1
                                font.italic: treeDelegate.visibilityState === 2
                                text: definitionQuantity > 0 ? qsTr("%1  %2x").arg(name).arg(definitionQuantity) : name
                                color: root.foreground
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignLeft
                                verticalAlignment: Text.AlignVCenter
                                // The whole name when the panel cuts it short.
                                ThemedToolTip {
                                    theme: root.theme
                                    text: treeLabel.text
                                    visible: treeHover.hovered && treeLabel.truncated
                                }
                            }

                            // Like the eye beside it: always shown for a transparent or partly
                            // transparent row, on hover otherwise. Not for a hidden one, which
                            // draws nothing to see through.
                            IconButton {
                                id: transparencyToggle
                                objectName: "treeTransparencyToggle"
                                theme: root.theme
                                visible: treeDelegate.visibilityState !== 2
                                // 22 px with radius.1: 3 px clear of the row's edges.
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                Layout.alignment: Qt.AlignVCenter
                                cornerRadius: root.theme.radius1
                                glyph: "transparent"
                                iconColor: treeDelegate.transparencyState === 2 ? root.theme.accent : root.foreground
                                opacity: treeDelegate.transparencyState !== 0 || treeHover.hovered || hovered ? 1 : 0
                                enabled: opacity > 0
                                Accessible.name: treeDelegate.transparencyState === 2
                                                 ? qsTr("Make %1 opaque").arg(treeDelegate.name)
                                                 : qsTr("Make %1 transparent").arg(treeDelegate.name)
                                onClicked: root.controller.setNodeTransparent(treeDelegate.stableId, treeDelegate.transparencyState !== 2)
                                ThemedToolTip {
                                    theme: root.theme
                                    text: treeDelegate.transparencyState === 2 ? qsTr("Transparent — click to make opaque")
                                          : treeDelegate.transparencyState === 1 ? qsTr("Partly transparent — click to make all transparent")
                                                                                 : qsTr("Make transparent")
                                    visible: transparencyToggle.hovered
                                }
                            }

                            // Always shown for a hidden or partly hidden row; on hover otherwise.
                            IconButton {
                                id: visibilityToggle
                                theme: root.theme
                                Layout.preferredWidth: 22
                                Layout.preferredHeight: 22
                                Layout.alignment: Qt.AlignVCenter
                                Layout.rightMargin: root.theme.spacing1
                                cornerRadius: root.theme.radius1
                                glyph: treeDelegate.visibilityState === 0 ? "eye" : "hide"
                                opacity: treeDelegate.visibilityState !== 0 || treeHover.hovered || hovered ? 1 : 0
                                enabled: opacity > 0
                                Accessible.name: treeDelegate.visibilityState === 2
                                                 ? qsTr("Show %1").arg(treeDelegate.name) : qsTr("Hide %1").arg(treeDelegate.name)
                                onClicked: root.controller.setNodeHidden(treeDelegate.stableId, treeDelegate.visibilityState !== 2)
                                ThemedToolTip {
                                    theme: root.theme
                                    text: treeDelegate.visibilityState === 2 ? qsTr("Hidden — click to show")
                                          : treeDelegate.visibilityState === 1 ? qsTr("Partly hidden — click to hide all")
                                                                               : qsTr("Hide")
                                    visible: visibilityToggle.hovered
                                }
                            }
                        }
                        HoverHandler {
                            id: treeHover
                            onHoveredChanged: {
                                if (hovered) root.treeHoveredNodeId = stableId
                                else if (root.treeHoveredNodeId === stableId) root.treeHoveredNodeId = ""
                            }
                        }
                        onStableIdChanged: if (treeHover.hovered) root.treeHoveredNodeId = stableId
                        TapHandler {
                            id: rowTap
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onTapped: function(eventPoint, button) {
                                if (button === Qt.RightButton) {
                                    if (root.controller.selectedNodeIds.indexOf(stableId) < 0)
                                        root.controller.setActiveNodeId(stableId)
                                    treeContextMenu.targetNodeId = stableId
                                    // At the cursor: the position here is the row's own, and the
                                    // menu belongs to another item, so it opened offset upward.
                                    treeContextMenu.popup()
                                    return
                                }
                                // The modifiers are the handler point's: the event point passed
                                // in has none, which left every click a plain one.
                                root.treeClicked(treeDelegate.row, stableId, rowTap.point.modifiers)
                            }
                        }
                    }
                }

                Component.onCompleted: Qt.callLater(function() { if (assemblyTree.rows > 0) assemblyTree.expandRecursively() })
                Connections {
                    target: root.controller ? root.controller.assemblyTree : null
                    function onModelReset() { Qt.callLater(function() { assemblyTree.expandRecursively() }) }
                    function onLayoutChanged() { Qt.callLater(function() { assemblyTree.expandRecursively() }) }
                }
            }

        }

        ElevatedPanel {
            id: viewport
            theme: root.theme
            wellSurface: true
            Layout.fillWidth: true
            Layout.fillHeight: true
            Loader {
                id: viewportLoader
                anchors.fill: parent
                // Inset by the panel's own corner radius so the viewport's
                // square corners don't paint over the rounded card boundary.
                anchors.margins: root.theme.radius3
                active: root.controller && (root.controller.documentState === AppState.Opening
                                            || root.controller.documentState === AppState.TreeReady)
                source: "StepViewport.qml"
                onLoaded: {
                    item.controller = root.controller
                    item.theme = root.theme
                    item.measurementActive = Qt.binding(function() {
                        return root.activeTask === "measure" && root.taskPanelVisible
                    })
                    item.explodedLayoutEnabled = true
                    item.explodeToolActive = Qt.binding(function() {
                        return root.activeTask === "explode" && root.taskPanelVisible
                    })
                    item.externalHighlightNodeId = Qt.binding(function() {
                        return root.treeHoveredNodeId
                    })
                    item.toolRequested.connect(function(tool) {
                        if (tool === "close") {
                            root.taskPanelVisible = false
                        } else if (tool !== "properties") {
                            root.showTask(tool)
                        }
                    })
                    item.openFileRequested.connect(function() { root.openFileRequested() })
                    item.savePictureRequested.connect(function() { captureDialog.open() })
                    // A document that arrives already loaded -- a tab dragged into a new
                    // window (#15) -- has no geometry streaming in, so ask for a replay.
                    if (root.controller.documentState === AppState.TreeReady) {
                        Qt.callLater(function() { if (root.controller) root.controller.replayGeometry() })
                        item.requestFit()
                    }
                }
            }
            Column {
                anchors.centerIn: parent
                visible: !root.controller || root.controller.documentState !== AppState.TreeReady
                         || (root.controller.importInProgress && (!viewportLoader.item || !viewportLoader.item.hasSceneBounds))
                width: Math.min(viewport.width - 48, 520)
                spacing: 8

                Label {
                    width: parent.width
                    text: root.controller && root.controller.importInProgress ? root.controller.importStage
                        : root.controller && root.controller.documentState === AppState.WorkerFailed ? qsTr("Could not open assembly")
                        : root.controller && root.controller.documentState === AppState.Invalid ? qsTr("File could not be opened")
                        : qsTr("Open a STEP assembly to inspect it")
                    color: root.controller && (root.controller.documentState === AppState.WorkerFailed
                                                || root.controller.documentState === AppState.Invalid) ? root.theme.error : root.foreground
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                Label {
                    width: parent.width
                    visible: root.controller && root.controller.errorMessage.length > 0
                    text: root.controller ? root.controller.errorMessage : ""
                    color: root.subduedForeground
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                }

                ThemedProgressBar {
                    theme: root.theme
                    width: parent.width
                    visible: root.controller && root.controller.importInProgress
                    from: 0
                    to: 1
                    value: root.controller ? root.controller.importProgress : 0
                }

                Label {
                    width: parent.width
                    visible: root.controller && root.controller.importInProgress
                    text: root.controller ? qsTr("%1%  •  %2 s  •  %3").arg(Math.round(root.controller.importProgress * 100)).arg(root.controller.importElapsedSeconds)
                        .arg(root.remainingText(root.controller.importEtaSeconds)) : ""
                    color: root.subduedForeground
                    horizontalAlignment: Text.AlignHCenter
                }

                ThemedButton {
                    theme: root.theme
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: root.controller && root.controller.importInProgress
                    text: root.controller && root.controller.previewReady
                        ? qsTr("Cancel refinement") : qsTr("Cancel import")
                    onClicked: root.controller.cancelImport()
                }
            }
            Rectangle {
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 16
                width: Math.min(parent.width - 32, 480)
                implicitHeight: refinementProgress.implicitHeight + 18
                radius: root.theme.radius2
                color: root.theme.surfaceRaised
                border.color: root.theme.border
                visible: root.controller && root.controller.documentState === AppState.TreeReady
                         && root.controller.importInProgress && viewportLoader.item && viewportLoader.item.hasSceneBounds
                z: 2

                Column {
                    id: refinementProgress
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: root.theme.spacing2
                    Label {
                        width: parent.width
                        text: root.controller ? root.controller.importStage : ""
                        color: root.foreground
                        font.bold: true
                    }
                    ThemedProgressBar {
                        theme: root.theme
                        width: parent.width
                        from: 0
                        to: 1
                        value: root.controller ? root.controller.importProgress : 0
                    }
                    RowLayout {
                        width: parent.width
                        Label {
                            Layout.fillWidth: true
                            text: root.controller ? qsTr("%1%  •  %2 s  •  %3")
                                .arg(Math.round(root.controller.importProgress * 100)).arg(root.controller.importElapsedSeconds)
                                .arg(root.remainingText(root.controller.importEtaSeconds)) : ""
                            color: root.subduedForeground
                        }
                        ThemedButton {
                            theme: root.theme
                            text: qsTr("Cancel refinement")
                            onClicked: root.controller.cancelImport()
                        }
                    }
                }
            }
            InspectionDock {
                objectName: "inspectionDock"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                availableWidth: parent.width - 2 * root.theme.spacing3
                openTool: root.taskPanelVisible ? root.activeTask : ""
                viewerPresentation: root.controller ? root.controller.viewerPresentation : AppState.Full
                exploded: root.exploded
                measureBlocked: root.explodedShowing
                selectionTransparent: root.selectionTransparent
                theme: root.theme
                onToolTriggered: function(tool) {
                    if (tool === "fit") root.controller.fitView()
                    else if (tool === "isolate") {
                        root.controller.toggleIsolateActiveNode()
                    } else if (tool === "hide") root.controller.hideActiveNode()
                    else if (tool === "transparent") root.controller.toggleTransparentSelection()
                    else if (tool === "section" || tool === "measure" || tool === "capture" || tool === "explode" || tool === "views") root.showTask(tool)
                }
            }
        }

        ContextPanel {
            id: contextPanel
            Layout.preferredWidth: contextSplitter.effectiveWidth
            Layout.fillWidth: false
            Layout.fillHeight: true
            controller: root.controller
            theme: root.theme
        }
    }

    PanelSplitter {
        id: treeSplitter
        objectName: "inspectTreeSplitter"
        theme: root.theme
        preferences: root.preferences
        key: "inspectTree"
        panel: treePanel
        gap: workspaceRow.spacing
        defaultWidth: 248
        minimumWidth: 180
        maximumWidth: Math.min(640, root.sidePanelRoom - contextSplitter.staticWidth)
    }
    PanelSplitter {
        id: contextSplitter
        objectName: "inspectPropertiesSplitter"
        theme: root.theme
        preferences: root.preferences
        key: "inspectProperties"
        panel: contextPanel
        panelOnLeft: false
        gap: workspaceRow.spacing
        defaultWidth: 300
        minimumWidth: 260
        maximumWidth: Math.min(560, root.sidePanelRoom - treeSplitter.effectiveWidth)
    }

    ThemedMenu {
        id: treeContextMenu
        theme: root.theme
        property string targetNodeId: ""
        ThemedMenuItem {
            theme: treeContextMenu.theme
            text: qsTr("Fit selection")
            enabled: treeContextMenu.targetNodeId.length > 0
            onTriggered: if (viewportLoader.item) viewportLoader.item.fitSelection()
        }
        ThemedMenuSeparator { theme: treeContextMenu.theme }
        ThemedMenuItem {
            theme: treeContextMenu.theme
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Exit isolate") : qsTr("Isolate")
            enabled: treeContextMenu.targetNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Hide"); enabled: treeContextMenu.targetNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: root.selectionTransparent ? qsTr("Make opaque") : qsTr("Make transparent"); enabled: treeContextMenu.targetNodeId.length > 0; onTriggered: root.controller.toggleTransparentSelection() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Make others transparent"); enabled: treeContextMenu.targetNodeId.length > 0; onTriggered: root.controller.makeOthersTransparent() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Show all"); onTriggered: root.controller.showAllNodes() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Undo visibility change"); enabled: root.controller && root.controller.canRestoreVisibility; onTriggered: root.controller.restoreVisibility() }
        ThemedMenuSeparator { theme: treeContextMenu.theme; visible: root.exploded }
        ThemedMenuItem {
            theme: treeContextMenu.theme
            visible: root.exploded
            text: qsTr("Reset position")
            enabled: {
                const revision = root.controller ? root.controller.explode.revision : -1
                return revision >= 0 && treeContextMenu.targetNodeId.length > 0
                        && root.controller.explode.canReset(treeContextMenu.targetNodeId)
            }
            onTriggered: root.controller.explode.resetNode(treeContextMenu.targetNodeId)
        }
        ThemedMenuItem {
            theme: treeContextMenu.theme
            visible: root.exploded
            text: qsTr("Reset exploded view")
            onTriggered: if (viewportLoader.item) viewportLoader.item.animateExplodeReset()
        }
    }

    // The open tool's panel, in a window the user can drag out of the way or minimise.
    FloatingToolWindow {
        id: taskWindow
        objectName: "taskWindow"
        theme: root.theme
        shown: root.activeTask !== "" && root.taskPanelVisible
        // Clear of the properties panel and above the toolbar, where the panel always opened.
        defaultRightMargin: contextPanel.width + 2 * root.theme.spacing3 + 2
        title: root.activeTask === "measure" ? qsTr("Measure")
             : root.activeTask === "section" ? qsTr("Section")
             : root.activeTask === "explode" ? qsTr("Exploded view")
             : root.activeTask === "views" ? qsTr("Views")
             : qsTr("Capture PNG")
        onCloseRequested: root.taskPanelVisible = false

        Loader {
            id: taskPanelLoader
            active: root.activeTask !== "" && root.taskPanelVisible
            source: root.activeTask === "measure" ? "MeasurementPanel.qml"
                  : root.activeTask === "section" ? "SectionPanel.qml"
                  : root.activeTask === "explode" ? "ExplodePanel.qml"
                  : root.activeTask === "views" ? "ViewsPanel.qml"
                  : "CapturePanel.qml"
            onLoaded: {
                item.theme = root.theme
                item.embedded = true
                if (root.activeTask === "measure") item.taskController = root.controller.measurement
                else if (root.activeTask === "section") item.taskController = root.controller.section
                else if (root.activeTask === "explode") {
                    item.taskController = root.controller.explode
                    item.documentController = root.controller
                    item.viewport = Qt.binding(function() { return viewportLoader.item })
                    item.autoExplodeRequested.connect(function(scopeId, factor) {
                        if (viewportLoader.item) viewportLoader.item.animateAutoExplode(scopeId, factor)
                    })
                    item.resetAllRequested.connect(function() {
                        if (viewportLoader.item) viewportLoader.item.animateExplodeReset()
                    })
                    item.exportRequested.connect(root.chooseExplodedExport)
                } else if (root.activeTask === "views") {
                    item.documentController = root.controller
                    item.viewport = Qt.binding(function() { return viewportLoader.item })
                } else {
                    item.taskController = root.controller.capture
                    item.captureRequested.connect(function() { captureDialog.open() })
                    item.captureToClipboardRequested.connect(function() {
                        if (viewportLoader.item) viewportLoader.item.captureToClipboard()
                    })
                }
                item.closeRequested.connect(function() {
                    root.taskPanelVisible = false
                })
                if (root.activeTask === "section") {
                    item.disableRequested.connect(function() {
                        root.controller.section.enabled = false
                        root.activeTask = ""
                        root.taskPanelVisible = false
                    })
                    item.normalViewRequested.connect(function() {
                        if (viewportLoader.item) viewportLoader.item.alignToSection()
                    })
                    item.oppositeViewRequested.connect(function() {
                        if (viewportLoader.item) viewportLoader.item.alignToSection(true)
                    })
                    item.borderColorRequested.connect(function(currentColor) {
                        sectionBorderColorDialog.selectedColor = currentColor
                        sectionBorderColorDialog.open()
                    })
                }
            }
        }
    }

    ThemedColorDialog {
        id: sectionBorderColorDialog
        theme: root.theme
        title: qsTr("Section border color")
        selectedColor: root.theme.edge
        onAccepted: if (root.controller)
            root.controller.section.sliceBorderColor = pickedColor.toString()
    }

    FileDialog {
        id: explodedExportDialog
        objectName: "explodedExportDialog"
        title: qsTr("Export exploded assembly")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("STEP assembly (*.step *.stp)"), qsTr("STL mesh (*.stl)"), qsTr("DXF drawing of this view (*.dxf)"),
                      qsTr("SVG drawing of this view (*.svg)"), qsTr("PDF drawing of this view (*.pdf)")]
        defaultSuffix: ["step", "stl", "dxf", "svg", "pdf"][Math.max(0, selectedNameFilter.index)]
        onAccepted: {
            const viewport = viewportLoader.item
            root.controller.exportExplodedAssembly(selectedFile,
                    viewport ? viewport.viewDirection : Qt.vector3d(0, 0, 1),
                    viewport ? viewport.viewUpDirection : Qt.vector3d(0, 1, 0))
        }
    }

    FileDialog {
        id: captureDialog
        title: qsTr("Save inspection capture")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("PNG image (*.png)")]
        defaultSuffix: "png"
        onAccepted: {
            if (viewportLoader.item) viewportLoader.item.captureToFile(selectedFile)
        }
    }
}
