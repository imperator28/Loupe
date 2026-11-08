import QtQuick
import QtTest
import "../../src/app/qml/inspect"

TestCase {
    name: "ExplodeHandleMathTests"

    ExplodeHandleMath {
        id: math
    }

    function circle(cx, cy, radius) {
        const points = []
        for (let step = 0; step < 48; ++step) {
            const angle = step / 48 * 2 * Math.PI
            points.push({ x: cx + radius * Math.cos(angle), y: cy + radius * Math.sin(angle) })
        }
        return points
    }

    // Seen straight down Z: X to the right, Y up the screen, Z at the viewer (a dot).
    function topViewLayout() {
        return {
            centre: { x: 200, y: 200 },
            tips: [{ x: 296, y: 200 }, { x: 200, y: 104 }, { x: 200, y: 200 }],
            rings: [
                [{ x: 200, y: 131 }, { x: 200, y: 269 }],
                [{ x: 131, y: 200 }, { x: 269, y: 200 }],
                circle(200, 200, 69)
            ]
        }
    }

    function test_segmentDistance() {
        compare(math.distanceToSegment(5, 3, 0, 0, 10, 0), 3)
        compare(math.distanceToSegment(-4, 3, 0, 0, 10, 0), 5)
        compare(math.distanceToSegment(2, 2, 0, 0, 0, 0), Math.sqrt(8))
    }

    function test_pressOnAnArrowMovesAlongIt() {
        const hit = math.handleAt(260, 203, topViewLayout())
        compare(hit.kind, "move")
        compare(hit.axis, 0)
        compare(math.handleAt(202, 140, topViewLayout()).axis, 1)
    }

    // The arrow straight at the viewer is too short to drag along; the press falls through
    // to whatever ring is there.
    function test_anArrowSeenEndOnIsNotATarget() {
        const hit = math.handleAt(200 + 69 * Math.cos(0.8), 200 + 69 * Math.sin(0.8), topViewLayout())
        compare(hit.kind, "rotate")
        compare(hit.axis, 2)
    }

    function test_arrowsWinWhereTheyCrossARing() {
        // (269, 200) is on the Z ring and on the X arrow.
        const hit = math.handleAt(269, 200, topViewLayout())
        compare(hit.kind, "move")
        compare(hit.axis, 0)
    }

    function test_pressAwayFromTheHandlesIsNothing() {
        compare(math.handleAt(20, 20, topViewLayout()), null)
        compare(math.handleAt(200, 200, null), null)
        // Just out of the ball in the middle, an edge-on ring is.
        compare(math.handleAt(200, 225, topViewLayout()).kind, "rotate")
    }

    // The ball in the middle moves freely, over the edge-on rings that cross it.
    function test_theMiddleMovesFreely() {
        compare(math.handleAt(203, 201, topViewLayout()).kind, "free")
        compare(math.handleAt(200, 200, topViewLayout()).kind, "free")
    }

    function test_screenPlaneTravel() {
        // Right is +X and up is +Z on screen, at 0.5 mm a pixel: 10 px right and 4 px up
        // (screen y grows downward) is 5 mm along X and 2 mm along Z.
        const travel = math.screenPlaneTravel(10, -4, Qt.vector3d(1, 0, 0), Qt.vector3d(0, 0, 1), 0.5)
        compare(travel.x, 5)
        compare(travel.y, 0)
        compare(travel.z, 2)
    }

    function test_travelAlongAnAxis() {
        // The axis covers 100 px for 50 mm: 40 px along it is 20 mm, across it nothing.
        compare(math.axisTravel(40, 0, 100, 0, 50), 20)
        compare(math.axisTravel(0, 40, 100, 0, 50), 0)
        fuzzyCompare(math.axisTravel(-30, 40, 60, 80, 10), 1.4, 1e-9)
        compare(math.axisTravel(10, 10, 0, 0, 10), 0)
    }

    function test_angleStepCrossesTheSeam() {
        compare(math.angleStep(170, -170), 20)
        compare(math.angleStep(-170, 170), -20)
        compare(math.angleStep(10, 40), 30)
    }

    function test_screenAngle() {
        compare(math.screenAngle(10, 0, { x: 0, y: 0 }), 0)
        compare(math.screenAngle(0, 10, { x: 0, y: 0 }), 90)
    }

    // A section's cut follows the pointer along its axis: the axis point nearest the ray.
    function test_alongLineNearestRay() {
        // A ray straight down through (5, *, 0) meets the X axis at t = 5.
        const t = math.alongLineNearestRay(Qt.vector3d(5, 10, 0), Qt.vector3d(5, -10, 0),
                                           Qt.vector3d(0, 0, 0), Qt.vector3d(1, 0, 0))
        fuzzyCompare(t, 5, 1e-9)
        // A skew ray, slanted and offset in depth: still the nearest point, x = 3 on the axis.
        const skew = math.alongLineNearestRay(Qt.vector3d(3, 4, 7), Qt.vector3d(3, -4, 9),
                                              Qt.vector3d(-2, 0, 0), Qt.vector3d(1, 0, 0))
        fuzzyCompare(skew, 5, 1e-9)
        // Along the ray itself: no answer, rather than a wild one.
        compare(math.alongLineNearestRay(Qt.vector3d(0, 0, 10), Qt.vector3d(0, 0, -10),
                                         Qt.vector3d(0, 0, 0), Qt.vector3d(0, 0, 1)), null)
    }
}
