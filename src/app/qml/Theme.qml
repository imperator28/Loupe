pragma Singleton
import QtQuick

QtObject {
    id: root

    property string mode: "System"
    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }
    readonly property bool dark: mode === "Dark" || (mode === "System"
        && systemPalette.window.r + systemPalette.window.g + systemPalette.window.b < 1.5)

    readonly property color surface: dark ? "#101418" : "#f5f7f8"
    readonly property color viewport: dark ? "#101418" : "#f7f9fa"
    readonly property color surfaceRaised: dark ? "#182027" : "#ffffff"
    readonly property color surfaceSubtle: dark ? "#202a33" : "#e8edf0"
    readonly property color control: dark ? "#26323c" : "#dde5e9"
    readonly property color border: dark ? "#34414b" : "#b9c5cb"
    readonly property color onSurface: dark ? "#e6edf3" : "#172127"
    readonly property color foreground: onSurface
    readonly property color muted: dark ? "#aeb8c2" : "#53636c"
    readonly property color accent: dark ? "#67d5c0" : "#087b74"
    readonly property color selection: dark ? "#315b64" : "#a8ddd7"
    readonly property color selectedBody: dark ? "#ffd166" : "#a86100"
    readonly property color selectedEdge: dark ? "#fff0a6" : "#7d4a00"
    readonly property color edge: dark ? "#b8c7d1" : "#3a525d"
    readonly property color error: dark ? "#ffb4ab" : "#b42318"
}
