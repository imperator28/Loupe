import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import "NearClip.js" as NearClip

// What the Measure tool draws over the model, in screen space so it reads at any zoom:
//   - under the pointer, the snap point a click would take (a vertex's square, a circle
//     centre's crosshair, an edge point's diamond, a face point's ring) and a chip naming it
//     -- "Flat face · 240.00 mm²", or why this mode cannot use it;
//   - the edge under the pointer, bold;
//   - the result in progress and every pinned one, each drawn where it was measured
//     (MeasurementAnnotation): bold picked edges, a dimension line, radius leader, angle arc,
//     bounding box or value tag, and the picks.
// Hover uses the accent (the temporary role), accepted picks and results the measurement
// amber, so what is about to happen never reads as what has happened.
Item {
    id: root
    // Nothing it draws leaves the viewport: an edge running off the view is cut at its border,
    // not drawn on over the panels beside it.
    clip: true

    property QtObject theme
    property QtObject visualTheme
    // The View3D the scene points are projected through, and a number that changes whenever
    // the camera does (mapFrom3DScene has no notifier of its own).
    property var view: null
    property int revision: 0
    property QtObject measurement: null
    // The candidate under the pointer (StepViewport.measureCandidateAt), and the pointer.
    property var hover: null
    property point pointer: Qt.point(0, 0)
    property bool showHover: true
    property bool showResult: true
    // The measurement in progress, apart from the pinned ones.
    property bool showCurrent: true
    // Scene length of the normal drawn from a hovered face whose plane a tool would use
    // (hover.info.previewAt, previewNormal); 0 draws none.
    property real normalPreviewLength: 0

    readonly property color hoverColor: theme ? theme.accent : "transparent"
    readonly property color haloColor: visualTheme ? visualTheme.markerBorder : "transparent"

    // Where a scene point is off the screen, or behind the camera: far outside every margin.
    readonly property point offScreen: Qt.point(-10000, -10000)

    // The camera's position, view direction and near plane, for clipping against it. An
    // orthographic camera projects nothing mirrored, so it clips nothing.
    function cameraFrame() {
        const camera = view ? view.camera : null
        if (!camera || camera.clipNear === undefined || camera.fieldOfView === undefined) return null
        const position = camera.scenePosition
        const forward = camera.forward
        // Cut a little in front of the plane: right on it, points project to infinity.
        return { position: position, forward: forward, near: Math.max(camera.clipNear * 1.01, camera.clipNear + 1e-4) }
    }
    function depthFunction(frame) {
        return function(point) {
            return (point.x - frame.position.x) * frame.forward.x + (point.y - frame.position.y) * frame.forward.y
                 + (point.z - frame.position.z) * frame.forward.z
        }
    }

    // The screen position of a point already known to be in front of the camera. A capture
    // renders a larger texture with the orthographic magnification raised to match, so the
    // camera's normalised projection, and so this, is the same during a capture as on screen.
    function projectInFront(point) {
        const screen = view.mapFrom3DScene(point)
        return Qt.point(screen.x, screen.y)
    }
    function project(point) {
        if (!view || point === undefined || point === null || revision < 0) return offScreen
        const frame = cameraFrame()
        if (frame && depthFunction(frame)(point) < frame.near) return offScreen
        return projectInFront(point)
    }
    function isOffScreen(point) { return point.x <= offScreen.x + 1 && point.y <= offScreen.y + 1 }
    function onScreen(point, margin) {
        return point.x >= -margin && point.x <= width + margin && point.y >= -margin && point.y <= height + margin
    }
    // A polyline as the screen runs of it in front of the camera: [[point, ...], ...].
    function projectPath(points) {
        if (!view || !points || points.length < 2 || revision < 0) return []
        const frame = cameraFrame()
        const runs = frame ? NearClip.clipPolyline(points, depthFunction(frame), frame.near) : [points]
        const result = []
        for (const run of runs) {
            const screenRun = []
            for (const point of run) screenRun.push(projectInFront(point))
            result.push(screenRun)
        }
        return result
    }
    // The part of segment a-b in front of the camera, on screen, as [from, to]; null if none.
    function projectSegment(a, b) {
        if (!view || !a || !b || revision < 0) return null
        const frame = cameraFrame()
        const clipped = frame ? NearClip.clipSegment(a, b, depthFunction(frame), frame.near) : [a, b]
        return clipped ? [projectInFront(clipped[0]), projectInFront(clipped[1])] : null
    }
    // A closed polygon's part in front of the camera, on screen; empty if none.
    function projectPolygon(points) {
        if (!view || !points || points.length < 3 || revision < 0) return []
        const frame = cameraFrame()
        const clipped = frame ? NearClip.clipPolygon(points, depthFunction(frame), frame.near) : points
        const result = []
        for (const point of clipped) result.push(projectInFront(point))
        return result
    }
    // Screen runs as one SVG path, a move to the start of each.
    function svgRuns(runs) {
        let path = ""
        for (const run of runs) {
            if (run.length < 2) continue
            path += (path.length ? " M " : "M ") + run[0].x.toFixed(1) + " " + run[0].y.toFixed(1)
            for (let index = 1; index < run.length; ++index) path += " L " + run[index].x.toFixed(1) + " " + run[index].y.toFixed(1)
        }
        return path
    }

    // ---- Pinned results: kept where they were measured, under the one in progress ----
    Repeater {
        model: root.measurement ? root.measurement.pinnedResults : []
        delegate: MeasurementAnnotation {
            required property var modelData
            objectName: "measurementPinned-" + modelData.id
            anchors.fill: parent
            projector: root
            theme: root.theme
            visualTheme: root.visualTheme
            visible: root.showResult
            pinned: true
            numbered: false
            draggable: true
            lineColor: root.theme ? root.theme.measureColor(modelData.color) : "transparent"
            tagOffset: modelData.offset !== undefined ? modelData.offset : Qt.point(0, 0)
            onTagMoved: function(x, y) { if (root.measurement) root.measurement.setPinOffset(modelData.id, x, y) }
            title: modelData.title
            annotation: modelData.annotation
            edges: modelData.edges
            points: modelData.points
        }
    }

    // ---- The result in progress ----
    MeasurementAnnotation {
        objectName: "measurementResult"
        anchors.fill: parent
        projector: root
        theme: root.theme
        visualTheme: root.visualTheme
        visible: root.showResult && root.showCurrent
        annotation: root.measurement ? root.measurement.annotation : ({})
        points: root.measurement ? root.measurement.pickedPoints : []
        edges: {
            const result = []
            for (const entry of root.measurement ? root.measurement.pickedEntities : []) {
                if (entry.polyline && entry.polyline.length > 1) result.push(entry.polyline)
            }
            return result
        }
    }

    // The edge under the pointer, bold in the hover accent, so it is clear which one a click takes.
    // Its runs in front of the camera, on screen.
    readonly property var hoverRuns: {
        if (!root.showHover || !root.hover || !root.hover.info || !root.hover.info.polyline) return []
        return root.projectPath(root.hover.info.polyline)
    }
    // As an SVG path: what the icons draw with, on every graphics back end.
    readonly property string hoverPath: svgRuns(hoverRuns)
    Shape {
        objectName: "measurementHoverEdge"
        anchors.fill: parent
        visible: root.hoverPath.length > 0
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(root.haloColor.r, root.haloColor.g, root.haloColor.b, 0.5)
            strokeWidth: 5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.hoverPath }
        }
        ShapePath {
            strokeColor: root.hoverColor
            strokeWidth: 3
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.hoverPath }
        }
    }

    // A face whose plane a click would use: its normal, from where the pointer is, so it is
    // clear which way the plane faces before the click.
    readonly property var normalPreview: {
        const info = root.showHover && root.hover && root.hover.info ? root.hover.info : null
        if (!info || info.previewAt === undefined || info.previewNormal === undefined || root.normalPreviewLength <= 0)
            return null
        const at = info.previewAt
        const n = info.previewNormal
        const length = root.normalPreviewLength
        // Drawn only when its foot is in front of the camera; its tip is cut at the near plane.
        if (root.isOffScreen(root.project(at))) return null
        const segment = root.projectSegment(at, Qt.vector3d(at.x + n.x * length, at.y + n.y * length, at.z + n.z * length))
        if (!segment) return null
        const from = segment[0]
        const to = segment[1]
        const dx = to.x - from.x
        const dy = to.y - from.y
        const pixels = Math.sqrt(dx * dx + dy * dy)
        return { from: from, to: to, pixels: pixels, ux: pixels > 0 ? dx / pixels : 0, uy: pixels > 0 ? dy / pixels : 0 }
    }
    readonly property string normalPreviewPath: {
        const arrow = normalPreview
        if (!arrow || arrow.pixels < 6) return ""
        const head = Math.min(9, arrow.pixels * 0.4)
        const baseX = arrow.to.x - arrow.ux * head
        const baseY = arrow.to.y - arrow.uy * head
        const wingX = -arrow.uy * head * 0.5
        const wingY = arrow.ux * head * 0.5
        const f = function(value) { return value.toFixed(1) }
        return "M " + f(arrow.from.x) + " " + f(arrow.from.y) + " L " + f(arrow.to.x) + " " + f(arrow.to.y)
             + " M " + f(baseX + wingX) + " " + f(baseY + wingY) + " L " + f(arrow.to.x) + " " + f(arrow.to.y)
             + " L " + f(baseX - wingX) + " " + f(baseY - wingY)
    }
    Shape {
        objectName: "measurementNormalPreview"
        anchors.fill: parent
        visible: root.normalPreviewPath.length > 0
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: Qt.rgba(root.haloColor.r, root.haloColor.g, root.haloColor.b, 0.5)
            strokeWidth: 5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.normalPreviewPath }
        }
        ShapePath {
            strokeColor: root.hoverColor
            strokeWidth: 2.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.normalPreviewPath }
        }
    }
    // The point it is drawn from: a ring, as a point on a face is marked.
    Rectangle {
        visible: root.normalPreviewPath.length > 0
        x: root.normalPreview ? Math.round(root.normalPreview.from.x) - 4 : 0
        y: root.normalPreview ? Math.round(root.normalPreview.from.y) - 4 : 0
        width: 8; height: 8; radius: 4
        color: root.haloColor
        border.color: root.hoverColor
        border.width: 2
    }

    // ---- The hover: where a click would land, and what it would take ----
    readonly property var hoverInfo: hover && hover.info ? hover.info : null
    readonly property string hoverKind: hoverInfo && hoverInfo.kind ? hoverInfo.kind : ""
    readonly property bool hoverEligible: hoverInfo !== null && hoverInfo.eligible === true
    readonly property point snapScreen: hoverInfo && hoverInfo.point !== undefined ? project(hoverInfo.point) : Qt.point(-10000, -10000)
    readonly property bool hoverIsPoint: hoverKind === "vertex" || hoverKind === "centre" || hoverKind === "edgePoint"
                                         || hoverKind === "midpoint" || hoverKind === "surfacePoint"

    Item {
        id: snapMarker
        objectName: "measurementSnapMarker"
        visible: root.showHover && root.hoverIsPoint && root.onScreen(root.snapScreen, 0)
        x: Math.round(root.snapScreen.x)
        y: Math.round(root.snapScreen.y)

        // Vertex: a square, as CAD tools mark an end point.
        Rectangle {
            visible: root.hoverKind === "vertex"
            x: -6; y: -6; width: 12; height: 12
            color: "transparent"
            border.color: root.hoverColor
            border.width: 2
            Rectangle { anchors.fill: parent; anchors.margins: -1; color: "transparent"; border.color: root.haloColor; border.width: 1; z: -1 }
        }
        // Circle centre: a ring with a cross.
        Item {
            visible: root.hoverKind === "centre"
            Rectangle { x: -7; y: -7; width: 14; height: 14; radius: 7; color: "transparent"; border.color: root.haloColor; border.width: 4 }
            Rectangle { x: -7; y: -7; width: 14; height: 14; radius: 7; color: "transparent"; border.color: root.hoverColor; border.width: 2 }
            Rectangle { x: -10; y: -1; width: 20; height: 2; color: root.hoverColor }
            Rectangle { x: -1; y: -10; width: 2; height: 20; color: root.hoverColor }
        }
        // An edge's midpoint: a triangle, as CAD tools mark one.
        Shape {
            visible: root.hoverKind === "midpoint"
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.haloColor
                strokeWidth: 4
                fillColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M 0 -7 L 6.5 5 L -6.5 5 Z" }
            }
            ShapePath {
                strokeColor: root.hoverColor
                strokeWidth: 2
                fillColor: "transparent"
                joinStyle: ShapePath.RoundJoin
                PathSvg { path: "M 0 -7 L 6.5 5 L -6.5 5 Z" }
            }
        }
        // A point on an edge: a diamond.
        Rectangle {
            visible: root.hoverKind === "edgePoint"
            x: -5; y: -5; width: 10; height: 10
            rotation: 45
            color: root.hoverColor
            border.color: root.haloColor
            border.width: 1
        }
        // A point on a face: a small ring.
        Rectangle {
            visible: root.hoverKind === "surfacePoint"
            x: -4; y: -4; width: 8; height: 8; radius: 4
            color: root.haloColor
            border.color: root.hoverColor
            border.width: 2
        }
    }

    // The chip by the pointer: what a click takes, or why it takes nothing.
    Rectangle {
        id: hoverChip
        objectName: "measurementHoverChip"
        readonly property real gap: 18
        visible: root.showHover && root.hoverInfo !== null && root.hoverInfo.title !== undefined
                 && String(root.hoverInfo.title).length > 0
        // Below and right of the pointer, flipped where it would leave the viewport.
        x: Math.round(root.pointer.x + gap + width > root.width ? root.pointer.x - gap - width : root.pointer.x + gap)
        y: Math.round(root.pointer.y + gap + height > root.height ? root.pointer.y - gap - height : root.pointer.y + gap)
        width: Math.min(260, Math.max(titleRow.implicitWidth, root.hoverEligible ? 0 : reasonLabel.implicitWidth) + 20)
        height: chipContent.implicitHeight + 12
        radius: root.theme ? root.theme.radius2 : 0
        color: root.theme ? root.theme.surface3 : "transparent"
        border.color: root.theme ? root.theme.borderPanel : "transparent"
        border.width: 1

        Column {
            id: chipContent
            x: 10
            y: 6
            width: hoverChip.width - 20
            spacing: 2

            Row {
                id: titleRow
                spacing: 6
                ToolIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 14
                    height: 14
                    name: root.hoverEligible ? "measure" : "alert"
                    color: root.hoverEligible ? root.hoverColor : (root.theme ? root.theme.warning : "transparent")
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.hoverInfo && root.hoverInfo.title ? root.hoverInfo.title : ""
                    color: root.theme ? (root.hoverEligible ? root.theme.foreground : root.theme.muted) : "transparent"
                    font.pixelSize: root.theme ? root.theme.fontSection : 12
                    font.weight: Font.Medium
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text.length > 0
                    text: root.hoverInfo && root.hoverInfo.detail ? root.hoverInfo.detail : ""
                    color: root.theme ? root.theme.muted : "transparent"
                    font.pixelSize: root.theme ? root.theme.fontSection : 12
                    font.features: { "tnum": 1 }
                }
            }
            Label {
                id: reasonLabel
                width: chipContent.width
                visible: !root.hoverEligible && text.length > 0
                text: root.hoverInfo && root.hoverInfo.reason ? root.hoverInfo.reason : ""
                wrapMode: Text.WordWrap
                color: root.theme ? root.theme.muted : "transparent"
                font.pixelSize: root.theme ? root.theme.fontCaption : 11
            }
        }
    }
}
