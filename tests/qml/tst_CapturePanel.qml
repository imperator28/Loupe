import QtQuick
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    Component {
        id: panelComponent
        CapturePanel {}
    }

    TestCase {
        name: "CapturePanelTests"
        when: windowShown

        function test_capturePanelHasDesktopSize() {
            let panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            verify(panel.implicitWidth >= 320)
        }
    }
}
