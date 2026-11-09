import QtQuick
import QtTest
import "../../src/app/qml/inspect/NearClip.js" as NearClip

// Clipping against the camera's near plane, with depth taken as z and the plane at z = 1.
TestCase {
    name: "NearClipTests"

    function depth(point) { return point.z }
    function v(x, y, z) { return Qt.vector3d(x, y, z) }

    function test_aSegmentInFrontIsKeptWhole() {
        const segment = NearClip.clipSegment(v(0, 0, 2), v(4, 0, 6), depth, 1)
        compare(segment[0], v(0, 0, 2))
        compare(segment[1], v(4, 0, 6))
    }

    function test_aSegmentBehindIsDropped() {
        compare(NearClip.clipSegment(v(0, 0, -2), v(4, 0, 0.5), depth, 1), null)
    }

    function test_aSegmentThroughThePlaneIsCutWhereItCrosses() {
        // From z = 3 to z = -1: crosses z = 1 halfway, and nothing past it is kept.
        const segment = NearClip.clipSegment(v(0, 0, 3), v(8, 4, -1), depth, 1)
        compare(segment[0], v(0, 0, 3))
        fuzzyCompare(segment[1].x, 4, 1e-6)
        fuzzyCompare(segment[1].y, 2, 1e-6)
        fuzzyCompare(segment[1].z, 1, 1e-6)
        // Either way round.
        const reversed = NearClip.clipSegment(v(8, 4, -1), v(0, 0, 3), depth, 1)
        fuzzyCompare(reversed[0].x, 4, 1e-6)
        compare(reversed[1], v(0, 0, 3))
    }

    function test_aPolylineGoingBehindAndBackIsTwoRuns() {
        const runs = NearClip.clipPolyline([v(0, 0, 3), v(1, 0, 3), v(2, 0, -3), v(3, 0, 3), v(4, 0, 3)], depth, 1)
        compare(runs.length, 2)
        compare(runs[0].length, 3)
        fuzzyCompare(runs[0][2].z, 1, 1e-6)
        fuzzyCompare(runs[0][2].x, 1 + 1 / 3, 1e-6)
        compare(runs[1].length, 3)
        fuzzyCompare(runs[1][0].z, 1, 1e-6)
        compare(runs[1][2], v(4, 0, 3))
        // No point of any run lies behind the plane.
        for (const run of runs)
            for (const point of run) verify(point.z >= 1 - 1e-9)
    }

    function test_aPolylineAllBehindIsNothing() {
        compare(NearClip.clipPolyline([v(0, 0, -3), v(1, 0, 0), v(2, 0, 0.5)], depth, 1).length, 0)
    }

    function test_aPolygonCutByThePlaneKeepsItsFrontPart() {
        // A square standing across the plane: two corners in front, two behind.
        const clipped = NearClip.clipPolygon([v(0, 0, 3), v(2, 0, 3), v(2, 0, -1), v(0, 0, -1)], depth, 1)
        compare(clipped.length, 4)
        for (const point of clipped) verify(point.z >= 1 - 1e-9)
        compare(NearClip.clipPolygon([v(0, 0, -3), v(2, 0, -3), v(2, 2, -3)], depth, 1).length, 0)
    }
}
