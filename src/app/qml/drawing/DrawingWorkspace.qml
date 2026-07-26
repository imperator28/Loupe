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
    // drawingId -> the preview as it was when queued, for the queue thumbnails.
    property var queuedPreviews: ({})
    // Hovering a part shows it, so the tree can be browsed without committing a choice.
    property string hoveredNodeId: ""
    readonly property string viewportNodeId: hoveredNodeId.length > 0
        ? hoveredNodeId : (draft ? draft.candidateNodeId : "")

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
        if (previewReplayIssued || !controller || !modelPreviewPanel.viewportReady) return
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
            Layout.preferredWidth: 290
            Layout.fillHeight: true
            controller: root.controller
            draft: root.draft
            theme: root.theme
            onHoveredNodeIdChanged: root.hoveredNodeId = hoveredNodeId
        }

        // 3D on the left, 2D on the right: the view is chosen in one and verified in the
        // other, so they have to be side by side rather than in tabs.
        Inspect.ElevatedPanel {
            id: modelPreviewPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 420
            theme: root.theme
            wellSurface: true

            readonly property bool viewportReady: viewportLoader.status === Loader.Ready
            // The Loader is asynchronous, so it is not ready when activatePreviews() first
            // asks. Without this the replay never happened and the viewport stayed empty
            // until some other workspace replayed the geometry for it.
            onViewportReadyChanged: root.replayGeometryWhenReady()

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
                    item.displayOnlyNodeId = Qt.binding(function() { return root.viewportNodeId })
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

        // The preview stays put and only the configuration scrolls. Everything to do with
        // exporting has moved into a sheet behind the queue button, so the drawing being
        // configured is what is on screen, and the preview never scrolls out from under the
        // controls that change it.
        ColumnLayout {
            // The same split as the Export workspace: 290 for the picker, 360 here, and the
            // 3D view takes what is left with a 420 minimum. Matching it rather than picking
            // new numbers means the two workspaces do not feel like different products.
            Layout.preferredWidth: 360
            Layout.fillHeight: true
            spacing: root.theme.spacing3

            DrawingPreview2D {
                id: preview2d
                Layout.fillWidth: true
                // Sticky: outside the scroll area, sized to its own content.
                Layout.preferredHeight: implicitHeight
                draft: root.draft
                theme: root.theme
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                DrawingSetupPanel {
                    width: parent.width
                    draft: root.draft
                    theme: root.theme
                    viewResolver: root.controller
                }
            }

            // Add and review side by side: they are the two things done at this point, and
            // stacking them made the second look like a consequence of the first.
            RowLayout {
                Layout.fillWidth: true
                spacing: root.theme.spacing2

                Inspect.ThemedButton {
                    id: addButton
                    objectName: "drawingAddToQueue"
                    theme: root.theme
                    primary: true
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: qsTr("Add to queue")
                    enabled: root.draft && root.draft.candidateValid && !root.draft.exporting
                    onClicked: {
                        if (!root.draft) return
                        const drawingId = root.draft.addCandidateToQueue()
                        if (drawingId.length === 0) return
                        // Snapshot the preview alongside the drawing, so the queue can show what
                        // was queued without re-running a hidden-line projection per row.
                        // A fresh object, not the existing one mutated: assigning the same
                        // reference back emits no change signal, so the queue never learned
                        // about it and every thumbnail stayed on its placeholder. The
                        // surrounding code already avoids this by using slice/filter/concat.
                        const next = Object.assign({}, root.queuedPreviews)
                        next[drawingId] = preview2d.preview
                        root.queuedPreviews = next
                        queueButton.acknowledge()
                    }
                    Inspect.ThemedToolTip {
                        theme: root.theme
                        visible: parent.hovered
                        text: root.draft && root.draft.candidateValid
                              ? qsTr("Queue this view. The direction is fixed now, so later orbiting will not change it.")
                              : qsTr("Choose a part and a view first")
                    }
                }

                Inspect.ThemedButton {
                    id: queueButton
                    objectName: "drawingQueueButton"
                    theme: root.theme
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: qsTr("Review queue")
                    // The count is a badge, so it is not in the label the reader hears.
                    Accessible.name: root.draft && root.draft.queueCount > 0
                                     ? qsTr("Review queue, %1 drawing(s)").arg(root.draft.queueCount)
                                     : qsTr("Review queue, empty")
                    enabled: root.draft && root.draft.queueCount > 0
                    onClicked: exportSheet.open()

                    function acknowledge() {
                        // A queued drawing goes somewhere the user cannot see, so the count
                        // bumps and the button pulses. Transform and opacity only, per the
                        // motion rules -- no colour animation and no layout movement.
                        acknowledgeAnimation.restart()
                    }

                    // The count lives inside the button rather than in its label, so it reads
                    // as a quantity rather than as part of a sentence.
                    Rectangle {
                        id: countBadge
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: root.theme.spacing2
                        visible: root.draft && root.draft.queueCount > 0
                        width: Math.max(20, countLabel.implicitWidth + 10)
                        height: 20
                        radius: height / 2
                        color: root.theme.accent

                        Label {
                            id: countLabel
                            anchors.centerIn: parent
                            text: root.draft ? root.draft.queueCount : 0
                            color: root.theme.accentForeground
                            font.bold: true
                            font.pixelSize: root.theme.fontCaption
                        }
                    }

                    SequentialAnimation {
                        id: acknowledgeAnimation
                        ParallelAnimation {
                            NumberAnimation { target: countBadge; property: "scale"; to: 1.45
                                              duration: root.theme.durFast
                                              easing.type: Easing.OutQuad }
                            NumberAnimation { target: queueButton; property: "opacity"; to: 0.55
                                              duration: root.theme.durFast }
                        }
                        ParallelAnimation {
                            NumberAnimation { target: countBadge; property: "scale"; to: 1.0
                                              duration: root.theme.durStandard
                                              easing.type: Easing.OutBack }
                            NumberAnimation { target: queueButton; property: "opacity"; to: 1.0
                                              duration: root.theme.durStandard }
                        }
                    }

                    Inspect.ThemedToolTip {
                        theme: root.theme
                        visible: parent.hovered
                        text: root.draft && root.draft.queueCount > 0
                              ? qsTr("Open the queue to set format and destination, then export")
                              : qsTr("Add a drawing to the queue first")
                    }
                }
            }
        }
    }

    DrawingExportSheet {
        id: exportSheet
        objectName: "drawingExportSheet"
        draft: root.draft
        theme: root.theme
        previews: root.queuedPreviews
    }

    function applyFaceFrame(frame) {
        if (!root.draft || !frame) return
        // The controller decides whether the face is flat enough; passing the deviation
        // rather than a verdict keeps that judgement in one place.
        root.draft.setCandidateFaceNormal(frame.normal.x, frame.normal.y, frame.normal.z,
                                          frame.maximumDeviationDegrees)
    }
}
