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
        // The middle of the handle is neither an arrow nor the edge-on rings through it.
        compare(math.handleAt(203, 201, topViewLayout()), null)
        // Just out of it, an edge-on ring is.
        compare(math.handleAt(200, 225, topViewLayout()).kind, "rotate")
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
}
