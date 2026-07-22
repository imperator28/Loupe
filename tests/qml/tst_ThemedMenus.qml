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
        property color surfaceRaised: "#182027"
        property color border: "#34414b"
        property color foreground: "#e6edf3"
        property color muted: "#aeb8c2"
        property color accent: "#8B93F8"
        property color accentForeground: "#12142B"
        property color selection: "#2E3560"
    }

    QtObject {
        id: lightTheme
        property bool dark: false
        property color surfaceRaised: "#ffffff"
        property color border: "#b9c5cb"
        property color foreground: "#172127"
        property color muted: "#53636c"
        property color accent: "#4F46E5"
        property color accentForeground: "#ffffff"
        property color selection: "#DEE1FB"
    }

    Component { id: itemComponent; ThemedMenuItem { text: "Inspect" } }

    TestCase {
        name: "ThemedMenuTests"
        when: windowShown

        function test_explicitReadableForegrounds_data() {
            return [
                { tag: "dark", theme: darkTheme, normal: "#e6edf3", muted: "#aeb8c2", accentForeground: "#12142b" },
                { tag: "light", theme: lightTheme, normal: "#172127", muted: "#53636c", accentForeground: "#ffffff" }
            ]
        }

        function test_explicitReadableForegrounds(data) {
            const item = createTemporaryObject(itemComponent, root, { theme: data.theme })
            verify(!!item)

            compare(item.contentItem.color, data.normal)
            item.highlighted = true
            compare(item.contentItem.color, data.accentForeground)
            compare(item.background.color, data.theme.accent)
            item.highlighted = false
            item.enabled = false
            compare(item.contentItem.color, data.muted)
            verify(item.contentItem.color !== data.theme.surfaceRaised)
        }
    }
}
