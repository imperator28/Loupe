import QtQuick
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    Component {
        id: panelComponent
        MeasurementPanel {}
    }

    TestCase {
        name: "MeasurementPanelTests"
        when: windowShown

        function test_measurementOptionsAreAvailable() {
            let panel = createTemporaryObject(panelComponent, root)
            verify(!!panel, "Component exists")
            compare(panel.modes.length, 8)
            verify(panel.implicitWidth > 0)
        }
    }
}
