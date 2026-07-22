import QtQuick
import QtQuick3D

QtObject {
    id: root

    required property QtObject theme
    property bool transparentCapture: false

    readonly property bool dark: theme.dark
    readonly property int backgroundMode: transparentCapture ? SceneEnvironment.Transparent : SceneEnvironment.Color
    readonly property color canvasBackground: transparentCapture ? "transparent" : theme.viewport
    readonly property color edgeColor: theme.edge
    readonly property color selectionColor: theme.selectedBody
    readonly property color selectionEdgeColor: theme.selectedEdge
    // Muted, formula-derived tint of the accent (not a literal hex): a
    // full-strength accent fill would obscure the hovered surface, so this
    // desaturates and shifts lightness away from the base accent instead.
    readonly property color accentBase: theme.accent
    readonly property color hoverColor: Qt.hsla(accentBase.hslHue,
                                                accentBase.hslSaturation * 0.55,
                                                dark ? accentBase.hslLightness * 0.72
                                                     : Math.min(0.92, accentBase.hslLightness * 1.18),
                                                1.0)
    readonly property color selectionEmissive: theme.selectionEmissive
    readonly property color hoverEmissive: Qt.hsla(accentBase.hslHue, accentBase.hslSaturation * 0.4,
                                                    dark ? 0.16 : 0.42, 1.0)
    readonly property color markerBorder: theme.viewportMarkerBorder
    // Fixed off-white contrast dot for the section pivot marker; it sits on
    // top of the accentVivid arrow/handle and must read against it in both
    // themes, so it does not derive from the accent token.
    readonly property color sectionBorder: theme.viewportSectionBorder
    readonly property color sectionLabel: theme.accentForeground
    readonly property real keyLightBrightness: dark ? 1.1 : 1.2
    readonly property real fillLightBrightness: dark ? 0.55 : 0.7
}
