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

    // Saved camera views (Views tool window): the whole camera as plain numbers, which the
    // app keeps with the file.
    function cameraState() {
        return {
            position: [cameraPosition.x, cameraPosition.y, cameraPosition.z],
            orientation: [orientation.scalar, orientation.x, orientation.y, orientation.z],
            pivot: [activePivot.x, activePivot.y, activePivot.z],
            distance: distance,
            magnification: orthographicMagnification,
            fieldOfView: perspectiveFieldOfView,
            projection: projectionMode
        }
    }

    // Back to a saved camera, gliding there over `duration` ms (0: at once). Any other
    // camera move -- a drag, a zoom, a fit -- takes over from wherever the glide has got to.
    property real tweenProgress: 0
    property var tweenFrom: null
    property var tweenTo: null
    property NumberAnimation tween: NumberAnimation {
        target: root
        property: "tweenProgress"
        from: 0
        to: 1
        easing.type: Easing.BezierSpline
        easing.bezierCurve: [0.20, 0.0, 0.0, 1.0, 1.0, 1.0]
    }
    onTweenProgressChanged: if (tweenFrom && tweenTo) blendCamera(tweenProgress)

    function validCameraState(state) {
        const triple = function(value) { return value && value.length === 3 && value.every(isFinite) }
        return state && triple(state.position) && triple(state.pivot) && state.orientation
            && state.orientation.length === 4 && state.orientation.every(isFinite)
            && isFinite(state.distance) && isFinite(state.magnification) && state.magnification > 0
    }

    function restoreCameraState(state, duration) {
        if (!validCameraState(state)) return false
        stopTween()
        // The projection switches at once; position and zoom glide.
        if (state.projection === "perspective" || state.projection === "orthographic") projectionMode = state.projection
        tweenFrom = cameraState()
        tweenTo = state
        if (!(duration > 0)) {
            blendCamera(1)
            return true
        }
        tween.duration = duration
        tweenProgress = 0
        tween.restart()
        return true
    }

    function stopTween() {
        if (tween.running) tween.stop()
        tweenFrom = null
        tweenTo = null
    }

    function slerp(a, b, t) {
        let bw = b[0], bx = b[1], by = b[2], bz = b[3]
        let dot = a[0] * bw + a[1] * bx + a[2] * by + a[3] * bz
        if (dot < 0) { bw = -bw; bx = -bx; by = -by; bz = -bz; dot = -dot }
        let wa = 1 - t, wb = t
        if (dot < 0.9995) {
            const theta = Math.acos(Math.min(1, dot))
            const sine = Math.sin(theta)
            wa = Math.sin((1 - t) * theta) / sine
            wb = Math.sin(t * theta) / sine
        }
        return Qt.quaternion(wa * a[0] + wb * bw, wa * a[1] + wb * bx, wa * a[2] + wb * by, wa * a[3] + wb * bz).normalized()
    }

    function blendCamera(t) {
        const from = tweenFrom
        const to = tweenTo
        if (!from || !to) return
        const mix = function(a, b) { return a + (b - a) * t }
        const mix3 = function(a, b) { return Qt.vector3d(mix(a[0], b[0]), mix(a[1], b[1]), mix(a[2], b[2])) }
        cameraPosition = mix3(from.position, to.position)
        activePivot = mix3(from.pivot, to.pivot)
        pendingPivot = activePivot
        hasPendingPivot = false
        orientation = slerp(from.orientation, to.orientation, t)
        distance = mix(from.distance, to.distance)
        // Zoom glides evenly in scale, not in raw magnification.
        orthographicMagnification = Math.exp(mix(Math.log(from.magnification), Math.log(to.magnification)))
        if (isFinite(to.fieldOfView)) perspectiveFieldOfView = mix(from.fieldOfView, to.fieldOfView)
        apply()
        if (t >= 1) {
            tweenFrom = null
            tweenTo = null
        }
    }

    function apply() {
        if (!cameraRig || !perspectiveCamera || !orthographicCamera) return
        cameraRig.position = cameraPosition
        cameraRig.rotation = orientation
        perspectiveCamera.position = Qt.vector3d(0, 0, 0)
        perspectiveCamera.fieldOfView = perspectiveFieldOfView
        // Its distance behind the rig is the viewport's (orthographicBackoff), not set here.
        orthographicCamera.horizontalMagnification = orthographicMagnification
        orthographicCamera.verticalMagnification = orthographicMagnification
        revision += 1
    }

    function reset() {
        stopTween()
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
        stopTween()
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

    // Re-centre on some bounds without changing how close the camera is.
    //
    // Standard views need this: alignToNormal orbits about activePivot, so if the pivot is
    // still at the origin (or wherever the last orbit left it) while the part sits elsewhere,
    // the camera ends up aimed at empty space and the part slides off to one side. Splitting
    // this out of fitBounds is what lets a named view re-frame without also resetting the
    // zoom, which is the whole reason a cube click stopped preserving magnification.
    function centreOn(minimum, maximum) {
        stopTween()
        activePivot = Qt.vector3d((minimum.x + maximum.x) * 0.5,
                                  (minimum.y + maximum.y) * 0.5,
                                  (minimum.z + maximum.z) * 0.5)
        cameraPosition = add(activePivot, orientation.times(Qt.vector3d(0, 0, distance)))
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
        stopTween()
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

    // Spin the view about the axis it already looks along, for a two-finger
    // twist. Rotating the camera offset about its own direction is the identity,
    // so the camera stays put and only its up vector turns -- which is what
    // makes this feel like turning the drawing rather than orbiting the part.
    // A standard-view click re-derives up from world axes and so clears any roll.
    function roll(degrees) {
        stopTween()
        if (!isFinite(degrees) || Math.abs(degrees) < 0.0001) return
        const forward = orientation.times(Qt.vector3d(0, 0, 1))
        orientation = Quaternion.fromAxisAndAngle(forward, degrees).times(orientation).normalized()
        apply()
    }

    function pan(deltaX, deltaY) {
        stopTween()
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
        stopTween()
        const direction = normalized(normal, Qt.vector3d(0, 0, 1))
        cameraPosition = add(activePivot, scaled(direction, distance))
        const up = Math.abs(direction.y) > 0.95 ? Qt.vector3d(0, 0, -1) : Qt.vector3d(0, 1, 0)
        orientation = Quaternion.lookAt(cameraPosition, activePivot, Qt.vector3d(0, 0, -1), up)
        hasPendingPivot = false
        apply()
    }

    // Look straight along -direction at target, the camera on the direction side, keeping
    // the zoom. For "View normal to face": target is the face's centre, so the face lands in
    // the middle of the view rather than wherever the old pivot left it.
    //
    // Up starts as alignToNormal's world-derived up. Given a straight edge of the face, it
    // then turns to the nearest of that edge's four in-plane directions, so a rectangular
    // face reads square on screen instead of at whatever angle world up happens to give it.
    function viewAlong(direction, target, edgeDirection) {
        stopTween()
        const back = normalized(direction, Qt.vector3d(0, 0, 1))
        let up = Math.abs(back.y) > 0.95 ? Qt.vector3d(0, 0, -1) : Qt.vector3d(0, 1, 0)
        if (edgeDirection) {
            const along = edgeDirection.x * back.x + edgeDirection.y * back.y + edgeDirection.z * back.z
            const edge = normalized(subtract(edgeDirection, scaled(back, along)), Qt.vector3d(0, 0, 0))
            if (vectorLength(edge) > 0.5) {
                const across = back.crossProduct(edge)
                let best = up
                let bestAlignment = -2
                for (const candidate of [edge, scaled(edge, -1), across, scaled(across, -1)]) {
                    const alignment = candidate.x * up.x + candidate.y * up.y + candidate.z * up.z
                    if (alignment > bestAlignment) {
                        bestAlignment = alignment
                        best = candidate
                    }
                }
                up = best
            }
        }
        // Quaternion.lookAt cannot be used here: it only uses its up argument as a fallback
        // axis, so the roll comes out as whatever the shortest turn from -Z gives.
        const trueUp = normalized(subtract(up, scaled(back, up.x * back.x + up.y * back.y + up.z * back.z)),
                                  Math.abs(back.y) > 0.95 ? Qt.vector3d(0, 0, -1) : Qt.vector3d(0, 1, 0))
        activePivot = Qt.vector3d(target.x, target.y, target.z)
        hasPendingPivot = false
        cameraPosition = add(activePivot, scaled(back, distance))
        orientation = quaternionFromAxes(trueUp.crossProduct(back), trueUp, back)
        apply()
    }

    // The rotation taking the camera's x, y and z axes to right, up and back (orthonormal).
    function quaternionFromAxes(right, up, back) {
        const m00 = right.x, m01 = up.x, m02 = back.x
        const m10 = right.y, m11 = up.y, m12 = back.y
        const m20 = right.z, m21 = up.z, m22 = back.z
        const trace = m00 + m11 + m22
        let w, x, y, z
        if (trace > 0) {
            const s = 0.5 / Math.sqrt(trace + 1)
            w = 0.25 / s; x = (m21 - m12) * s; y = (m02 - m20) * s; z = (m10 - m01) * s
        } else if (m00 > m11 && m00 > m22) {
            const s = 2 * Math.sqrt(1 + m00 - m11 - m22)
            w = (m21 - m12) / s; x = 0.25 * s; y = (m01 + m10) / s; z = (m02 + m20) / s
        } else if (m11 > m22) {
            const s = 2 * Math.sqrt(1 + m11 - m00 - m22)
            w = (m02 - m20) / s; x = (m01 + m10) / s; y = 0.25 * s; z = (m12 + m21) / s
        } else {
            const s = 2 * Math.sqrt(1 + m22 - m00 - m11)
            w = (m10 - m01) / s; x = (m02 + m20) / s; y = (m12 + m21) / s; z = 0.25 * s
        }
        return Qt.quaternion(w, x, y, z).normalized()
    }

    function setProjection(mode) {
        stopTween()
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

    // A wheel step, as a zoom factor: positive deltas move closer.
    function zoom(delta, screenX, screenY) {
        stopTween()
        zoomByFactor(1.0 / Math.pow(0.998, delta), screenX, screenY)
    }

    // Zoom by `factor` (above 1 is closer), keeping the scene under (screenX, screenY) fixed on
    // screen. Without a screen point it anchors on the viewport centre.
    //
    // Before this, zoom scaled about the screen centre in orthographic, and in perspective it
    // moved the camera toward activePivot -- which, after orbiting about a picked point, is off
    // the view axis, so the camera slid sideways. Either way the part under the user's fingers
    // moved away during a pinch, which reads as the view shifting on its own. Anchoring on the
    // pointer is how every other macOS pinch behaves.
    function zoomByFactor(factor, screenX, screenY) {
        stopTween()
        if (!isFinite(factor) || factor <= 0) return
        const anchorX = (screenX === undefined || !isFinite(screenX)) ? 0 : screenX - viewportWidth * 0.5
        const anchorY = (screenY === undefined || !isFinite(screenY)) ? 0 : screenY - viewportHeight * 0.5
        const back = orientation.times(Qt.vector3d(0, 0, 1))
        let worldPerPixelBefore = 0
        let worldPerPixelAfter = 0
        if (projectionMode === "orthographic") {
            worldPerPixelBefore = 1.0 / Math.max(minimumMagnification, orthographicMagnification)
            orthographicMagnification = Math.max(minimumMagnification,
                                                 Math.min(maximumMagnification, orthographicMagnification * factor))
            worldPerPixelAfter = 1.0 / Math.max(minimumMagnification, orthographicMagnification)
        } else {
            // Dolly along the view axis, never toward an off-axis pivot.
            const pixelScale = 2.0 * Math.tan(perspectiveFieldOfView * Math.PI / 360.0) / Math.max(1, viewportHeight)
            const offset = cameraOffset()
            const depthBefore = Math.max(minimumDistance, offset.x * back.x + offset.y * back.y + offset.z * back.z)
            const depthAfter = Math.max(minimumDistance, Math.min(maximumDistance, depthBefore / factor))
            cameraPosition = add(cameraPosition, scaled(back, depthAfter - depthBefore))
            worldPerPixelBefore = depthBefore * pixelScale
            worldPerPixelAfter = depthAfter * pixelScale
        }
        // What sat under the anchor was anchor * worldPerPixelBefore from the axis and must stay
        // there: slide camera and pivot together, exactly as a pan would.
        const right = orientation.times(Qt.vector3d(1, 0, 0))
        const up = orientation.times(Qt.vector3d(0, 1, 0))
        const shift = worldPerPixelBefore - worldPerPixelAfter
        const translation = add(scaled(right, anchorX * shift), scaled(up, -anchorY * shift))
        cameraPosition = add(cameraPosition, translation)
        activePivot = add(activePivot, translation)
        if (hasPendingPivot) pendingPivot = add(pendingPivot, translation)
        distance = Math.max(minimumDistance, vectorLength(cameraOffset()))
        apply()
    }
}
