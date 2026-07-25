import QtQuick
import QtQuick3D

// A solid 3D view cube that turns with the part, in the manner of a mainstream
// CAD orientation cube: visible edges, labelled faces, a corner axis triad, and
// draggable to orbit.
//
// Face placement is derived from the SAME function that resolves a click, which is
// what keeps geometry and action in agreement. An earlier revision placed faces on
// hard-coded local axes while resolving clicks through the controller, so on a
// Z-up document -- where "Top" is +Z, not +Y -- clicking Top produced the front
// view. Deriving both from one source makes that class of mismatch impossible.
Item {
    id: root

    property QtObject theme
    property QtObject controller
    property quaternion cameraOrientation: Qt.quaternion(1, 0, 0, 0)

    signal viewRequested(vector3d normal)
    // Dragging across the cube orbits the part, as it does in CAD.
    signal orbitRequested(real deltaX, real deltaY)
    // Clicking the face you are already square-on to spins the drawing in its own
    // plane, which is how a CAD cube lets you reorient a view without leaving it.
    signal planarRotateRequested(real degrees)

    implicitWidth: 96
    implicitHeight: 96

    readonly property color faceFill: theme ? theme.surface3 : "transparent"
    readonly property color edgeColor: theme ? theme.borderStrong : "transparent"
    readonly property color labelColor: theme ? theme.textPrimary : "transparent"
    readonly property color ringColor: theme ? theme.accentColor : "transparent"
    readonly property int ringWidth: theme ? theme.focusRingWidth : 2

    readonly property real halfSize: 1.0
    // Pixels per world unit for the cube's orthographic camera. Shared with the
    // 2D overlays so they can be placed analytically instead of via
    // mapFrom3DScene, which collapsed every label onto one point.
    readonly property real pixelsPerUnit: Math.min(width, height) / 3.6
    // #Cube and #Rectangle are 100 units across, so this scales them to 2 units.
    readonly property real unitScale: 0.02

    readonly property var viewNames: ["Front", "Back", "Right", "Left", "Top", "Bottom"]

    function directionFor(name) {
        if (root.controller) {
            const resolved = root.controller.directionForStandardView(name)
            if (resolved && (resolved.x !== 0 || resolved.y !== 0 || resolved.z !== 0)) return resolved
        }
        // Only reached before a controller is attached.
        switch (name) {
        case "Front": return Qt.vector3d(0, 0, 1)
        case "Back": return Qt.vector3d(0, 0, -1)
        case "Right": return Qt.vector3d(1, 0, 0)
        case "Left": return Qt.vector3d(-1, 0, 0)
        case "Top": return Qt.vector3d(0, 1, 0)
        default: return Qt.vector3d(0, -1, 0)
        }
    }

    // Which named view a cube-local direction corresponds to. Resolved by best
    // match against the same directions the faces are placed on, so it is exactly
    // the inverse of directionFor and cannot drift from it.
    function viewNameForLocalDirection(direction) {
        let bestName = ""
        let bestDot = 0.5
        for (const name of root.viewNames) {
            const candidate = root.directionFor(name)
            const dot = candidate.x * direction.x + candidate.y * direction.y + candidate.z * direction.z
            if (dot > bestDot) {
                bestDot = dot
                bestName = name
            }
        }
        return bestName
    }

    // True when the camera already looks straight down this face's normal, i.e.
    // the cube is showing it as a flat square.
    function isSquareOn(name) {
        const rotated = root.cameraOrientation.inverted().times(root.directionFor(name))
        return rotated.z > 0.999
    }

    function requestView(name) {
        if (name.length === 0) return
        // Re-aligning to the view you are already in would do nothing visible, so
        // that click is more usefully a quarter turn within the plane.
        if (root.isSquareOn(name)) {
            root.planarRotateRequested(90)
            return
        }
        root.viewRequested(root.directionFor(name))
    }

    property string hoveredView: ""
    property int focusedFace: -1

    // The twelve cube edges, as thin bars. Qt Quick 3D has no wireframe mode, and
    // an unoutlined cube reads as three flat blocks of colour rather than a solid.
    readonly property real edgeThickness: 0.00025
    readonly property var edgeBars: [
        { p: Qt.vector3d(0, 1, 1), axis: "x" }, { p: Qt.vector3d(0, 1, -1), axis: "x" },
        { p: Qt.vector3d(0, -1, 1), axis: "x" }, { p: Qt.vector3d(0, -1, -1), axis: "x" },
        { p: Qt.vector3d(1, 0, 1), axis: "y" }, { p: Qt.vector3d(1, 0, -1), axis: "y" },
        { p: Qt.vector3d(-1, 0, 1), axis: "y" }, { p: Qt.vector3d(-1, 0, -1), axis: "y" },
        { p: Qt.vector3d(1, 1, 0), axis: "z" }, { p: Qt.vector3d(1, -1, 0), axis: "z" },
        { p: Qt.vector3d(-1, 1, 0), axis: "z" }, { p: Qt.vector3d(-1, -1, 0), axis: "z" }
    ]

    function barScale(axis) {
        const long = root.unitScale
        const thin = root.edgeThickness
        if (axis === "x") return Qt.vector3d(long, thin, thin)
        if (axis === "y") return Qt.vector3d(thin, long, thin)
        return Qt.vector3d(thin, thin, long)
    }

    View3D {
        id: cubeView
        anchors.fill: parent
        environment: SceneEnvironment {
            clearColor: "transparent"
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
        }

        OrthographicCamera {
            z: 20
            // One magnification for both axes. Deriving them separately from width
            // and height distorts the cube the moment the item is not square, which
            // is what made an earlier revision look squeezed.
            horizontalMagnification: root.pixelsPerUnit
            verticalMagnification: root.pixelsPerUnit
        }

        // No lights: the body is flat-shaded below. Directional lighting left the
        // face turned away from both lights rendering solid black, and a shaded,
        // shadowed widget does not match the flat design language.
        Node {
            id: cubeRoot
            // Inverting the camera orientation makes the cube turn as the part
            // appears to turn.
            rotation: root.cameraOrientation.inverted()

            // One solid body. Faces are identified on pick from the hit normal
            // rather than by being separate models, which keeps the cube a single
            // watertight object.
            Model {
                source: "#Cube"
                scale: Qt.vector3d(root.unitScale, root.unitScale, root.unitScale)
                pickable: true
                objectName: "viewCubeBody"
                materials: PrincipledMaterial {
                    baseColor: root.faceFill
                    lighting: PrincipledMaterial.NoLighting
                }
            }

            Repeater3D {
                model: root.edgeBars
                Model {
                    required property var modelData
                    source: "#Cube"
                    position: Qt.vector3d(modelData.p.x * root.halfSize,
                                          modelData.p.y * root.halfSize,
                                          modelData.p.z * root.halfSize)
                    scale: root.barScale(modelData.axis)
                    materials: PrincipledMaterial {
                        baseColor: root.edgeColor
                        lighting: PrincipledMaterial.NoLighting
                    }
                }
            }
        }
    }

    // Face labels, projected from the cube's own face centres so they stay upright
    // and legible however the cube is turned.
    Repeater {
        model: root.viewNames

        Text {
            required property var modelData
            readonly property vector3d direction: root.directionFor(modelData)
            // Rotate the face direction the same way the cube node is rotated, then
            // project it by hand. The camera is orthographic and looks down -Z, so
            // screen x is scene x and screen y is inverted scene y -- exact, and
            // free of mapFrom3DScene's missing camera dependency.
            readonly property vector3d rotated: root.cameraOrientation.inverted().times(direction)
            readonly property real facing: rotated.z

            visible: facing > 0.25
            x: root.width / 2 + rotated.x * root.halfSize * root.pixelsPerUnit - width / 2
            y: root.height / 2 - rotated.y * root.halfSize * root.pixelsPerUnit - height / 2
            text: modelData
            color: root.labelColor
            font.pixelSize: root.theme ? root.theme.fontCaption - 1 : 10
            font.weight: root.hoveredView === modelData ? Font.Bold : Font.DemiBold
        }
    }

    // Axis triad. The cube alone says which face is which; the triad says which way
    // the model's own axes run, which is what makes an unfamiliar orientation
    // readable at a glance.
    Repeater {
        model: [
            { label: "X", direction: Qt.vector3d(1, 0, 0) },
            { label: "Y", direction: Qt.vector3d(0, 1, 0) },
            { label: "Z", direction: Qt.vector3d(0, 0, 1) }
        ]

        Text {
            required property var modelData
            readonly property vector3d rotated: root.cameraOrientation.inverted().times(modelData.direction)
            readonly property real facing: rotated.z
            readonly property real reach: root.halfSize * 1.6

            visible: facing > -0.4
            x: root.width / 2 + rotated.x * reach * root.pixelsPerUnit - width / 2
            y: root.height / 2 - rotated.y * reach * root.pixelsPerUnit - height / 2
            text: modelData.label
            color: modelData.label === "X" ? (root.theme ? root.theme.errorColor : "transparent")
                 : modelData.label === "Y" ? (root.theme ? root.theme.success : "transparent")
                 : (root.theme ? root.theme.accentColor : "transparent")
            font.pixelSize: root.theme ? root.theme.fontCaption - 2 : 9
            font.weight: Font.Bold
            opacity: facing > 0.1 ? 1.0 : 0.45
        }
    }

    Repeater {
        model: root.viewNames

        Item {
            required property var modelData
            required property int index
            width: 1
            height: 1
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("%1 view").arg(modelData)
            Accessible.onPressAction: root.requestView(modelData)
            Keys.onReturnPressed: root.requestView(modelData)
            Keys.onSpacePressed: root.requestView(modelData)
            onActiveFocusChanged: root.focusedFace = activeFocus
                ? index : (root.focusedFace === index ? -1 : root.focusedFace)
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: -(root.ringWidth + 1)
        radius: width / 2
        color: "transparent"
        border.color: root.ringColor
        border.width: root.ringWidth
        visible: root.focusedFace >= 0
    }

    MouseArea {
        id: cubeInput
        anchors.fill: parent
        hoverEnabled: true
        // Right-click reaches any of the six views regardless of which faces happen
        // to be turned towards the viewer.
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: root.hoveredView.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor

        property real pressX: 0
        property real pressY: 0
        property real lastX: 0
        property real lastY: 0
        property bool dragging: false

        // Resolve the picked face from the hit normal, converted out of scene space
        // into the cube's own frame and matched against the named directions.
        function viewAt(x, y) {
            const hit = cubeView.pick(x, y)
            if (!hit.objectHit) return ""
            const local = root.cameraOrientation.times(hit.sceneNormal)
            return root.viewNameForLocalDirection(local)
        }

        onPressed: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                faceMenu.popup(mouse.x, mouse.y)
                return
            }
            pressX = lastX = mouse.x
            pressY = lastY = mouse.y
            dragging = false
        }
        onPositionChanged: function(mouse) {
            if (!pressedButtons) {
                root.hoveredView = viewAt(mouse.x, mouse.y)
                return
            }
            if (!dragging && (Math.abs(mouse.x - pressX) > 3 || Math.abs(mouse.y - pressY) > 3)) {
                dragging = true
            }
            if (!dragging) return
            root.orbitRequested(mouse.x - lastX, mouse.y - lastY)
            lastX = mouse.x
            lastY = mouse.y
        }
        onReleased: function(mouse) {
            if (mouse.button === Qt.RightButton) return
            // A drag orbits; only a tap selects a view.
            if (!dragging) root.requestView(viewAt(mouse.x, mouse.y))
            dragging = false
        }
        onExited: root.hoveredView = ""
    }

    ThemedMenu {
        id: faceMenu
        theme: root.theme
        Repeater {
            model: root.viewNames
            ThemedMenuItem {
                required property var modelData
                theme: faceMenu.theme
                text: qsTr("%1 view").arg(modelData)
                onTriggered: root.viewRequested(root.directionFor(modelData))
            }
        }
        ThemedMenuSeparator { theme: faceMenu.theme }
        ThemedMenuItem {
            theme: faceMenu.theme
            text: qsTr("Rotate 90° in plane")
            onTriggered: root.planarRotateRequested(90)
        }
    }

    ThemedToolTip {
        theme: root.theme
        text: root.hoveredView.length > 0 ? qsTr("%1 view").arg(root.hoveredView) : ""
        visible: root.hoveredView.length > 0 && !cubeInput.dragging
        delay: 600
        x: Math.round((root.width - width) / 2)
        y: root.height + 8
    }
}
