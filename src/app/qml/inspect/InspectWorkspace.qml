import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import Loupe.App

Item {
    id: root
    property QtObject controller
    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
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
        spacing: 10

        Rectangle {
            id: treePanel
            color: root.theme.surfaceRaised
            radius: 8
            Layout.preferredWidth: 248
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8
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
                        required property string name
                        required property string stableId
                        required property int definitionQuantity
                        highlighted: root.controller && root.controller.activeNodeId === stableId
                        palette.buttonText: root.theme.onSurface
                        palette.text: root.theme.onSurface
                        palette.windowText: root.theme.onSurface
                        palette.highlightedText: root.theme.onSurface
                        palette.base: root.theme.surfaceRaised
                        palette.alternateBase: root.theme.surfaceSubtle
                        palette.highlight: root.theme.selection
                        contentItem: Label {
                            text: definitionQuantity > 0 ? qsTr("%1  %2x").arg(name).arg(definitionQuantity) : name
                            color: root.foreground
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: root.controller.setActiveNodeId(stableId)
                        HoverHandler {
                            id: treeHover
                            onHoveredChanged: {
                                if (hovered) root.treeHoveredNodeId = stableId
                                else if (root.treeHoveredNodeId === stableId) root.treeHoveredNodeId = ""
                            }
                        }
                        onStableIdChanged: if (treeHover.hovered) root.treeHoveredNodeId = stableId
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: function(eventPoint) {
                                root.controller.setActiveNodeId(stableId)
                                treeContextMenu.targetNodeId = stableId
                                treeContextMenu.popup(eventPoint.position.x, eventPoint.position.y)
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

        Rectangle {
            id: viewport
            color: root.theme.viewport
            radius: 8
            Layout.fillWidth: true
            Layout.fillHeight: true
            Loader {
                id: viewportLoader
                anchors.fill: parent
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

                ProgressBar {
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

                Button {
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
                radius: 6
                color: root.theme.surfaceRaised
                border.color: root.theme.border
                visible: root.controller && root.controller.documentState === AppState.TreeReady
                         && root.controller.importInProgress && viewportLoader.item && viewportLoader.item.hasSceneBounds
                z: 2

                Column {
                    id: refinementProgress
                    anchors.fill: parent
                    anchors.margins: 9
                    spacing: 6
                    Label {
                        width: parent.width
                        text: root.controller ? root.controller.importStage : ""
                        color: root.foreground
                        font.bold: true
                    }
                    ProgressBar {
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
                        Button {
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
            }
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
