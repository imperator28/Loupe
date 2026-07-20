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
        property color control: "#dde5e9"
        property color surfaceSubtle: "#e8edf0"
        property color border: "#b9c5cb"
        property color selection: "#a8ddd7"
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
            verify(control.implicitWidth > 0)
        }

        function test_projectionButtonsRemainExclusive() {
            const control = createTemporaryObject(controlComponent, root)
            verify(!!control)
            const buttons = control.children.filter(function(child) { return child.checkable === true })
            compare(buttons.length, 2)

            mouseClick(buttons[1])
            compare(buttons[0].checked, false)
            compare(buttons[1].checked, true)
        }
    }
}
