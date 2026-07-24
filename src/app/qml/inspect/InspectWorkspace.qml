import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Loupe.App

Item {
    id: root
    property QtObject controller
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    property string activeTask: ""
    property bool taskPanelVisible: false
    property string treeHoveredNodeId: ""
    signal openFileRequested()

    function showTask(tool) {
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

    Connections {
        target: root.controller
        function onSnapshotChanged() {
            root.activeTask = ""
            root.taskPanelVisible = false
            root.treeHoveredNodeId = ""
        }
        function onActiveNodeIdChanged() { Qt.callLater(root.revealActiveNode) }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing3

        ElevatedPanel {
            id: treePanel
            theme: root.theme
            Layout.preferredWidth: 248
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: root.theme.spacing3
                spacing: root.theme.spacing2
                Label {
                    text: qsTr("Assembly tree")
                    color: root.foreground
                    font.bold: true
                    Layout.fillWidth: true
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
                        required property string name
                        required property string stableId
                        required property int definitionQuantity
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

                            Item {
                                Layout.preferredWidth: root.theme.spacing6 + treeDelegate.depth * 16
                                Layout.preferredHeight: 1
                            }

                            ThemedToolButton {
                                id: disclosureButton
                                theme: root.theme
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 24
                                visible: treeDelegate.hasChildren
                                text: treeDelegate.expanded ? "▾" : "▸"
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
                                rightPadding: root.theme.spacing6
                                text: definitionQuantity > 0 ? qsTr("%1  %2x").arg(name).arg(definitionQuantity) : name
                                color: root.foreground
                                elide: Text.ElideRight
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
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
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onTapped: function(eventPoint, button) {
                                if (button === Qt.RightButton) {
                                    if (root.controller.selectedNodeIds.indexOf(stableId) < 0)
                                        root.controller.setActiveNodeId(stableId)
                                    treeContextMenu.targetNodeId = stableId
                                    treeContextMenu.popup(eventPoint.position.x, eventPoint.position.y)
                                    return
                                }
                                const modifiers = eventPoint.modifiers
                                root.controller.selectNode(stableId, (modifiers & Qt.ShiftModifier)
                                                           || (modifiers & Qt.ControlModifier)
                                                           || (modifiers & Qt.MetaModifier))
                            }
                        }
                    }
                }

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
                        .arg(root.controller.importEtaSeconds >= 0 ? qsTr("about %1 s remaining").arg(root.controller.importEtaSeconds) : qsTr("estimating remaining time")) : ""
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
                                .arg(root.controller.importEtaSeconds >= 0 ? qsTr("about %1 s remaining").arg(root.controller.importEtaSeconds)
                                                                            : qsTr("estimating remaining time")) : ""
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
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                viewerPresentation: root.controller ? root.controller.viewerPresentation : AppState.Full
                theme: root.theme
                onToolTriggered: function(tool) {
                    if (tool === "fit") root.controller.fitView()
                    else if (tool === "isolate") {
                        root.controller.toggleIsolateActiveNode()
                    } else if (tool === "hide") root.controller.hideActiveNode()
                    else if (tool === "section" || tool === "measure" || tool === "capture") root.showTask(tool)
                }
            }
        }

        ContextPanel {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            controller: root.controller
            theme: root.theme
        }
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
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Restore") : qsTr("Isolate")
            enabled: treeContextMenu.targetNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Hide"); enabled: treeContextMenu.targetNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Hide others"); enabled: treeContextMenu.targetNodeId.length > 0; onTriggered: root.controller.hideOtherNodes() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Show all"); onTriggered: root.controller.showAllNodes() }
        ThemedMenuItem { theme: treeContextMenu.theme; text: qsTr("Restore visibility"); enabled: root.controller && root.controller.canRestoreVisibility; onTriggered: root.controller.restoreVisibility() }
    }

    Loader {
        id: taskPanelLoader
        anchors.right: parent.right
        anchors.rightMargin: 314
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 96
        active: root.activeTask !== "" && root.taskPanelVisible
        // Subtle rise + fade as the tool panel appears, so it reads as
        // surfacing from the toolbar rather than snapping into place.
        opacity: active ? 1 : 0
        transform: Translate {
            y: taskPanelLoader.active ? 0 : 8
            Behavior on y { NumberAnimation { duration: root.theme.durStandard; easing.type: Easing.BezierSpline; easing.bezierCurve: root.theme.easeEnter } }
        }
        Behavior on opacity { NumberAnimation { duration: root.theme.durStandard; easing.type: Easing.BezierSpline; easing.bezierCurve: root.theme.easeEnter } }
        source: root.activeTask === "measure" ? "MeasurementPanel.qml"
              : root.activeTask === "section" ? "SectionPanel.qml"
              : "CapturePanel.qml"
        onLoaded: {
            item.theme = root.theme
            if (root.activeTask === "measure") item.taskController = root.controller.measurement
            else if (root.activeTask === "section") item.taskController = root.controller.section
            else {
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

    ThemedColorDialog {
        id: sectionBorderColorDialog
        theme: root.theme
        title: qsTr("Section border color")
        selectedColor: root.theme.edge
        onAccepted: if (root.controller)
            root.controller.section.sliceBorderColor = pickedColor.toString()
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
