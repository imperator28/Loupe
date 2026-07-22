import QtQuick

// Indigo Precision token source (design.md). Instantiated once by the shell
// and injected into components; every production color, radius, spacing, and
// duration must come from here. Contrast pairs are verified by
// scripts/check_theme_contrast.py — rerun it when changing any color.
QtObject {
    id: root

    property string mode: "System"
    property bool systemDark: false
    readonly property bool dark: mode === "Dark" || (mode === "System" && systemDark)
    property bool reducedMotion: false

    // Neutral surfaces
    readonly property color canvas: dark ? "#0F1116" : "#F2F3F8"
    readonly property color surface1: dark ? "#151823" : "#ffffff"
    readonly property color surface2: dark ? "#1B1F2C" : "#E9EBF3"
    readonly property color surface3: dark ? "#232838" : "#ffffff"
    readonly property color controlFill: dark ? "#252B3D" : "#E9EBF3"
    readonly property color borderSubtle: dark ? "#2A3040" : "#D7DAE6"
    // Calm, clearly-visible card/panel boundary: a step up from the
    // near-invisible decorative hairline, but softer than borderStrong
    // (reserved for functional/focus-level emphasis like field outlines).
    readonly property color borderPanel: dark ? "#4A5266" : "#A8AFC5"
    readonly property color borderStrong: dark ? "#6C7690" : "#7A829E"

    // Elevation via light, not outlines. Panels imply height through a subtle
    // vertical gradient (lit from above), a faint top highlight edge, and a
    // whisper-thin contact hairline — replacing the hard panel border for a
    // seamless, integrated read. Dark mode lifts the top; light mode lets the
    // surface curve gently away toward the bottom.
    readonly property color panelGradientTop: dark ? "#1B2030" : "#FFFFFF"
    readonly property color panelGradientBottom: dark ? "#141826" : "#F5F7FC"
    readonly property color panelHighlight: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(1, 1, 1, 0.7)
    readonly property color panelHairline: dark ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.07)

    // Text
    readonly property color textPrimary: dark ? "#F2F4F9" : "#1A1D28"
    readonly property color textSecondary: dark ? "#B3BACD" : "#4A5165"
    readonly property color textTertiary: dark ? "#8B93A8" : "#616880"

    // Accent and semantics
    readonly property color accentColor: dark ? "#8B93F8" : "#4F46E5"
    readonly property color accentVivid: dark ? "#A78BFA" : "#7C3AED"
    readonly property color accentForeground: dark ? "#12142B" : "#ffffff"
    readonly property color accentTint: dark ? Qt.rgba(0.545, 0.576, 0.973, 0.16)
                                             : Qt.rgba(0.310, 0.275, 0.898, 0.12)
    readonly property color selectionFill: dark ? "#2E3560" : "#DEE1FB"
    readonly property color warning: dark ? "#FDB022" : "#B54708"
    readonly property color errorColor: dark ? "#F97066" : "#B42318"
    readonly property color success: dark ? "#47CD89" : "#067647"
    readonly property color measure: dark ? "#FFD166" : "#8A5300"
    readonly property color neutralBody: dark ? "#7C8494" : "#9AA2B2"

    // Fixed control details
    readonly property color switchThumb: "#ffffff"

    // Viewport
    readonly property color viewportBackground: dark ? "#10121A" : "#F7F8FC"
    // CAD edges need a clean contrast boundary against shaded bodies. They
    // are deliberately brighter in dark viewports and near-black in light
    // viewports, rather than inheriting a body/material color.
    readonly property color definingEdge: dark ? "#EEF2FF" : "#111318"

    // Legacy role names consumed by existing panels and the C++ test harness.
    // They alias the palette above; new work should prefer the names above.
    readonly property color surface: canvas
    readonly property color viewport: viewportBackground
    readonly property color surfaceRaised: surface1
    readonly property color surfaceSubtle: surface2
    readonly property color control: controlFill
    readonly property color border: borderSubtle
    readonly property color onSurface: textPrimary
    readonly property color foreground: textPrimary
    readonly property color muted: textSecondary
    readonly property color accent: accentColor
    readonly property color selection: selectionFill
    readonly property color selectedBody: dark ? "#FFD166" : "#A86100"
    readonly property color selectedEdge: dark ? "#FFF0A6" : "#7D4A00"
    readonly property color selectionEmissive: dark ? "#3A2C00" : "#2E1700"
    readonly property color viewportMarkerBorder: dark ? "#FFF4CF" : "#FFFFFF"
    readonly property color viewportSectionBorder: dark ? "#F5F3FF" : "#FFFFFF"
    readonly property color edge: definingEdge
    readonly property color error: errorColor

    // Spacing (4 px base, 8 px rhythm)
    readonly property int spacing1: 4
    readonly property int spacing2: 8
    readonly property int spacing3: 12
    readonly property int spacing4: 16
    readonly property int spacing6: 24

    // Shape
    readonly property int radius1: 4
    readonly property int radius2: 6
    readonly property int radius3: 8
    readonly property int radius4: 10
    // Native window shells use a larger radius than controls. Full-window
    // overlays need this token so their border follows the visible frame.
    readonly property int windowRadius: 18
    readonly property int focusRingWidth: 2

    // Density
    readonly property int rowCompact: 28
    readonly property int controlHeight: 30
    readonly property int toolbarHeight: 44

    // Type scale
    readonly property int fontCaption: 11
    readonly property int fontBody: 13
    readonly property int fontSection: 12
    readonly property int fontTitle: 15

    // Motion durations (ms); zero when reduced motion removes travel
    readonly property int durInstant: reducedMotion ? 0 : 80
    readonly property int durFast: reducedMotion ? 0 : 130
    readonly property int durStandard: reducedMotion ? 0 : 180
    readonly property int durPanel: reducedMotion ? 0 : 240
    readonly property int durSpatial: reducedMotion ? 0 : 320
    readonly property int durIndeterminatePulse: 900
    readonly property int durIndeterminateTravel: 1200

    // Easing tokens for Easing.BezierSpline
    readonly property var easeEnter: [0.16, 1.0, 0.30, 1.0, 1.0, 1.0]
    readonly property var easeExit: [0.40, 0.0, 1.0, 1.0, 1.0, 1.0]
    readonly property var easeMove: [0.20, 0.0, 0.0, 1.0, 1.0, 1.0]
}
