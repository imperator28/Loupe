import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property QtObject controller
    property QtObject theme
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

    onVisibleChanged: if (visible) {
        root.activatePreviews()
        Qt.callLater(root.synchronizeSceneSelection)
    }
    Component.onCompleted: if (visible) root.activatePreviews()

    Connections {
        target: root.controller
        function onActiveNodeIdChanged() { Qt.callLater(root.synchronizeSceneSelection) }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        ExportComponentPicker {
            id: componentPicker
            Layout.preferredWidth: 290
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

        ColumnLayout {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            spacing: 10

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
                highlightNodeId: ""
                loadEnabled: root.previewsActivated
                onViewportReadyChanged: root.replayGeometryWhenReady()
            }

            ExportBucket {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 220
                draft: root.draft
                theme: root.theme
            }

            ExportSettings {
                Layout.fillWidth: true
                draft: root.draft
                theme: root.theme
            }
        }
    }
}
