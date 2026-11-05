import QtQuick
import QtQuick.Controls
import QtQuick.Shapes

// What the Measure tool draws over the model, in screen space so it reads at any zoom:
//   - under the pointer, the snap point a click would take (a vertex's square, a circle
//     centre's crosshair, an edge point's diamond, a face point's ring) and a chip naming it
//     -- "Flat face · 240.00 mm²", or why this mode cannot use it;
//   - the accepted picks as numbered markers;
//   - the result where it was measured: a dimension line with its value, a radius leader,
//     or a value tag on the edge or face.
// Hover uses the accent (the temporary role), accepted picks and results the measurement
// amber, so what is about to happen never reads as what has happened.
Item {
    id: root

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

    readonly property color hoverColor: theme ? theme.accent : "transparent"
    readonly property color pickColor: visualTheme ? visualTheme.selectionColor : "transparent"
    readonly property color pickTextColor: visualTheme ? visualTheme.sectionLabel : "transparent"
    readonly property color haloColor: visualTheme ? visualTheme.markerBorder : "transparent"

    function project(point) {
        if (!view || point === undefined || point === null || revision < 0) return Qt.point(-10000, -10000)
        return view.mapFrom3DScene(point)
    }
    function onScreen(point, margin) {
        return point.x >= -margin && point.x <= width + margin && point.y >= -margin && point.y <= height + margin
    }

    // ---- The result ----
    readonly property var annotation: measurement ? measurement.annotation : ({})
    readonly property string annotationKind: annotation && annotation.kind ? annotation.kind : ""
    readonly property point fromScreen: annotationKind === "distance" || annotationKind === "radius"
                                        ? project(annotation.from) : Qt.point(0, 0)
    readonly property point toScreen: annotationKind === "distance" || annotationKind === "radius"
                                      ? project(annotation.to) : Qt.point(0, 0)
    readonly property point atScreen: annotationKind === "label" ? project(annotation.at) : Qt.point(0, 0)
    readonly property real lineLength: Math.hypot(toScreen.x - fromScreen.x, toScreen.y - fromScreen.y)
    // Square to the line, for the end ticks.
    readonly property real tickX: lineLength > 0 ? -(toScreen.y - fromScreen.y) / lineLength * 6 : 0
    readonly property real tickY: lineLength > 0 ? (toScreen.x - fromScreen.x) / lineLength * 6 : 0

    Shape {
        id: dimension
        objectName: "measurementDimension"
        anchors.fill: parent
        visible: root.showResult && (root.annotationKind === "distance" || root.annotationKind === "radius")
                 && (root.onScreen(root.fromScreen, 0) || root.onScreen(root.toScreen, 0))
        preferredRendererType: Shape.CurveRenderer

        // A thin halo under the line keeps it legible over any part colour.
        ShapePath {
            strokeColor: Qt.rgba(root.haloColor.r, root.haloColor.g, root.haloColor.b, 0.55)
            strokeWidth: 3.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathMove { x: root.fromScreen.x; y: root.fromScreen.y }
            PathLine { x: root.toScreen.x; y: root.toScreen.y }
        }
        ShapePath {
            strokeColor: root.pickColor
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathMove { x: root.fromScreen.x; y: root.fromScreen.y }
            PathLine { x: root.toScreen.x; y: root.toScreen.y }
            // End ticks: a dimension, not a leader. A radius has its tick at the rim only.
            PathMove { x: root.toScreen.x - root.tickX; y: root.toScreen.y - root.tickY }
            PathLine { x: root.toScreen.x + root.tickX; y: root.toScreen.y + root.tickY }
            PathMove {
                x: root.annotationKind === "radius" ? root.fromScreen.x - 5 : root.fromScreen.x - root.tickX
                y: root.annotationKind === "radius" ? root.fromScreen.y : root.fromScreen.y - root.tickY
            }
            PathLine {
                x: root.annotationKind === "radius" ? root.fromScreen.x + 5 : root.fromScreen.x + root.tickX
                y: root.annotationKind === "radius" ? root.fromScreen.y : root.fromScreen.y + root.tickY
            }
            PathMove { x: root.fromScreen.x; y: root.annotationKind === "radius" ? root.fromScreen.y - 5 : root.fromScreen.y }
            PathLine { x: root.fromScreen.x; y: root.annotationKind === "radius" ? root.fromScreen.y + 5 : root.fromScreen.y }
        }
    }

    // The value, on the line's middle, or on the picked edge or face.
    Rectangle {
        id: valueTag
        objectName: "measurementValueTag"
        readonly property point anchorPoint: root.annotationKind === "label" ? root.atScreen
            : Qt.point((root.fromScreen.x + root.toScreen.x) / 2, (root.fromScreen.y + root.toScreen.y) / 2)
        visible: root.showResult && root.annotationKind.length > 0 && root.onScreen(anchorPoint, 0)
        // A tag on a point sits just above it; on a line, centred across it.
        x: Math.round(anchorPoint.x - width / 2)
        y: Math.round(root.annotationKind === "label" ? anchorPoint.y - height - 10 : anchorPoint.y - height / 2)
        width: valueText.implicitWidth + 14
        height: valueText.implicitHeight + 6
        radius: root.theme ? root.theme.radius2 : 0
        color: root.pickColor
        border.color: root.haloColor
        border.width: 1

        Label {
            id: valueText
            anchors.centerIn: parent
            text: root.annotation && root.annotation.text ? root.annotation.text : ""
            color: root.pickTextColor
            font.pixelSize: root.theme ? root.theme.fontSection : 12
            font.weight: Font.DemiBold
            font.features: { "tnum": 1 }
        }
    }
    // A tag on a point says which point.
    Rectangle {
        visible: valueTag.visible && root.annotationKind === "label"
        x: Math.round(valueTag.anchorPoint.x - 3)
        y: Math.round(valueTag.anchorPoint.y - 3)
        width: 6
        height: 6
        radius: 3
        color: root.pickColor
        border.color: root.haloColor
        border.width: 1
    }

    // ---- The accepted picks, numbered ----
    Repeater {
        model: root.measurement ? root.measurement.pickedPoints : []
        delegate: Rectangle {
            required property var modelData
            required property int index
            readonly property point screenPosition: root.project(modelData)
            objectName: "measurementPickMarker" + index
            width: 20
            height: 20
            radius: 10
            x: Math.round(screenPosition.x - width / 2)
            y: Math.round(screenPosition.y - height / 2)
            visible: root.showResult && root.onScreen(screenPosition, width)
            color: root.pickColor
            border.color: root.haloColor
            border.width: 2

            Label {
                anchors.centerIn: parent
                text: index + 1
                color: root.pickTextColor
                font.pixelSize: root.theme ? root.theme.fontCaption : 11
                font.weight: Font.Bold
                font.features: { "tnum": 1 }
            }
        }
    }

    // ---- The hover: where a click would land, and what it would take ----
    readonly property var hoverInfo: hover && hover.info ? hover.info : null
    readonly property string hoverKind: hoverInfo && hoverInfo.kind ? hoverInfo.kind : ""
    readonly property bool hoverEligible: hoverInfo !== null && hoverInfo.eligible === true
    readonly property point snapScreen: hoverInfo && hoverInfo.point !== undefined ? project(hoverInfo.point) : Qt.point(-10000, -10000)
    readonly property bool hoverIsPoint: hoverKind === "vertex" || hoverKind === "centre" || hoverKind === "edgePoint"
                                         || hoverKind === "surfacePoint"

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
