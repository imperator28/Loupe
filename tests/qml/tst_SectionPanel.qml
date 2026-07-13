import QtQuick
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    Component {
        id: panelComponent
        SectionPanel {}
    }

    TestCase {
        name: "SectionPanelTests"
        when: windowShown

        function test_sectionPanelHasDesktopSize() {
            let panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            verify(panel.implicitWidth >= 320)
        }
    }
}
