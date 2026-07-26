import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

// The 2D drawing workspace.
//
// Layout order is the workflow order: choose a part, choose a view, read the 2D preview,
// then queue it. The add action sits below the preview on purpose -- the requirement is
// that nothing is queued sight-unseen.
Item {
    id: root

    property QtObject controller
    property QtObject theme
    property bool previewsActivated: false
    property bool previewReplayIssued: false
    readonly property QtObject draft: controller ? controller.drawingWorkspace : null

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
        if (previewReplayIssued || !controller || !modelPreview.viewportReady) return
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
        function onDrawingPreviewReady(previewJson, revision, approximate) {
            preview2d.applyPreview(previewJson, revision, approximate)
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing3

        DrawingComponentPicker {
            id: componentPicker
            Layout.preferredWidth: 260
            Layout.fillHeight: true
            controller: root.controller
            draft: root.draft
            theme: root.theme
        }

        // 3D on the left, 2D on the right: the view is chosen in one and verified in the
        // other, so they have to be side by side rather than in tabs.
        Inspect.ElevatedPanel {
            id: modelPreviewPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 320
            theme: root.theme
            wellSurface: true

            readonly property bool viewportReady: viewportLoader.status === Loader.Ready

            Loader {
                id: viewportLoader
                anchors.fill: parent
                anchors.margins: root.theme && root.theme.radius3 !== undefined ? root.theme.radius3 : 8
                active: root.previewsActivated && root.controller
                asynchronous: true
                source: "../inspect/StepViewport.qml"
                onLoaded: {
                    item.controller = root.controller
                    item.theme = Qt.binding(function() { return root.theme })
                    item.presentationOnly = true
                    item.renderModeControlVisible = true
                    item.viewCubeVisible = true
                    // A click picks a face to project normal to, not a component to select.
                    item.selectionEnabled = false
                    item.faceFrameSelectionEnabled = true
                    item.componentHoverEnabled = false
                    item.contextActionsEnabled = false
                    item.requireDisplayFilter = true
                    item.displayOnlyNodeId = Qt.binding(function() {
                        return root.draft ? root.draft.candidateNodeId : ""
                    })
                    item.faceFrameSelected.connect(root.applyFaceFrame)
                }
            }

            Label {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: root.theme ? root.theme.spacing3 : 12
                text: root.draft && root.draft.candidateNodeId.length > 0
                      ? qsTr("Click a flat face, or a cube face")
                      : qsTr("Choose a part to draw")
                color: root.theme ? root.theme.foreground : "transparent"
                font.bold: true
                z: 3
            }
        }

        ColumnLayout {
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            spacing: root.theme.spacing3

            DrawingPreview2D {
                id: preview2d
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(240, root.height * 0.38)
                draft: root.draft
                theme: root.theme
            }

            DrawingSetupPanel {
                Layout.fillWidth: true
                draft: root.draft
                theme: root.theme
                viewResolver: root.controller
            }

            DrawingQueue {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 180
                draft: root.draft
                theme: root.theme
            }

            DrawingOutputPanel {
                Layout.fillWidth: true
                draft: root.draft
                theme: root.theme
            }
        }
    }

    function applyFaceFrame(frame) {
        if (!root.draft || !frame) return
        // The controller decides whether the face is flat enough; passing the deviation
        // rather than a verdict keeps that judgement in one place.
        root.draft.setCandidateFaceNormal(frame.normal.x, frame.normal.y, frame.normal.z,
                                          frame.maximumDeviationDegrees)
    }
}
