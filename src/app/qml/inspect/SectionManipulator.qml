import QtQuick
import QtQuick3D

Node {
    id: root

    property vector3d normal: Qt.vector3d(0, 0, 1)
    property real arrowLength: 1
    // Overridden by the caller with theme.accentVivid / a themed contrast
    // color; these are inert defaults for standalone use only.
    property color color: "transparent"
    property color outlineColor: "transparent"

    function removedSideNormal(value, flipped) {
        const direction = flipped ? 1 : -1
        return Qt.vector3d(value.x * direction, value.y * direction, value.z * direction)
    }

    function rotationForNormal(value) {
        const length = Math.sqrt(value.x * value.x + value.y * value.y + value.z * value.z)
        if (length < 0.000001)
            return Quaternion.fromEulerAngles(0, 0, 0)

        const x = value.x / length
        const y = Math.max(-1, Math.min(1, value.y / length))
        const z = value.z / length
        if (y > 0.999999)
            return Quaternion.fromEulerAngles(0, 0, 0)
        if (y < -0.999999)
            return Quaternion.fromAxisAndAngle(Qt.vector3d(1, 0, 0), 180)

        const axisLength = Math.sqrt(x * x + z * z)
        const axis = Qt.vector3d(z / axisLength, 0, -x / axisLength)
        return Quaternion.fromAxisAndAngle(axis, Math.acos(y) * 180 / Math.PI)
    }

    rotation: rotationForNormal(normal)

    Model {
        source: "#Sphere"
        scale: Qt.vector3d(root.arrowLength * 0.0017,
                           root.arrowLength * 0.0017,
                           root.arrowLength * 0.0017)
        pickable: false
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: root.outlineColor
        }
    }

    Model {
        y: root.arrowLength * 0.36
        source: "#Cylinder"
        scale: Qt.vector3d(root.arrowLength * 0.00115,
                           root.arrowLength * 0.0072,
                           root.arrowLength * 0.00115)
        pickable: false
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: root.color
        }
    }

    Model {
        y: root.arrowLength * 0.83
        source: "#Cone"
        scale: Qt.vector3d(root.arrowLength * 0.0032,
                           root.arrowLength * 0.0032,
                           root.arrowLength * 0.0032)
        pickable: false
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: root.color
        }
    }
}
