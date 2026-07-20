import QtQuick
import QtQuick3D

QtObject {
    id: root

    property var cameraRig
    property var perspectiveCamera
    property var orthographicCamera
    property real viewportWidth: 1
    property real viewportHeight: 1
    property string projectionMode: "orthographic"
    property vector3d activePivot: Qt.vector3d(0, 0, 0)
    property vector3d pendingPivot: Qt.vector3d(0, 0, 0)
    property bool hasPendingPivot: false
    property quaternion orientation: Quaternion.fromEulerAngles(-35.264, 45, 0)
    property real distance: 120
    property vector3d cameraPosition: Qt.vector3d(0, 0, 120)
    property real orthographicMagnification: 5
    property real perspectiveFieldOfView: 45
    property int revision: 0
    readonly property real minimumDistance: 0.001
    readonly property real maximumDistance: 1000000
    readonly property real minimumMagnification: 0.00001
    readonly property real maximumMagnification: 100000

    function vectorLength(value) {
        return Math.sqrt(value.x * value.x + value.y * value.y + value.z * value.z)
    }

    function normalized(value, fallback) {
        const length = vectorLength(value)
        return length > 0.0000001
            ? Qt.vector3d(value.x / length, value.y / length, value.z / length)
            : fallback
    }

    function add(left, right) {
        return Qt.vector3d(left.x + right.x, left.y + right.y, left.z + right.z)
    }

    function subtract(left, right) {
        return Qt.vector3d(left.x - right.x, left.y - right.y, left.z - right.z)
    }

    function scaled(value, amount) {
        return Qt.vector3d(value.x * amount, value.y * amount, value.z * amount)
    }

    function cameraOffset() {
        return subtract(cameraPosition, activePivot)
    }

    function perspectivePixelsPerUnit(distanceValue) {
        const height = Math.max(1, viewportHeight)
        const halfAngle = perspectiveFieldOfView * Math.PI / 360.0
        return height / Math.max(0.000001, 2.0 * distanceValue * Math.tan(halfAngle))
    }

    function perspectiveDistanceForMagnification(magnification) {
        const height = Math.max(1, viewportHeight)
        const halfAngle = perspectiveFieldOfView * Math.PI / 360.0
        return height / Math.max(0.000001, 2.0 * magnification * Math.tan(halfAngle))
    }

    function apply() {
        if (!cameraRig || !perspectiveCamera || !orthographicCamera) return
        cameraRig.position = cameraPosition
        cameraRig.rotation = orientation
        perspectiveCamera.position = Qt.vector3d(0, 0, 0)
        perspectiveCamera.fieldOfView = perspectiveFieldOfView
        orthographicCamera.position = Qt.vector3d(0, 0, 0)
        orthographicCamera.horizontalMagnification = orthographicMagnification
        orthographicCamera.verticalMagnification = orthographicMagnification
        revision += 1
    }

    function reset() {
        orientation = Quaternion.fromEulerAngles(-35.264, 45, 0)
        activePivot = Qt.vector3d(0, 0, 0)
        pendingPivot = activePivot
        hasPendingPivot = false
        distance = 120
        cameraPosition = add(activePivot, orientation.times(Qt.vector3d(0, 0, distance)))
        orthographicMagnification = 5
        projectionMode = "orthographic"
        apply()
    }

    function fitBounds(minimum, maximum) {
        activePivot = Qt.vector3d((minimum.x + maximum.x) * 0.5,
                                  (minimum.y + maximum.y) * 0.5,
                                  (minimum.z + maximum.z) * 0.5)
        const extent = Math.max(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z)
        const safeExtent = Math.max(0.001, extent)
        distance = Math.max(minimumDistance, Math.min(maximumDistance, safeExtent * 1.8))
        cameraPosition = add(activePivot, orientation.times(Qt.vector3d(0, 0, distance)))
        const availablePixels = Math.max(1, Math.min(viewportWidth, viewportHeight))
        orthographicMagnification = Math.max(minimumMagnification,
                                             Math.min(maximumMagnification, availablePixels / (safeExtent * 1.35)))
        hasPendingPivot = false
        apply()
    }

    function setPendingOrbitPivot(pivot) {
        if (!pivot || !isFinite(pivot.x) || !isFinite(pivot.y) || !isFinite(pivot.z)) {
            hasPendingPivot = false
            return
        }
        pendingPivot = Qt.vector3d(pivot.x, pivot.y, pivot.z)
        hasPendingPivot = true
    }

    function clearPendingOrbitPivot() {
        hasPendingPivot = false
    }

    function activatePendingOrbitPivot() {
        if (!hasPendingPivot) return false
        activePivot = pendingPivot
        hasPendingPivot = false
        distance = Math.max(minimumDistance, vectorLength(cameraOffset()))
        return true
    }

    function orbit(deltaX, deltaY) {
        activatePendingOrbitPivot()
        if (Math.abs(deltaX) < 0.0001 && Math.abs(deltaY) < 0.0001) return

        const up = orientation.times(Qt.vector3d(0, 1, 0))
        const yaw = Quaternion.fromAxisAndAngle(up, -deltaX * 0.35)
        const yawedOrientation = yaw.times(orientation)
        const right = yawedOrientation.times(Qt.vector3d(1, 0, 0))
        const pitch = Quaternion.fromAxisAndAngle(right, -deltaY * 0.35)
        const rotation = pitch.times(yaw)
        cameraPosition = add(activePivot, rotation.times(cameraOffset()))
        orientation = rotation.times(orientation).normalized()
        distance = Math.max(minimumDistance, vectorLength(cameraOffset()))
        apply()
    }

    function pan(deltaX, deltaY) {
        const right = orientation.times(Qt.vector3d(1, 0, 0))
        const up = orientation.times(Qt.vector3d(0, 1, 0))
        const worldPerPixel = projectionMode === "orthographic"
            ? 1.0 / Math.max(minimumMagnification, orthographicMagnification)
            : 2.0 * Math.max(minimumDistance, distance)
                * Math.tan(perspectiveFieldOfView * Math.PI / 360.0) / Math.max(1, viewportHeight)
        const translation = add(scaled(right, -deltaX * worldPerPixel), scaled(up, deltaY * worldPerPixel))
        cameraPosition = add(cameraPosition, translation)
        activePivot = add(activePivot, translation)
        if (hasPendingPivot) pendingPivot = add(pendingPivot, translation)
        apply()
    }

    function alignToNormal(normal) {
        const direction = normalized(normal, Qt.vector3d(0, 0, 1))
        cameraPosition = add(activePivot, scaled(direction, distance))
        const up = Math.abs(direction.y) > 0.95 ? Qt.vector3d(0, 0, -1) : Qt.vector3d(0, 1, 0)
        orientation = Quaternion.lookAt(cameraPosition, activePivot, Qt.vector3d(0, 0, -1), up)
        hasPendingPivot = false
        apply()
    }

    function setProjection(mode) {
        const normalizedMode = mode === "perspective" ? "perspective" : "orthographic"
        if (normalizedMode === projectionMode) return
        if (normalizedMode === "orthographic") {
            orthographicMagnification = Math.max(minimumMagnification,
                                                 Math.min(maximumMagnification, perspectivePixelsPerUnit(distance)))
        } else {
            distance = Math.max(minimumDistance,
                                Math.min(maximumDistance, perspectiveDistanceForMagnification(orthographicMagnification)))
            const direction = normalized(cameraOffset(), orientation.times(Qt.vector3d(0, 0, 1)))
            cameraPosition = add(activePivot, scaled(direction, distance))
        }
        projectionMode = normalizedMode
        apply()
    }

    function zoom(delta) {
        const factor = Math.pow(0.998, delta)
        if (projectionMode === "orthographic") {
            orthographicMagnification = Math.max(minimumMagnification,
                                                 Math.min(maximumMagnification, orthographicMagnification / factor))
        } else {
            const offset = cameraOffset()
            distance = Math.max(minimumDistance, Math.min(maximumDistance, vectorLength(offset) * factor))
            cameraPosition = add(activePivot, scaled(normalized(offset, orientation.times(Qt.vector3d(0, 0, 1))), distance))
        }
        apply()
    }

    function zoomByFactor(factor) {
        if (!isFinite(factor) || factor <= 0) return
        if (projectionMode === "orthographic") {
            orthographicMagnification = Math.max(minimumMagnification,
                                                 Math.min(maximumMagnification, orthographicMagnification * factor))
        } else {
            const offset = cameraOffset()
            distance = Math.max(minimumDistance, Math.min(maximumDistance, vectorLength(offset) / factor))
            cameraPosition = add(activePivot, scaled(normalized(offset, orientation.times(Qt.vector3d(0, 0, 1))), distance))
        }
        apply()
    }
}
