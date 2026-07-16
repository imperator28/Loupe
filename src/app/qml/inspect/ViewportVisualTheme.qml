import QtQuick
import QtQuick3D

QtObject {
    id: root

    property QtObject theme
    property bool transparentCapture: false

    readonly property bool dark: theme ? theme.dark : true
    readonly property int backgroundMode: transparentCapture ? SceneEnvironment.Transparent : SceneEnvironment.Color
    readonly property color canvasBackground: transparentCapture ? "transparent" : theme ? theme.viewport : "#101418"
    readonly property color edgeColor: theme ? theme.edge : "#b8c7d1"
    readonly property color selectionColor: theme ? theme.selectedBody : "#ffd166"
    readonly property color selectionEdgeColor: theme ? theme.selectedEdge : "#fff0a6"
    readonly property color hoverColor: dark ? "#3e8999" : "#78c7c1"
    readonly property color selectionEmissive: dark ? "#3a2c00" : "#2e1700"
    readonly property color hoverEmissive: dark ? "#17313a" : "#154c48"
    readonly property color markerBorder: dark ? "#fff4cf" : "#ffffff"
    readonly property color sectionBorder: dark ? "#dffbf4" : "#ffffff"
    readonly property color sectionLabel: dark ? "#101418" : "#ffffff"
    readonly property real keyLightBrightness: dark ? 1.1 : 1.2
    readonly property real fillLightBrightness: dark ? 0.55 : 0.7
}
