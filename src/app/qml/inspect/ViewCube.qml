import QtQuick
import QtQuick.Shapes

// Isometric view cube: three visible faces (Top / Front / Right) drawn as an
// axonometric cube, each a one-click standard view per the handoff. Pointer
// hits are resolved per-face by point-in-quad tests; each face is also a
// keyboard/accessibility stop.
Item {
    id: root

    property QtObject theme
    signal viewRequested(vector3d normal)

    readonly property color faceTop: theme && theme.dark ? Qt.lighter(baseFill, 1.45) : Qt.lighter(baseFill, 1.04)
    readonly property color faceFront: theme && theme.dark ? Qt.lighter(baseFill, 1.18) : Qt.darker(baseFill, 1.06)
    readonly property color faceRight: baseFill
    readonly property color baseFill: theme && theme.surface3 !== undefined && theme.dark ? theme.surface3
                                                                                          : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme && theme.borderStrong !== undefined ? theme.borderStrong
                                                                               : theme ? theme.border : "transparent"
    readonly property color labelColor: theme ? theme.foreground : "transparent"
    readonly property color hoverTint: theme && theme.accentTint !== undefined ? theme.accentTint : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0

    implicitWidth: 92
    implicitHeight: 96

    readonly property real cx: width / 2
    readonly property real cy: height / 2
    readonly property real r: Math.min(width, height) * 0.46
    readonly property real hx: r * 0.866
    readonly property real hy: r * 0.5
    // Hexagon corners around the cube silhouette
    readonly property point vTop: Qt.point(cx, cy - r)
    readonly property point vUpperLeft: Qt.point(cx - hx, cy - hy)
    readonly property point vUpperRight: Qt.point(cx + hx, cy - hy)
    readonly property point vLowerLeft: Qt.point(cx - hx, cy + hy)
    readonly property point vLowerRight: Qt.point(cx + hx, cy + hy)
    readonly property point vBottom: Qt.point(cx, cy + r)
    readonly property point vCenter: Qt.point(cx, cy)

    readonly property var faces: [
        { key: "top", label: qsTr("T"), name: qsTr("Top view"),
          normal: Qt.vector3d(0, 1, 0),
          quad: [vUpperLeft, vTop, vUpperRight, vCenter] },
        { key: "front", label: qsTr("F"), name: qsTr("Front view"),
          normal: Qt.vector3d(0, 0, 1),
          quad: [vUpperLeft, vCenter, vBottom, vLowerLeft] },
        { key: "right", label: qsTr("R"), name: qsTr("Right view"),
          normal: Qt.vector3d(1, 0, 0),
          quad: [vCenter, vUpperRight, vLowerRight, vBottom] }
    ]

    property int hoveredFace: -1
    property int focusedFace: -1

    function centroid(quad) {
        let x = 0
        let y = 0
        for (const p of quad) { x += p.x; y += p.y }
        return Qt.point(x / quad.length, y / quad.length)
    }

    function quadContains(quad, x, y) {
        let inside = false
        for (let i = 0, j = quad.length - 1; i < quad.length; j = i++) {
            const xi = quad[i].x, yi = quad[i].y
            const xj = quad[j].x, yj = quad[j].y
            if (((yi > y) !== (yj > y))
                    && (x < (xj - xi) * (y - yi) / (yj - yi) + xi))
                inside = !inside
        }
        return inside
    }

    function faceAt(x, y) {
        for (let i = 0; i < faces.length; ++i)
            if (quadContains(faces[i].quad, x, y)) return i
        return -1
    }

    function svgQuad(quad) {
        return "M %1 %2 L %3 %4 L %5 %6 L %7 %8 Z"
            .arg(quad[0].x).arg(quad[0].y).arg(quad[1].x).arg(quad[1].y)
            .arg(quad[2].x).arg(quad[2].y).arg(quad[3].x).arg(quad[3].y)
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.outline
            strokeWidth: 1
            fillColor: root.faceTop
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.svgQuad(root.faces[0].quad) }
        }
        ShapePath {
            strokeColor: root.outline
            strokeWidth: 1
            fillColor: root.faceFront
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.svgQuad(root.faces[1].quad) }
        }
        ShapePath {
            strokeColor: root.outline
            strokeWidth: 1
            fillColor: root.faceRight
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.svgQuad(root.faces[2].quad) }
        }
        // Hover / keyboard-focus tint over the active face
        ShapePath {
            strokeColor: "transparent"
            fillColor: root.hoverTint
            PathSvg {
                path: {
                    const active = root.hoveredFace >= 0 ? root.hoveredFace : root.focusedFace
                    return active >= 0 ? root.svgQuad(root.faces[active].quad) : ""
                }
            }
        }
    }

    Repeater {
        model: root.faces

        Text {
            required property var modelData
            readonly property point c: root.centroid(modelData.quad)
            x: c.x - width / 2
            y: c.y - height / 2
            text: modelData.label
            color: root.labelColor
            font.pixelSize: root.theme && root.theme.fontCaption !== undefined ? root.theme.fontCaption + 1 : 12
            font.weight: Font.DemiBold
        }
    }

    // Keyboard / assistive-technology stops, one per face.
    Repeater {
        model: root.faces

        Item {
            required property var modelData
            required property int index
            width: 1
            height: 1
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: modelData.name
            Accessible.onPressAction: root.viewRequested(modelData.normal)
            Keys.onReturnPressed: root.viewRequested(modelData.normal)
            Keys.onSpacePressed: root.viewRequested(modelData.normal)
            onActiveFocusChanged: root.focusedFace = activeFocus ? index : (root.focusedFace === index ? -1 : root.focusedFace)
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -(root.ringWidth + 1)
        radius: width / 2
        color: "transparent"
        border.color: root.accentFill
        border.width: root.ringWidth
        visible: root.focusedFace >= 0
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: root.hoveredFace >= 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
        onPositionChanged: function(mouse) { root.hoveredFace = root.faceAt(mouse.x, mouse.y) }
        onExited: root.hoveredFace = -1
        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                hiddenFacesMenu.popup(mouse.x, mouse.y)
                return
            }
            const face = root.faceAt(mouse.x, mouse.y)
            if (face >= 0) root.viewRequested(root.faces[face].normal)
        }
    }

    // The isometric cube only ever shows three faces at once; the opposite
    // three standard views (handoff-optional, not one-click-mandated like
    // Top/Front/Right) are reachable via right-click instead of animating
    // the cube through a rotation.
    ThemedMenu {
        id: hiddenFacesMenu
        theme: root.theme
        ThemedMenuItem { theme: hiddenFacesMenu.theme; text: qsTr("Bottom"); onTriggered: root.viewRequested(Qt.vector3d(0, -1, 0)) }
        ThemedMenuItem { theme: hiddenFacesMenu.theme; text: qsTr("Back"); onTriggered: root.viewRequested(Qt.vector3d(0, 0, -1)) }
        ThemedMenuItem { theme: hiddenFacesMenu.theme; text: qsTr("Left"); onTriggered: root.viewRequested(Qt.vector3d(-1, 0, 0)) }
    }

    ThemedToolTip {
        theme: root.theme
        text: root.hoveredFace >= 0 ? root.faces[root.hoveredFace].name : ""
        visible: root.hoveredFace >= 0
        delay: 600
        x: Math.round((root.width - width) / 2)
        y: root.height + 8
    }
}
