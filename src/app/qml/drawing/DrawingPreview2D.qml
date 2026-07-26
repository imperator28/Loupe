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

    // Sizes itself to its content so the warnings below the canvas cannot overflow the
    // panel. The enclosing column is scrollable, so growing is preferable to clipping.
    implicitHeight: content.implicitHeight + (theme ? theme.spacing3 * 2 : 24)

    // Display-only fit. The written file is always 1:1 whatever this happens to be.
    readonly property real fitScale: {
        if (!hasGeometry) return 1
        const w = preview.widthMm
        const h = preview.heightMm
        if (!(w > 0) || !(h > 0)) return 1
        return Math.max(0.0001, Math.min((canvasFrame.width - 24) / w, (canvasFrame.height - 24) / h))
    }
    readonly property real surfaceWidth: hasGeometry ? preview.widthMm * fitScale : 0
    readonly property real surfaceHeight: hasGeometry ? preview.heightMm * fitScale : 0

    // Display-only navigation. None of it reaches the drawing: the exported file is built
    // from the projection, never from what the canvas happens to be showing, so zooming in to
    // inspect a corner cannot change what gets cut.
    property real viewScale: 1.0
    property real panX: 0
    property real panY: 0
    property real viewRotation: 0

    function resetView() {
        root.viewScale = 1.0
        root.panX = 0
        root.panY = 0
        root.viewRotation = 0
    }

    function zoomAt(factor, pointX, pointY) {
        const next = Math.max(0.2, Math.min(40.0, root.viewScale * factor))
        const applied = next / root.viewScale
        // Keep the point under the cursor fixed, measured from the frame centre, so zooming
        // magnifies what is being looked at rather than the middle of the panel.
        const centreX = pointX - canvasFrame.width / 2
        const centreY = pointY - canvasFrame.height / 2
        root.panX = centreX - applied * (centreX - root.panX)
        root.panY = centreY - applied * (centreY - root.panY)
        root.viewScale = next
    }

    // One polyline list per stroke style rather than one object per contour.
    //
    // This is not a micro-optimisation, it is the only thing that works: Repeater requires
    // an Item delegate, and ShapePath is not an Item, so a Repeater over contours silently
    // adds nothing to the Shape and the preview renders blank. PathMultiline exists for
    // exactly this shape of data.
    readonly property var closedPaths: root.buildPaths(true)
    readonly property var openPaths: root.buildPaths(false)

    function buildPaths(closed) {
        if (!root.hasGeometry) return []
        const result = []
        const scale = root.fitScale
        const height = root.surfaceHeight
        const originX = root.preview.minX
        const originY = root.preview.minY
        for (const layer of root.preview.layers) {
            for (const contour of layer.contours) {
                if (contour.closed !== closed) continue
                const points = contour.points
                const polyline = []
                for (let index = 0; index + 1 < points.length; index += 2) {
                    // Y is flipped for display only: the IR is Y-up, the scene is Y-down.
                    // The writers do their own conversion.
                    polyline.push(Qt.point((points[index] - originX) * scale,
                                           height - (points[index + 1] - originY) * scale))
                }
                if (polyline.length > 1) result.push(polyline)
            }
        }
        return result
    }

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
        // A different drawing at the previous pan and zoom would open somewhere arbitrary.
        root.resetView()
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
        id: content
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
                elide: Text.ElideRight
            }
            Inspect.ThemedToolButton {
                objectName: "drawingPreviewFit"
                theme: root.theme
                visible: root.hasGeometry
                text: "\u2922"
                enabled: root.viewScale !== 1.0 || root.panX !== 0 || root.panY !== 0
                         || root.viewRotation !== 0
                onClicked: root.resetView()
                Inspect.ThemedToolTip {
                    theme: root.theme
                    visible: parent.hovered
                    text: qsTr("Fit the drawing back into the frame")
                }
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
            implicitHeight: 200
            color: root.theme ? root.theme.surfaceSubtle : "transparent"
            border.color: root.theme ? root.theme.border : "transparent"
            radius: root.theme.radius1
            clip: true

            Item {
                objectName: "drawingPreviewSurface"
                visible: root.hasGeometry
                width: root.surfaceWidth
                height: root.surfaceHeight
                // Positioned rather than anchored, since panning moves it off centre.
                x: (canvasFrame.width - width) / 2 + root.panX
                y: (canvasFrame.height - height) / 2 + root.panY
                scale: root.viewScale
                rotation: root.viewRotation
                transformOrigin: Item.Center

                Shape {
                    anchors.fill: parent
                    asynchronous: false

                    ShapePath {
                        objectName: "drawingPreviewClosedPath"
                        strokeColor: root.theme ? root.theme.accent : "transparent"
                        strokeWidth: 1
                        fillColor: "transparent"
                        PathMultiline { paths: root.closedPaths }
                    }
                    ShapePath {
                        objectName: "drawingPreviewOpenPath"
                        // An open contour will not cut, so it is drawn in the warning
                        // colour rather than being indistinguishable from a closed one.
                        strokeColor: root.theme ? root.theme.warning : "transparent"
                        strokeWidth: 1
                        fillColor: "transparent"
                        PathMultiline { paths: root.openPaths }
                    }
                }
            }

            MouseArea {
                id: canvasInput
                anchors.fill: parent
                enabled: root.hasGeometry
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.ArrowCursor
                property real lastX: 0
                property real lastY: 0
                property real lastAngle: 0

                function angleAt(x, y) {
                    return Math.atan2(y - height / 2, x - width / 2) * 180 / Math.PI
                }

                onPressed: function(mouse) {
                    lastX = mouse.x
                    lastY = mouse.y
                    lastAngle = canvasInput.angleAt(mouse.x, mouse.y)
                }
                onPositionChanged: function(mouse) {
                    if (!pressed) return
                    if (mouse.modifiers & Qt.AltModifier) {
                        // Alt+drag rotates in plane, the same gesture the 3D viewport uses,
                        // rather than a second convention to remember.
                        const angle = canvasInput.angleAt(mouse.x, mouse.y)
                        let delta = angle - lastAngle
                        // Normalise across the +/-180 seam, or a drag past it would spin.
                        if (delta > 180) delta -= 360
                        else if (delta < -180) delta += 360
                        root.viewRotation += delta
                        lastAngle = angle
                    } else {
                        root.panX += mouse.x - lastX
                        root.panY += mouse.y - lastY
                    }
                    lastX = mouse.x
                    lastY = mouse.y
                }
                onDoubleClicked: root.resetView()
                onWheel: function(wheel) {
                    const notches = wheel.angleDelta.y / 120
                    if (notches === 0) return
                    root.zoomAt(Math.pow(1.15, notches), wheel.x, wheel.y)
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
            objectName: "drawingPreviewNavigationHint"
            visible: root.hasGeometry
            text: qsTr("Drag to pan · scroll to zoom · Alt+drag to rotate · double-click to fit")
            color: root.theme.muted
            wrapMode: Text.Wrap
            font.pixelSize: root.theme.fontCaption
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

        // Warnings are joined into one wrapping label rather than a Repeater of labels, so
        // the panel's implicit height accounts for all of them and none can spill out.
        Label {
            Layout.fillWidth: true
            objectName: "drawingPreviewWarnings"
            visible: root.hasGeometry && text.length > 0
            text: {
                if (!root.hasGeometry) return ""
                const lines = []
                for (const warning of root.preview.warnings) {
                    if (warning.code === "non_solid_bodies_ignored")
                        lines.push(qsTr("Ignored %1 non-solid body(ies): they have no interior to bound.")
                                       .arg(warning.count))
                    else if (warning.code === "silhouette_unavailable")
                        lines.push(qsTr("The outline could not be isolated for this view."))
                    else if (warning.code === "coarse_curve_fallback")
                        lines.push(qsTr("%1 curve(s) fell back to a coarser approximation.").arg(warning.count))
                    else if (warning.code === "open_contour")
                        lines.push(qsTr("%1 contour(s) could not be closed.").arg(warning.count))
                    else if (warning.code === "duplicate_edge_removed")
                        lines.push(qsTr("Removed %1 duplicate edge(s).").arg(warning.count))
                    else lines.push(warning.code)
                }
                return lines.join("\n")
            }
            color: root.theme.warning
            wrapMode: Text.Wrap
            font.pixelSize: root.theme.fontCaption
        }
    }
}
