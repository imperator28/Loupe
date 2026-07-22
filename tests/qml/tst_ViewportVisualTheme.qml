import QtQuick
import QtQuick3D
import QtTest
import "../../src/app/qml/inspect"

Item {
    id: root
    width: 800
    height: 600

    QtObject {
        id: testTheme
        property bool dark: true
        property color viewport: dark ? "#101418" : "#f7f9fa"
        property color edge: dark ? "#eef2ff" : "#111318"
        property color selectedBody: dark ? "#ffd166" : "#a86100"
        property color selectedEdge: dark ? "#fff0a6" : "#7d4a00"
        property color selectionEmissive: dark ? "#3a2c00" : "#2e1700"
        property color viewportMarkerBorder: dark ? "#fff4cf" : "#ffffff"
        property color viewportSectionBorder: dark ? "#f5f3ff" : "#ffffff"
        property color accentForeground: dark ? "#12142b" : "#ffffff"
        property color accent: dark ? "#67d5c0" : "#087b74"
    }

    Component {
        id: visualThemeComponent
        ViewportVisualTheme { theme: testTheme }
    }

    TestCase {
        name: "ViewportVisualThemeTests"
        when: windowShown

        function test_darkThemeDrivesCanvasAndEdgeContrast() {
            const visual = createTemporaryObject(visualThemeComponent, root)
            verify(!!visual, "Viewport visual theme exists")

            testTheme.dark = true
            compare(visual.canvasBackground, "#101418")
            compare(visual.edgeColor, "#eef2ff")
            compare(visual.selectionColor, "#ffd166")
            compare(visual.backgroundMode, SceneEnvironment.Color)
        }

        function test_lightThemeDrivesCanvasAndEdgeContrast() {
            const visual = createTemporaryObject(visualThemeComponent, root)
            verify(!!visual, "Viewport visual theme exists")

            testTheme.dark = false
            compare(visual.canvasBackground, "#f7f9fa")
            compare(visual.edgeColor, "#111318")
            compare(visual.selectionColor, "#a86100")
            compare(visual.backgroundMode, SceneEnvironment.Color)
        }

        function test_transparentCaptureOverridesOnlyTheCanvas() {
            const visual = createTemporaryObject(visualThemeComponent, root)
            verify(!!visual, "Viewport visual theme exists")

            testTheme.dark = false
            visual.transparentCapture = true
            compare(visual.backgroundMode, SceneEnvironment.Transparent)
            compare(visual.canvasBackground, "#00000000")
            compare(visual.edgeColor, "#111318")
            compare(visual.selectionColor, "#a86100")
        }
    }
}
