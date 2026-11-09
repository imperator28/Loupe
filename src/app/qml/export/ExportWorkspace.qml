import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

Item {
    id: root

    property QtObject controller
    property QtObject theme
    property QtObject preferences: null
    // The side panels can be dragged wider or narrower; the 3D view keeps at least this.
    readonly property real centreMinimumWidth: 420
    readonly property real sidePanelRoom: width - 4 * (theme ? theme.spacing3 : 12) - centreMinimumWidth
    property bool previewsActivated: false
    property bool previewReplayIssued: false
    readonly property QtObject draft: controller ? controller.exportWorkspace : null
    readonly property string previewName: {
        if (!draft || draft.previewNodeId.length === 0) return ""
        const items = draft.components
        for (const item of items) if (item.nodeId === draft.previewNodeId) return item.name
        return ""
    }

    function synchronizeSceneSelection() {
        if (!root.visible || !root.controller || !root.draft) return
        const pickerNodeId = root.draft.focusSceneNode(root.controller.activeNodeId)
        if (pickerNodeId.length > 0 && pickerNodeId !== root.controller.activeNodeId) {
            root.controller.activeNodeId = pickerNodeId
            return
        }
        componentPicker.revealNode(pickerNodeId)
    }

    // One shortcut per mode for the whole workspace, not one per viewport. Two viewports each
    // declaring "1" would make the sequence ambiguous, and Qt would fire neither.
    function applyRenderMode(mode) {
        masterPreview.setRenderMode(mode)
        standalonePreview.setRenderMode(mode)
    }

    // Cmd/Ctrl+A: every part in the part list, as its Select all does; a text field keeps it.
    Shortcut {
        objectName: "exportSelectAllShortcut"
        sequences: [StandardKey.SelectAll]
        enabled: root.visible && root.draft !== null && !root.textFieldFocused()
        onActivated: root.draft.setAllChecked(true)
    }
    function textFieldFocused() {
        const item = root.Window.window ? root.Window.window.activeFocusItem : null
        return !!(item && typeof item.selectAll === "function" && "text" in item)
    }
    Shortcut { sequences: ["1"]; enabled: root.visible; onActivated: root.applyRenderMode(0) }
    Shortcut { sequences: ["2"]; enabled: root.visible; onActivated: root.applyRenderMode(1) }
    Shortcut { sequences: ["3"]; enabled: root.visible; onActivated: root.applyRenderMode(2) }

    function activatePreviews() {
        if (!previewsActivated) previewsActivated = true
        Qt.callLater(root.replayGeometryWhenReady)
    }

    function replayGeometryWhenReady() {
        if (previewReplayIssued || !controller || !masterPreview.viewportReady
                || !standalonePreview.viewportReady) return
        previewReplayIssued = true
        controller.replayGeometry()
    }

    // Arriving from Inspect, the previews show whatever camera they were left with,
    // which after a selection change is rarely framed on what the user is now looking
    // at. Ask for a fit on every entry; StepViewport holds the request until the
    // geometry it needs exists, so this works whether or not the replay has landed.
    function refitPreviews() {
        masterPreview.requestFit()
        standalonePreview.requestFit()
    }

    onVisibleChanged: if (visible) {
        root.activatePreviews()
        Qt.callLater(root.synchronizeSceneSelection)
        Qt.callLater(root.refitPreviews)
    }
    Component.onCompleted: if (visible) root.activatePreviews()

    Connections {
        target: root.controller
        function onActiveNodeIdChanged() { Qt.callLater(root.synchronizeSceneSelection) }
    }

    RowLayout {
        id: workspaceRow
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing3

        ExportComponentPicker {
            id: componentPicker
            Layout.preferredWidth: pickerSplitter.effectiveWidth
            Layout.fillWidth: false
            Layout.fillHeight: true
            controller: root.controller
            draft: root.draft
            theme: root.theme
        }

        ExportPreview {
            id: masterPreview
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            controller: root.controller
            theme: root.theme
            title: root.previewName.length > 0
                   ? qsTr("Master assembly · %1 highlighted").arg(root.previewName)
                   : qsTr("Master assembly")
            displayOnlyNodeId: ""
            highlightNodeId: root.draft ? root.draft.previewNodeId : ""
            selectionEnabled: true
            loadEnabled: root.previewsActivated
            onViewportReadyChanged: root.replayGeometryWhenReady()
        }

        // The width is pinned on this column rather than on the ScrollView, which
        // is what the Drawing workspace does. A ScrollView is a Control, so its
        // implicit width comes from its content and its padding includes the
        // scroll bar, and both of those differ per platform -- pinning it there
        // let macOS resolve this column wider than the 360 the two workspaces are
        // meant to share, so Export and Drawing did not line up. A ColumnLayout
        // takes the width it is given.
        ColumnLayout {
            id: settingsColumn
            Layout.preferredWidth: settingsSplitter.effectiveWidth
            // A layout whose children fill their width fills too, unless told otherwise:
            // this column then shared the window's spare width with the 3D view and stood
            // at twice the width its divider set.
            Layout.fillWidth: false
            Layout.fillHeight: true
            spacing: root.theme.spacing3

        // Scrollable, because at a small window height the destination, format, naming and
        // export button fall off the bottom with no way to reach them.
        ScrollView {
            id: settingsScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: parent.width
            // At least the viewport's height, so Layout.fillHeight below has something to
            // fill. Without it this column is only as tall as its content, fillHeight
            // resolves to the bucket's minimum, and the panel stops well short of the
            // bottom with dead space under it -- while still growing and scrolling when
            // the content genuinely exceeds the viewport.
            height: Math.max(implicitHeight, settingsScroll.availableHeight)
            spacing: root.theme.spacing3

            ExportPreview {
                id: standalonePreview
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(220, root.height * 0.34)
                controller: root.controller
                theme: root.theme
                title: root.previewName.length > 0
                       ? qsTr("Standalone preview · %1").arg(root.previewName)
                       : qsTr("Standalone preview")
                emptyText: qsTr("Hover a component to preview its standalone file")
                requireSelection: true
                displayOnlyNodeId: root.draft ? root.draft.previewNodeId : ""
                // The part clicked last (in the bucket or the tree) stays lit while it is shown.
                highlightNodeId: root.draft && root.draft.previewNodeId === root.draft.focusedNodeId ? root.draft.focusedNodeId : ""
                loadEnabled: root.previewsActivated
                onViewportReadyChanged: root.replayGeometryWhenReady()
            }

            ExportBucket {
                objectName: "exportBucket"
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
                draft: root.draft
                theme: root.theme
                onRowSelected: function(nodeId) {
                    if (root.controller) root.controller.activeNodeId = nodeId
                    componentPicker.revealNode(nodeId)
                }
            }

            ExportSettings {
                Layout.fillWidth: true
                draft: root.draft
                theme: root.theme
            }
        }
        }
        }
    }

    Inspect.PanelSplitter {
        objectName: "pickerSplitter"
        id: pickerSplitter
        theme: root.theme
        preferences: root.preferences
        // Shared with the other output workspace, so Export and Drawing keep lining up.
        key: "outputPicker"
        panel: componentPicker
        gap: workspaceRow.spacing
        defaultWidth: 290
        minimumWidth: 180
        maximumWidth: Math.min(560, root.sidePanelRoom - settingsSplitter.staticWidth)
    }
    Inspect.PanelSplitter {
        objectName: "settingsSplitter"
        id: settingsSplitter
        theme: root.theme
        preferences: root.preferences
        key: "outputSettings"
        panel: settingsColumn
        panelOnLeft: false
        gap: workspaceRow.spacing
        defaultWidth: 360
        minimumWidth: 260
        maximumWidth: Math.min(640, root.sidePanelRoom - pickerSplitter.effectiveWidth)
    }
}
