import QtQuick
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 640
    height: 480

    QtObject {
        id: darkTheme
        property bool dark: true
        property color surface: "#101418"
        property color surfaceRaised: "#182027"
        property color surfaceSubtle: "#202a33"
        property color control: "#26323c"
        property color border: "#34414b"
        property color foreground: "#e6edf3"
        property color muted: "#aeb8c2"
        property color accent: "#67d5c0"
        property color selection: "#315b64"
    }

    Component { id: comboComponent; ThemedComboBox { theme: darkTheme; model: ["One", "Two"] } }
    Component { id: spinComponent; ThemedSpinBox { theme: darkTheme; value: 12 } }
    Component { id: switchComponent; ThemedSwitch { theme: darkTheme; text: "Visible label" } }
    Component { id: radioComponent; ThemedRadioButton { theme: darkTheme; text: "Visible choice" } }
    Component { id: fieldComponent; ThemedTextField { theme: darkTheme; text: "Visible value" } }

    TestCase {
        name: "ThemedControlsTests"
        when: windowShown

        function test_darkThemeUsesExplicitReadableForegrounds() {
            const combo = createTemporaryObject(comboComponent, root)
            const spin = createTemporaryObject(spinComponent, root)
            const toggle = createTemporaryObject(switchComponent, root)
            const radio = createTemporaryObject(radioComponent, root)
            const field = createTemporaryObject(fieldComponent, root)
            verify(!!combo && !!spin && !!toggle && !!radio && !!field)

            compare(combo.contentItem.color, "#e6edf3")
            compare(spin.contentItem.color, "#e6edf3")
            compare(toggle.contentItem.color, "#e6edf3")
            compare(radio.contentItem.color, "#e6edf3")
            compare(field.color, "#e6edf3")
            verify(combo.contentItem.color !== darkTheme.surfaceRaised)
            verify(toggle.contentItem.color !== darkTheme.surfaceRaised)
        }
    }
}
