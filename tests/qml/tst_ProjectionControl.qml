import QtQuick
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 500
    height: 200

    QtObject {
        id: mockTheme
        property bool dark: false
        property color foreground: "#172127"
        property color muted: "#53636c"
        property color control: "#dde5e9"
        property color surfaceRaised: "#ffffff"
        property color surface3: "#ffffff"
        property color surfaceSubtle: "#e8edf0"
        property color border: "#b9c5cb"
        property color borderStrong: "#7a829e"
        property color accent: "#4f46e5"
        property color accentForeground: "#ffffff"
        property color selection: "#dee1fb"
        property int radius1: 4
        property int radius2: 6
        property int radius3: 8
        property int fontCaption: 11
        property int durInstant: 0
        property int durFast: 0
        property int focusRingWidth: 2
    }

    Component {
        id: controlComponent
        ProjectionControl { theme: mockTheme }
    }

    TestCase {
        name: "ProjectionControlTests"
        when: windowShown

        function test_defaultsToOrthographic() {
            const control = createTemporaryObject(controlComponent, root)
            verify(!!control)
            compare(control.projectionMode, "orthographic")
            compare(control.currentIndex, 0)
            verify(control.implicitWidth > 0)
        }

        function test_currentIndexTracksProjectionMode() {
            const control = createTemporaryObject(controlComponent, root, { projectionMode: "perspective" })
            verify(!!control)
            compare(control.currentIndex, 1)
        }

        function test_selectingPerspectiveRequestsPerspectiveProjection() {
            const control = createTemporaryObject(controlComponent, root)
            verify(!!control)
            let requestedMode = ""
            control.projectionRequested.connect(function(mode) { requestedMode = mode })

            control.currentIndex = 1
            control.activated(1)

            compare(requestedMode, "perspective")
        }
    }
}
