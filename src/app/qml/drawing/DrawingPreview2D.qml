import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../inspect" as Inspect

// The 2D preview of the candidate drawing.
//
// Two rules drive the whole file. It must never present stale geometry as current, so a
// superseded revision blanks the geometry rather than leaving the previous drawing on
// screen looking authoritative. And it must state its measured extents, because those are
// what a user checks a 1:1 drawing against -- a picture alone cannot be verified.
Inspect.ElevatedPanel {
    id: root

    property QtObject draft
    // Set from the controller's drawingPreviewReady signal.
    property var preview: null
    property int previewRevision: 0
    property bool approximate: false
    readonly property color foreground: theme ? theme.foreground : "transparent"
    // Resolving whenever the candidate has moved past the drawing currently held.
    readonly property bool resolving: draft !== null && draft.candidateValid
                                      && previewRevision !== draft.previewRevision
    readonly property bool hasGeometry: preview !== null && !resolving
                                        && preview.empty === false
                                        && previewRevision === (draft ? draft.previewRevision : -1)

    function applyPreview(json, revision, isApproximate) {
        // A reply for anything but the newest candidate is dropped outright.
        if (!root.draft || revision !== root.draft.previewRevision) return
        try {
            root.preview = JSON.parse(json)
        } catch (error) {
            root.preview = null
        }
        root.previewRevision = revision
        root.approximate = isApproximate
    }

    function clearPreview() {
        root.preview = null
        root.previewRevision = 0
        root.approximate = false
    }

    Connections {
        target: root.draft
        // A new candidate invalidates what is on screen immediately, before any reply
        // arrives, so nothing stale is ever shown as current.
        function onCandidateChanged() {
            if (!root.draft || root.previewRevision !== root.draft.previewRevision) root.preview = null
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing2

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("2D preview")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
            Label {
                objectName: "drawingPreviewExtents"
                visible: root.hasGeometry
                text: root.hasGeometry
                      ? qsTr("%1 × %2 mm").arg(root.preview.widthMm.toFixed(2))
                                          .arg(root.preview.heightMm.toFixed(2))
                      : ""
                color: root.foreground
                font.bold: true
            }
        }

        Rectangle {
            id: canvasFrame
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            color: root.theme ? root.theme.surfaceSubtle : "transparent"
            border.color: root.theme ? root.theme.border : "transparent"
            radius: root.theme.radius1
            clip: true

            // Fit the drawing into the frame. Display-only: the written file is always
            // 1:1 regardless of what this scale happens to be.
            readonly property real fitScale: {
                if (!root.hasGeometry) return 1
                const w = root.preview.widthMm
                const h = root.preview.heightMm
                if (!(w > 0) || !(h > 0)) return 1
                return Math.min((width - 24) / w, (height - 24) / h)
            }

            Item {
                id: drawingSurface
                visible: root.hasGeometry
                width: root.hasGeometry ? root.preview.widthMm * canvasFrame.fitScale : 0
                height: root.hasGeometry ? root.preview.heightMm * canvasFrame.fitScale : 0
                anchors.centerIn: parent

                Repeater {
                    model: root.hasGeometry ? root.preview.layers : []
                    delegate: Shape {
                        required property var modelData
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer

                        Repeater {
                            model: modelData.contours
                            delegate: ShapePath {
                                required property var modelData
                                strokeColor: root.theme
                                             ? (modelData.closed ? root.theme.accent : root.theme.warning)
                                             : "transparent"
                                strokeWidth: 1
                                fillColor: "transparent"
                                // Y is flipped for display only: the IR is Y-up, the scene
                                // is Y-down. The writers do their own conversion.
                                startX: (modelData.points[0] - root.preview.minX) * canvasFrame.fitScale
                                startY: drawingSurface.height
                                        - (modelData.points[1] - root.preview.minY) * canvasFrame.fitScale

                                PathPolyline {
                                    path: {
                                        const result = []
                                        const points = modelData.points
                                        for (let index = 0; index + 1 < points.length; index += 2) {
                                            result.push(Qt.point(
                                                (points[index] - root.preview.minX) * canvasFrame.fitScale,
                                                drawingSurface.height
                                                - (points[index + 1] - root.preview.minY) * canvasFrame.fitScale))
                                        }
                                        return result
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: !root.hasGeometry
                width: parent.width - 32
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: root.theme ? root.theme.muted : "transparent"
                text: root.resolving ? qsTr("Projecting…")
                      : !root.draft || !root.draft.candidateValid
                        ? qsTr("Choose a part and a view to preview the drawing")
                        : qsTr("This view produced no outline")
            }
        }

        Label {
            Layout.fillWidth: true
            objectName: "drawingPreviewApproximate"
            visible: root.hasGeometry && root.approximate
            text: qsTr("Approximate preview. The exported file is computed exactly.")
            color: root.theme.warning
            wrapMode: Text.Wrap
            font.pixelSize: root.theme.fontCaption
        }

        Label {
            Layout.fillWidth: true
            objectName: "drawingPreviewContours"
            visible: root.hasGeometry
            text: {
                if (!root.hasGeometry) return ""
                const open = root.preview.openContours
                return open > 0
                       ? qsTr("%1 closed, %2 not closed — an open contour will not cut")
                             .arg(root.preview.closedContours).arg(open)
                       : qsTr("%1 closed contour(s)").arg(root.preview.closedContours)
            }
            color: root.hasGeometry && root.preview.openContours > 0
                   ? root.theme.warning : root.theme.muted
            wrapMode: Text.Wrap
            font.pixelSize: root.theme.fontCaption
        }

        Repeater {
            model: root.hasGeometry ? root.preview.warnings : []
            delegate: Label {
                required property var modelData
                Layout.fillWidth: true
                text: {
                    if (modelData.code === "non_solid_bodies_ignored")
                        return qsTr("Ignored %1 non-solid body(ies): they have no interior to bound.")
                                   .arg(modelData.count)
                    if (modelData.code === "silhouette_unavailable")
                        return qsTr("The outline could not be isolated for this view.")
                    if (modelData.code === "coarse_curve_fallback")
                        return qsTr("%1 curve(s) fell back to a coarser approximation.").arg(modelData.count)
                    if (modelData.code === "open_contour")
                        return qsTr("%1 contour(s) could not be closed.").arg(modelData.count)
                    if (modelData.code === "duplicate_edge_removed")
                        return qsTr("Removed %1 duplicate edge(s).").arg(modelData.count)
                    return modelData.code
                }
                color: root.theme.warning
                wrapMode: Text.Wrap
                font.pixelSize: root.theme.fontCaption
            }
        }
    }
}
