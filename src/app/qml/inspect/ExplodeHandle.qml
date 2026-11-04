import QtQuick
import QtQuick3D
import Loupe.App

// The exploded-view handle (#17): an arrow along each world axis to move the selection, and
// a ring about each to turn it. Drawn in its own overlay view so it is never buried in the
// model; the viewport scales `length` to keep it a constant size on screen and does the
// hit-testing in screen space (ExplodeHandleMath).
Node {
    id: root

    property real length: 1
    readonly property real ringRadius: length * 0.72
    property color xColor: "transparent"
    property color yColor: "transparent"
    property color zColor: "transparent"
    property color activeColor: "transparent"
    // The ball in the middle, which moves the selection freely in the screen's plane.
    property color centreColor: "transparent"
    // The handle under the pointer or being dragged: kind "move" or "rotate", axis 0-2.
    property string activeKind: ""
    property int activeAxis: -1

    function axisColor(axis) {
        return axis === 0 ? root.xColor : axis === 1 ? root.yColor : root.zColor
    }
    function colorFor(kind, axis) {
        return root.activeKind === kind && root.activeAxis === axis ? root.activeColor : root.axisColor(axis)
    }

    // Each arrow is built along +Y and turned onto its axis.
    Repeater3D {
        model: [
            { axis: 0, rotation: Qt.vector3d(0, 0, -90) },
            { axis: 1, rotation: Qt.vector3d(0, 0, 0) },
            { axis: 2, rotation: Qt.vector3d(90, 0, 0) }
        ]
        delegate: Node {
            required property var modelData
            eulerRotation: modelData.rotation
            Model {
                y: root.length * 0.48
                source: "#Cylinder"
                scale: Qt.vector3d(root.length * 0.0007, root.length * 0.006, root.length * 0.0007)
                pickable: false
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    baseColor: root.colorFor("move", modelData.axis)
                }
            }
            Model {
                y: root.length * 0.86
                source: "#Cone"
                scale: Qt.vector3d(root.length * 0.0024, root.length * 0.0024, root.length * 0.0024)
                pickable: false
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    baseColor: root.colorFor("move", modelData.axis)
                }
            }
        }
    }

    // The ring geometry lies in XY, about Z, and is turned to lie about its axis.
    Repeater3D {
        model: [
            { axis: 0, rotation: Qt.vector3d(0, 90, 0) },
            { axis: 1, rotation: Qt.vector3d(90, 0, 0) },
            { axis: 2, rotation: Qt.vector3d(0, 0, 0) }
        ]
        delegate: Model {
            required property var modelData
            eulerRotation: modelData.rotation
            scale: Qt.vector3d(root.ringRadius, root.ringRadius, root.ringRadius)
            geometry: HandleRingGeometry { tubeRadius: 0.022 }
            pickable: false
            materials: PrincipledMaterial {
                lighting: PrincipledMaterial.NoLighting
                baseColor: root.colorFor("rotate", modelData.axis)
                cullMode: Material.NoCulling
            }
        }
    }

    Model {
        source: "#Sphere"
        // #Sphere is 100 across: about a tenth of the arrow length, the size the screen-space
        // hit test grabs.
        scale: Qt.vector3d(root.length * 0.0019, root.length * 0.0019, root.length * 0.0019)
        pickable: false
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: root.activeKind === "free" ? root.activeColor : root.centreColor
        }
    }
}
