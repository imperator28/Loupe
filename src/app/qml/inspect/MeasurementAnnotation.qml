import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// One measurement drawn where it was measured, in screen space over the model: the result in
// progress, or one pinned to the view. Its parts, all anchored to scene points so they follow
// the camera:
//   - the picked edges, bold (a 3D line is one pixel wide on most graphics back ends);
//   - the annotation: a dimension line ("distance"), a radius leader ("radius"), an arc from
//     one face or edge to the other with a leg along each ("angle"), a box with its three
//     sizes on its edges ("box"), or a value tag on the picked edge or face ("label");
//   - the picks, as numbered markers, or as small dots once pinned.
Item {
    id: root

    // The overlay that projects scene points: project(point) and onScreen(point, margin).
    required property Item projector
    property QtObject theme
    property QtObject visualTheme
    property var annotation: ({})
    // Polylines ([vector3d, ...] each) and pick points ([vector3d, ...]).
    property var edges: []
    property var points: []
    property bool numbered: true
    // A pinned result names what it is on its tag ("Gap 5.000 mm").
    property string title: ""
    property bool pinned: false

    readonly property color lineColor: visualTheme ? visualTheme.selectionColor : "transparent"
    readonly property color tagTextColor: visualTheme ? visualTheme.sectionLabel : "transparent"
    readonly property color haloColor: visualTheme ? visualTheme.markerBorder : "transparent"
    readonly property color halo: Qt.rgba(haloColor.r, haloColor.g, haloColor.b, 0.55)
    readonly property string kind: annotation && annotation.kind ? annotation.kind : ""
    readonly property string tagText: {
        const text = annotation && annotation.text ? annotation.text : ""
        return pinned && title.length > 0 && kind !== "box" ? title + "  " + text : text
    }

    function screenPath(scenePoints) {
        const result = []
        if (!scenePoints) return result
        for (const point of scenePoints) {
            const screen = projector.project(point)
            result.push(Qt.point(screen.x, screen.y))
        }
        return result
    }

    // ---- Picked edges, bold ----
    Repeater {
        model: root.edges
        delegate: Shape {
            id: edgeShape
            required property var modelData
            readonly property var path: root.screenPath(modelData)
            anchors.fill: parent
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: root.halo
                strokeWidth: root.pinned ? 4 : 5.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathPolyline { path: edgeShape.path }
            }
            ShapePath {
                strokeColor: root.lineColor
                strokeWidth: root.pinned ? 2.5 : 3.5
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathPolyline { path: edgeShape.path }
            }
        }
    }

    // ---- Dimension line and radius leader ----
    readonly property bool hasLine: kind === "distance" || kind === "radius"
    readonly property point fromScreen: hasLine ? projector.project(annotation.from) : Qt.point(0, 0)
    readonly property point toScreen: hasLine ? projector.project(annotation.to) : Qt.point(0, 0)
    readonly property real lineLength: Math.hypot(toScreen.x - fromScreen.x, toScreen.y - fromScreen.y)
    readonly property real tickX: lineLength > 0 ? -(toScreen.y - fromScreen.y) / lineLength * 6 : 0
    readonly property real tickY: lineLength > 0 ? (toScreen.x - fromScreen.x) / lineLength * 6 : 0

    Shape {
        objectName: "measurementDimension"
        anchors.fill: parent
        visible: root.hasLine && (root.projector.onScreen(root.fromScreen, 0) || root.projector.onScreen(root.toScreen, 0))
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.halo
            strokeWidth: 3.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathMove { x: root.fromScreen.x; y: root.fromScreen.y }
            PathLine { x: root.toScreen.x; y: root.toScreen.y }
        }
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathMove { x: root.fromScreen.x; y: root.fromScreen.y }
            PathLine { x: root.toScreen.x; y: root.toScreen.y }
            // End ticks: a dimension, not a leader. A radius has a cross at its centre instead.
            PathMove { x: root.toScreen.x - root.tickX; y: root.toScreen.y - root.tickY }
            PathLine { x: root.toScreen.x + root.tickX; y: root.toScreen.y + root.tickY }
            PathMove {
                x: root.kind === "radius" ? root.fromScreen.x - 5 : root.fromScreen.x - root.tickX
                y: root.kind === "radius" ? root.fromScreen.y : root.fromScreen.y - root.tickY
            }
            PathLine {
                x: root.kind === "radius" ? root.fromScreen.x + 5 : root.fromScreen.x + root.tickX
                y: root.kind === "radius" ? root.fromScreen.y : root.fromScreen.y + root.tickY
            }
            PathMove { x: root.fromScreen.x; y: root.kind === "radius" ? root.fromScreen.y - 5 : root.fromScreen.y }
            PathLine { x: root.fromScreen.x; y: root.kind === "radius" ? root.fromScreen.y + 5 : root.fromScreen.y }
        }
    }

    // ---- Angle: an arc from one pick to the other, a leg along each ----
    readonly property var arcPath: kind === "angle" ? screenPath(annotation.arc) : []
    readonly property point arcCentre: kind === "angle" ? projector.project(annotation.centre) : Qt.point(0, 0)
    readonly property point legFrom: kind === "angle" ? projector.project(annotation.legFrom) : Qt.point(0, 0)
    readonly property point legTo: kind === "angle" ? projector.project(annotation.legTo) : Qt.point(0, 0)

    Shape {
        objectName: "measurementAngleArc"
        anchors.fill: parent
        visible: root.kind === "angle" && root.arcPath.length > 1
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.halo
            strokeWidth: 4
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.arcPath }
        }
        // The legs: from where the picks meet, out along each one, so it reads which to which.
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 1.25
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            strokeStyle: ShapePath.DashLine
            dashPattern: [4, 3]
            PathMove { x: root.arcCentre.x; y: root.arcCentre.y }
            PathLine { x: root.legFrom.x; y: root.legFrom.y }
            PathMove { x: root.arcCentre.x; y: root.arcCentre.y }
            PathLine { x: root.legTo.x; y: root.legTo.y }
        }
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 2
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathPolyline { path: root.arcPath }
        }
    }

    // ---- Bounds: the box's twelve edges, and its size along X, Y and Z on one edge each ----
    readonly property var corners: {
        if (kind !== "box") return []
        const a = annotation.minimum
        const b = annotation.maximum
        const result = []
        for (let index = 0; index < 8; ++index)
            result.push(projector.project(Qt.vector3d(index & 1 ? b.x : a.x, index & 2 ? b.y : a.y, index & 4 ? b.z : a.z)))
        return result
    }
    // Corner pairs along X (bit 1), Y (bit 2) and Z (bit 4).
    readonly property var boxEdges: [[[0, 1], [2, 3], [4, 5], [6, 7]], [[0, 2], [1, 3], [4, 6], [5, 7]], [[0, 4], [1, 5], [2, 6], [3, 7]]]
    // The edge each size goes on: an outline edge (far from the box's middle on screen, so the
    // tag does not sit over the model), and away from the tags already placed, so three edges
    // meeting at a corner do not stack their tags on it.
    readonly property var tagEdges: {
        if (corners.length !== 8) return [[0, 0], [0, 0], [0, 0]]
        let cx = 0
        let cy = 0
        for (const corner of corners) { cx += corner.x / 8; cy += corner.y / 8 }
        const chosen = []
        const middles = []
        for (let axis = 0; axis < 3; ++axis) {
            let best = boxEdges[axis][0]
            let bestScore = -Infinity
            let bestMiddle = null
            for (const pair of boxEdges[axis]) {
                const mx = (corners[pair[0]].x + corners[pair[1]].x) / 2
                const my = (corners[pair[0]].y + corners[pair[1]].y) / 2
                let apart = 1e6
                for (const other of middles) apart = Math.min(apart, Math.hypot(mx - other.x, my - other.y))
                const score = Math.hypot(mx - cx, my - cy) + 2 * Math.min(apart, 120) + my * 1e-3
                if (score > bestScore) { bestScore = score; best = pair; bestMiddle = Qt.point(mx, my) }
            }
            chosen.push(best)
            middles.push(bestMiddle)
        }
        return chosen
    }

    Shape {
        objectName: "measurementBox"
        anchors.fill: parent
        visible: root.kind === "box" && root.corners.length === 8
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 1.25
            fillColor: "transparent"
            strokeStyle: ShapePath.DashLine
            dashPattern: [5, 3]
            PathMultiline {
                paths: {
                    const result = []
                    if (root.corners.length !== 8) return result
                    for (const axis of root.boxEdges) {
                        for (const pair of axis) result.push([root.corners[pair[0]], root.corners[pair[1]]])
                    }
                    return result
                }
            }
        }
    }
    Repeater {
        model: root.kind === "box" ? [
            { axis: 0, label: "X", value: root.annotation.x, color: root.theme ? root.theme.axisX : "transparent" },
            { axis: 1, label: "Y", value: root.annotation.y, color: root.theme ? root.theme.axisY : "transparent" },
            { axis: 2, label: "Z", value: root.annotation.z, color: root.theme ? root.theme.axisZ : "transparent" }
        ] : []
        delegate: Rectangle {
            id: sizeTag
            required property var modelData
            readonly property var pair: root.tagEdges[modelData.axis]
            readonly property point middle: root.corners.length === 8
                ? Qt.point((root.corners[pair[0]].x + root.corners[pair[1]].x) / 2, (root.corners[pair[0]].y + root.corners[pair[1]].y) / 2)
                : Qt.point(-10000, -10000)
            objectName: "measurementBoxSize" + modelData.label
            visible: root.projector.onScreen(middle, 0)
            x: Math.round(middle.x - width / 2)
            y: Math.round(middle.y - height / 2)
            width: sizeRow.implicitWidth + 14
            height: sizeRow.implicitHeight + 6
            radius: root.theme ? root.theme.radius2 : 0
            color: root.lineColor
            border.color: root.haloColor
            border.width: 1
            Row {
                id: sizeRow
                anchors.centerIn: parent
                spacing: 5
                // The axis in its own colour, as on the view cube and the handles.
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: axisLetter.implicitWidth + 6
                    height: axisLetter.implicitHeight
                    radius: 3
                    color: sizeTag.modelData.color
                    Label {
                        id: axisLetter
                        anchors.centerIn: parent
                        text: sizeTag.modelData.label
                        color: root.haloColor
                        font.pixelSize: root.theme ? root.theme.fontCaption : 11
                        font.weight: Font.Bold
                    }
                }
                Label {
                    text: sizeTag.modelData.value || ""
                    color: root.tagTextColor
                    font.pixelSize: root.theme ? root.theme.fontSection : 12
                    font.weight: Font.DemiBold
                    font.features: { "tnum": 1 }
                }
            }
        }
    }

    // ---- The value tag ----
    Rectangle {
        id: valueTag
        objectName: "measurementValueTag"
        readonly property bool onPoint: root.kind === "label" || root.kind === "angle"
        readonly property point anchorPoint: root.kind === "label" || root.kind === "angle" ? root.projector.project(root.annotation.at)
            : root.kind === "box" ? Qt.point(-10000, -10000)
            : Qt.point((root.fromScreen.x + root.toScreen.x) / 2, (root.fromScreen.y + root.toScreen.y) / 2)
        visible: root.kind.length > 0 && root.kind !== "box" && root.tagText.length > 0 && root.projector.onScreen(anchorPoint, 0)
        // On a line or an arc, centred on it; on a point, above it with a leader.
        x: Math.round(anchorPoint.x - width / 2)
        y: Math.round(root.kind === "label" ? anchorPoint.y - height - 10 : anchorPoint.y - height / 2)
        width: valueText.implicitWidth + 14
        height: valueText.implicitHeight + 6
        radius: root.theme ? root.theme.radius2 : 0
        color: root.lineColor
        border.color: root.haloColor
        border.width: 1

        Label {
            id: valueText
            anchors.centerIn: parent
            text: root.tagText
            color: root.tagTextColor
            font.pixelSize: root.theme ? root.theme.fontSection : 12
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
    // A tag on a point: the leader from it down to the point it is about.
    Shape {
        anchors.fill: parent
        visible: valueTag.visible && root.kind === "label"
        ShapePath {
            strokeColor: root.lineColor
            strokeWidth: 1.25
            fillColor: "transparent"
            PathMove { x: valueTag.anchorPoint.x; y: valueTag.y + valueTag.height }
            PathLine { x: valueTag.anchorPoint.x; y: valueTag.anchorPoint.y }
        }
    }
    Rectangle {
        visible: valueTag.visible && root.kind === "label"
        x: Math.round(valueTag.anchorPoint.x - 3)
        y: Math.round(valueTag.anchorPoint.y - 3)
        width: 6
        height: 6
        radius: 3
        color: root.lineColor
        border.color: root.haloColor
        border.width: 1
    }

    // ---- The picks ----
    Repeater {
        model: root.points
        delegate: Rectangle {
            required property var modelData
            required property int index
            readonly property point screenPosition: root.projector.project(modelData)
            objectName: "measurementPickMarker" + index
            readonly property real size: root.numbered ? 20 : 8
            width: size
            height: size
            radius: size / 2
            x: Math.round(screenPosition.x - width / 2)
            y: Math.round(screenPosition.y - height / 2)
            visible: root.projector.onScreen(screenPosition, width)
            color: root.lineColor
            border.color: root.haloColor
            border.width: root.numbered ? 2 : 1

            Label {
                anchors.centerIn: parent
                visible: root.numbered
                text: index + 1
                color: root.tagTextColor
                font.pixelSize: root.theme ? root.theme.fontCaption : 11
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
        }
    }
}
