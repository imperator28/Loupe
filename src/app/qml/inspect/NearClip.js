.pragma library

// Clipping scene geometry against the camera's near plane before it is projected to the
// screen. A point behind a perspective camera projects to a mirrored, far-off position, so a
// line from in front of the camera to behind it would be drawn shooting across the view in
// the wrong direction. Everything here keeps only what lies in front of the plane, cutting a
// line or polygon where it crosses it.
//
// depthOf(point) is the point's distance in front of the camera, along its view direction;
// near is the depth below which nothing is kept. Points are anything with x, y and z.

function lerp(a, b, t) {
    return Qt.vector3d(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t)
}

// Where the segment a-b crosses depth `near`, given both ends' depths.
function crossing(a, b, da, db, near) {
    return lerp(a, b, (near - da) / (db - da))
}

// The part of a-b in front of the plane, as [from, to], or null when it is all behind.
function clipSegment(a, b, depthOf, near) {
    if (!a || !b) return null
    const da = depthOf(a)
    const db = depthOf(b)
    if (da >= near && db >= near) return [a, b]
    if (da < near && db < near) return null
    return da < near ? [crossing(a, b, da, db, near), b] : [a, crossing(a, b, da, db, near)]
}

// A polyline as the runs of it in front of the plane: [[point, ...], ...]. A run is cut where
// the line goes behind the camera and a new one starts where it comes back.
function clipPolyline(points, depthOf, near) {
    const runs = []
    if (!points || points.length === 0) return runs
    let run = []
    let previous = null
    let previousDepth = 0
    for (const point of points) {
        const depth = depthOf(point)
        if (previous === null) {
            if (depth >= near) run.push(point)
        } else if (depth >= near && previousDepth >= near) {
            run.push(point)
        } else if (depth >= near) {
            run = [crossing(previous, point, previousDepth, depth, near), point]
        } else if (previousDepth >= near) {
            run.push(crossing(previous, point, previousDepth, depth, near))
            if (run.length > 1) runs.push(run)
            run = []
        }
        previous = point
        previousDepth = depth
    }
    if (run.length > 1) runs.push(run)
    return runs
}

// A closed polygon cut by the plane (Sutherland-Hodgman against one plane): the corners in
// front, with the two crossing points added where it is cut. Empty when it is all behind.
function clipPolygon(points, depthOf, near) {
    const result = []
    if (!points || points.length < 3) return result
    for (let index = 0; index < points.length; ++index) {
        const current = points[index]
        const next = points[(index + 1) % points.length]
        const dc = depthOf(current)
        const dn = depthOf(next)
        if (dc >= near) result.push(current)
        if ((dc >= near) !== (dn >= near)) result.push(crossing(current, next, dc, dn, near))
    }
    return result.length >= 3 ? result : []
}
