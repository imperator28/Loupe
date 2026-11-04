import QtQuick

// The screen-space arithmetic behind the exploded-view handles (#17), kept free of the
// scene so it can be tested on its own. Every point is in viewport pixels.
QtObject {
    id: math

    // How close, in pixels, the pointer must be to grab an arrow or a ring.
    readonly property real arrowTolerance: 9
    readonly property real ringTolerance: 7
    // An arrow seen almost end-on is too short on screen to drag along meaningfully.
    readonly property real minimumArrowPixels: 14
    // The ball at the middle of the handle moves the selection freely, in the plane of the
    // screen. It wins over the edge-on rings that run through it.
    readonly property real centreRadius: 11

    function distanceToSegment(px, py, ax, ay, bx, by) {
        const dx = bx - ax
        const dy = by - ay
        const lengthSquared = dx * dx + dy * dy
        let t = lengthSquared > 0 ? ((px - ax) * dx + (py - ay) * dy) / lengthSquared : 0
        t = Math.max(0, Math.min(1, t))
        const x = ax + t * dx - px
        const y = ay + t * dy - py
        return Math.sqrt(x * x + y * y)
    }

    // Distance to a closed loop through `points` ([{x, y}, ...]).
    function distanceToLoop(px, py, points) {
        let best = Number.POSITIVE_INFINITY
        for (let index = 0; index < points.length; ++index) {
            const a = points[index]
            const b = points[(index + 1) % points.length]
            best = Math.min(best, distanceToSegment(px, py, a.x, a.y, b.x, b.y))
        }
        return best
    }

    // Which handle a press at (x, y) takes: {kind: "free"} for the ball in the middle,
    // {kind: "move" | "rotate", axis: 0 | 1 | 2} for an arrow or a ring, or null. `layout`
    // is {centre: {x, y}, tips: [3 x {x, y}], rings: [3 x [{x, y}, ...]]}. Arrows win where
    // they cross a ring: they are the smaller target.
    function handleAt(x, y, layout) {
        if (!layout) return null
        const fromCentreX = x - layout.centre.x
        const fromCentreY = y - layout.centre.y
        if (Math.sqrt(fromCentreX * fromCentreX + fromCentreY * fromCentreY) <= math.centreRadius)
            return { kind: "free", axis: -1 }
        let best = null
        let bestDistance = Number.POSITIVE_INFINITY
        for (let axis = 0; axis < 3; ++axis) {
            const tip = layout.tips[axis]
            const dx = tip.x - layout.centre.x
            const dy = tip.y - layout.centre.y
            if (Math.sqrt(dx * dx + dy * dy) < math.minimumArrowPixels) continue
            // From a little out of the centre, so a click on the middle is not an arrow.
            const start = { x: layout.centre.x + dx * 0.15, y: layout.centre.y + dy * 0.15 }
            const distance = distanceToSegment(x, y, start.x, start.y, tip.x, tip.y)
            if (distance <= math.arrowTolerance && distance < bestDistance) {
                best = { kind: "move", axis: axis }
                bestDistance = distance
            }
        }
        if (best) return best
        for (let axis = 0; axis < 3; ++axis) {
            const distance = distanceToLoop(x, y, layout.rings[axis])
            if (distance <= math.ringTolerance && distance < bestDistance) {
                best = { kind: "rotate", axis: axis }
                bestDistance = distance
            }
        }
        return best
    }

    // Millimetres along an axis for a pointer moved by (dx, dy): the motion projected onto
    // the axis' on-screen direction (axisX, axisY: the pixels `millimetres` of it covers).
    function axisTravel(dx, dy, axisX, axisY, millimetres) {
        const lengthSquared = axisX * axisX + axisY * axisY
        if (lengthSquared <= 0) return 0
        return (dx * axisX + dy * axisY) / lengthSquared * millimetres
    }

    // A pointer moved by (dx, dy) pixels, as a move in the screen's plane: `right` and `up`
    // are the screen's directions in the scene, `worldPerPixel` its scale at the handle.
    function screenPlaneTravel(dx, dy, right, up, worldPerPixel) {
        return Qt.vector3d((right.x * dx - up.x * dy) * worldPerPixel,
                           (right.y * dx - up.y * dy) * worldPerPixel,
                           (right.z * dx - up.z * dy) * worldPerPixel)
    }

    // The signed turn from one screen angle to the next, in degrees, across the +/-180 seam.
    function angleStep(fromDegrees, toDegrees) {
        let step = toDegrees - fromDegrees
        while (step > 180) step -= 360
        while (step <= -180) step += 360
        return step
    }

    function screenAngle(x, y, centre) {
        return Math.atan2(y - centre.y, x - centre.x) * 180 / Math.PI
    }
}
