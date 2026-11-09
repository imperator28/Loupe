import QtQuick
import QtTest
import "../../src/app/qml"
import "../../src/app/qml/inspect"
import "../../src/app/qml/inspect/NearClip.js" as NearClip

Item {
    id: root
    width: 600
    height: 400

    Theme { id: appTheme; mode: "Light" }
    QtObject {
        id: visual
        property color selectionColor: appTheme.measureColor("amber")
        property color sectionLabel: appTheme.surface3
        property color markerBorder: appTheme.surface3
    }
    // Scene points are already pixels here, and z is the depth in front of the camera: a
    // point with z below 0 is behind it.
    Item {
        id: fakeProjector
        anchors.fill: parent
        function depth(point) { return point.z }
        function project(point) { return point.z < 0 ? Qt.point(-10000, -10000) : Qt.point(point.x, point.y) }
        function onScreen(point, margin) { return point.x >= -margin && point.x <= width + margin && point.y >= -margin && point.y <= height + margin }
        function flat(points) { return points.map(function(point) { return Qt.point(point.x, point.y) }) }
        function projectPath(points) { return NearClip.clipPolyline(points, depth, 0).map(flat) }
        function projectSegment(a, b) {
            const clipped = NearClip.clipSegment(a, b, depth, 0)
            return clipped ? flat(clipped) : null
        }
        function projectPolygon(points) { return flat(NearClip.clipPolygon(points, depth, 0)) }
        function svgRuns(runs) {
            let path = ""
            for (const run of runs) {
                if (run.length < 2) continue
                path += (path.length ? " M " : "M ") + run[0].x + " " + run[0].y
                for (let index = 1; index < run.length; ++index) path += " L " + run[index].x + " " + run[index].y
            }
            return path
        }
    }
    // What lies under a pinned tag: the view, which must not get the press a drag starts with.
    MouseArea {
        id: view
        anchors.fill: parent
        property int presses: 0
        onPressed: presses += 1
    }

    Component {
        id: annotationComponent
        MeasurementAnnotation {
            anchors.fill: parent
            projector: fakeProjector
            theme: appTheme
            visualTheme: visual
            annotation: ({ kind: "label", at: Qt.vector3d(300, 250, 0), text: "12.000 mm" })
        }
    }

    Component {
        id: overlayComponent
        MeasurementOverlay {
            anchors.fill: parent
            theme: appTheme
            visualTheme: visual
        }
    }

    SignalSpy { id: moved; signalName: "tagMoved" }

    TestCase {
        name: "MeasurementAnnotationTests"
        when: windowShown

        function tag(annotation) { return findChild(annotation, "measurementValueTag") }

        function test_aPinnedTagDragsAndKeepsItsLeader() {
            const annotation = createTemporaryObject(annotationComponent, root, { draggable: true })
            moved.target = annotation
            moved.clear()
            view.presses = 0
            const label = tag(annotation)
            verify(label.visible)
            const startX = label.x
            const startY = label.y
            // In the window's coordinates: the tag moves under the pointer as it is dragged.
            const px = label.x + label.width / 2
            const py = label.y + label.height / 2
            mousePress(root, px, py)
            mouseMove(root, px + 40, py - 30)
            mouseMove(root, px + 80, py - 60)
            mouseRelease(root, px + 80, py - 60)
            compare(view.presses, 0, "the drag is the tag's, not the view's")
            compare(moved.count, 1)
            fuzzyCompare(moved.signalArguments[0][0], 80, 1)
            fuzzyCompare(moved.signalArguments[0][1], -60, 1)
            // The pin keeps the offset; the tag sits there.
            annotation.tagOffset = Qt.point(moved.signalArguments[0][0], moved.signalArguments[0][1])
            fuzzyCompare(label.x - startX, 80, 1)
            fuzzyCompare(label.y - startY, -60, 1)
            // Double-click puts it back.
            mouseDoubleClickSequence(root, label.x + label.width / 2, label.y + label.height / 2)
            compare(moved.signalArguments[moved.count - 1][0], 0)
        }

        function test_theResultInProgressIsNotDragged() {
            const annotation = createTemporaryObject(annotationComponent, root, { draggable: false })
            moved.target = annotation
            moved.clear()
            view.presses = 0
            const label = tag(annotation)
            mousePress(root, label.x + label.width / 2, label.y + label.height / 2)
            mouseRelease(root, label.x + label.width / 2, label.y + label.height / 2)
            compare(moved.count, 0)
            compare(view.presses, 1, "a click on it reaches the view")
        }

        function test_aLineRunningBehindTheCameraIsCutNotMirrored() {
            // From (100, 100) in front to (300, 100) behind: crossing z = 0 at x = 200.
            const annotation = createTemporaryObject(annotationComponent, root, {
                annotation: { kind: "distance", from: Qt.vector3d(100, 100, 10), to: Qt.vector3d(300, 100, -10), text: "5 mm" }
            })
            const dimension = findChild(annotation, "measurementDimension")
            verify(dimension.visible)
            fuzzyCompare(annotation.toScreen.x, 200, 1e-6)
            fuzzyCompare(annotation.toScreen.y, 100, 1e-6)
            // Entirely behind: nothing drawn.
            annotation.annotation = { kind: "distance", from: Qt.vector3d(100, 100, -1), to: Qt.vector3d(300, 100, -10), text: "5 mm" }
            verify(!dimension.visible)
        }

        function test_aPickedEdgeBehindTheCameraKeepsOnlyItsFrontPart() {
            const annotation = createTemporaryObject(annotationComponent, root, {
                annotation: ({}),
                edges: [[Qt.vector3d(50, 50, 5), Qt.vector3d(150, 50, 5), Qt.vector3d(250, 50, -5)]]
            })
            const shape = findChild(annotation, "measurementEdge0")
            verify(shape !== null)
            compare(shape.path, "M 50 50 L 150 50 L 200 50")
        }

        function test_theOverlayDrawsNothingOutsideTheViewport() {
            const overlay = createTemporaryObject(overlayComponent, root)
            verify(overlay.clip, "an edge running off the view is cut at its border")
            // With no view to project through, nothing is projected (and nothing throws).
            compare(overlay.projectPath([Qt.vector3d(0, 0, 0), Qt.vector3d(1, 0, 0)]).length, 0)
            compare(overlay.projectSegment(Qt.vector3d(0, 0, 0), Qt.vector3d(1, 0, 0)), null)
        }

        function test_aTagIsACardInTheThemeWithTheResultsColourOutline() {
            const annotation = createTemporaryObject(annotationComponent, root, { lineColor: appTheme.measureColor("teal") })
            const label = tag(annotation)
            verify(Qt.colorEqual(label.color, appTheme.surface3))
            verify(Qt.colorEqual(label.border.color, appTheme.measureColor("teal")))
        }
    }
}
