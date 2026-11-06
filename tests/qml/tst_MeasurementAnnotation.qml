import QtQuick
import QtTest
import "../../src/app/qml"
import "../../src/app/qml/inspect"

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
    // Scene points are already pixels here.
    Item {
        id: fakeProjector
        anchors.fill: parent
        function project(point) { return Qt.point(point.x, point.y) }
        function onScreen(point, margin) { return point.x >= -margin && point.x <= width + margin && point.y >= -margin && point.y <= height + margin }
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

        function test_aTagIsACardInTheThemeWithTheResultsColourOutline() {
            const annotation = createTemporaryObject(annotationComponent, root, { lineColor: appTheme.measureColor("teal") })
            const label = tag(annotation)
            verify(Qt.colorEqual(label.color, appTheme.surface3))
            verify(Qt.colorEqual(label.border.color, appTheme.measureColor("teal")))
        }
    }
}
