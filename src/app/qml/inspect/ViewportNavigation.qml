import QtQuick
import QtQuick3D

QtObject {
    id: root

    property var cameraRig
    property var camera
    property vector3d target: Qt.vector3d(0, 0, 0)
    property quaternion orientation: Quaternion.fromEulerAngles(-22, 32, 0)
    property real distance: 120
    readonly property real minimumDistance: 5
    readonly property real maximumDistance: 1000000

    function apply() {
        if (!cameraRig || !camera) return
        cameraRig.position = target
        cameraRig.rotation = orientation
        camera.position = Qt.vector3d(0, 0, distance)
    }

    function reset() {
        target = Qt.vector3d(0, 0, 0)
        orientation = Quaternion.fromEulerAngles(-22, 32, 0)
        distance = 120
        apply()
    }

    function fitBounds(minimum, maximum) {
        target = Qt.vector3d((minimum.x + maximum.x) * 0.5,
                             (minimum.y + maximum.y) * 0.5,
                             (minimum.z + maximum.z) * 0.5)
        const extent = Math.max(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z)
        distance = Math.max(minimumDistance, Math.min(maximumDistance, extent * 1.8))
        apply()
    }

    function setOrbitPivot(pivot) {
        if (!cameraRig || !camera || !pivot) return

        // Keep the camera in place while changing the point it orbits. This
        // makes a drag over a distant component feel anchored to that part.
        const currentOffset = orientation.times(Qt.vector3d(0, 0, distance))
        const cameraPosition = Qt.vector3d(target.x + currentOffset.x,
                                           target.y + currentOffset.y,
                                           target.z + currentOffset.z)
        const pivotOffset = Qt.vector3d(cameraPosition.x - pivot.x,
                                        cameraPosition.y - pivot.y,
                                        cameraPosition.z - pivot.z)
        const pivotDistance = Math.sqrt(pivotOffset.x * pivotOffset.x
                                        + pivotOffset.y * pivotOffset.y
                                        + pivotOffset.z * pivotOffset.z)
        if (!isFinite(pivotDistance) || pivotDistance < minimumDistance) return

        const currentUp = orientation.times(Qt.vector3d(0, 1, 0))
        target = Qt.vector3d(pivot.x, pivot.y, pivot.z)
        distance = Math.min(maximumDistance, pivotDistance)
        orientation = Quaternion.lookAt(cameraPosition, target,
                                         Qt.vector3d(0, 0, -1), currentUp)
        apply()
    }

    function orbit(deltaX, deltaY) {
        // Creo-style direct manipulation: the model follows the drag direction.
        const yawRotation = Quaternion.fromAxisAndAngle(Qt.vector3d(0, 1, 0), -deltaX * 0.35)
        const right = orientation.times(Qt.vector3d(1, 0, 0))
        const pitchRotation = Quaternion.fromAxisAndAngle(right, -deltaY * 0.35)
        orientation = pitchRotation.times(yawRotation).times(orientation).normalized()
        apply()
    }

    function pan(deltaX, deltaY) {
        const right = orientation.times(Qt.vector3d(1, 0, 0))
        const up = orientation.times(Qt.vector3d(0, 1, 0))
        const scale = Math.max(0.001, distance * 0.0015)
        target = Qt.vector3d(target.x - right.x * deltaX * scale + up.x * deltaY * scale,
                             target.y - right.y * deltaX * scale + up.y * deltaY * scale,
                             target.z - right.z * deltaX * scale + up.z * deltaY * scale)
        apply()
    }

    function alignToNormal(normal) {
        const magnitude = Math.sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z)
        if (magnitude < 0.000001) return
        const direction = Qt.vector3d(normal.x / magnitude, normal.y / magnitude, normal.z / magnitude)
        orientation = Quaternion.lookAt(direction, Qt.vector3d(0, 0, 0))
        apply()
    }

    function zoom(delta) {
        const factor = Math.pow(0.998, delta)
        distance = Math.max(minimumDistance, Math.min(maximumDistance, distance * factor))
        apply()
    }

    function zoomByFactor(factor) {
        if (!isFinite(factor) || factor <= 0) return
        distance = Math.max(minimumDistance, Math.min(maximumDistance, distance / factor))
        apply()
    }
}
