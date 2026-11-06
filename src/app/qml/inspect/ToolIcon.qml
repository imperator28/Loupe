import QtQuick
import QtQuick.Shapes

// One 20x20 outline glyph from Loupe's tool icon family (1.5 px stroke,
// round joins, per design.md iconography). `name` selects the glyph;
// `color` comes from the hosting control's text color.
Item {
    id: root

    property string name: ""
    property color color: "transparent"
    property real strokeWidth: 1.5

    implicitWidth: 20
    implicitHeight: 20

    // Each entry: s = stroked subpaths, f = solid-filled subpaths,
    // t = translucent tinted region. All in a 20x20 viewbox.
    readonly property var glyphs: ({
        "fit": {
            s: "M6.5 3 H4.5 Q3 3 3 4.5 V6.5 M13.5 3 H15.5 Q17 3 17 4.5 V6.5 "
               + "M17 13.5 V15.5 Q17 17 15.5 17 H13.5 M3 13.5 V15.5 Q3 17 4.5 17 H6.5",
            f: "M10 8.6 A1.4 1.4 0 1 0 10.01 8.6 Z",
            t: ""
        },
        "isolate": {
            s: "M10 3.2 A6.8 6.8 0 1 0 10.01 3.2 Z",
            f: "M10 7.9 A2.1 2.1 0 1 0 10.01 7.9 Z",
            t: ""
        },
        "hide": {
            s: "M2.6 10 C5.1 5.9 14.9 5.9 17.4 10 C14.9 14.1 5.1 14.1 2.6 10 Z "
               + "M4.6 15.4 L15.4 4.6",
            f: "M10 8 A2 2 0 1 0 10.01 8 Z",
            t: ""
        },
        // "hide" without its strike: shown, for the tree's visibility toggle.
        "eye": {
            s: "M2.6 10 C5.1 5.9 14.9 5.9 17.4 10 C14.9 14.1 5.1 14.1 2.6 10 Z",
            f: "M10 8 A2 2 0 1 0 10.01 8 Z",
            t: ""
        },
        "transparent": {
            s: "M3 3 H12 V12 H3 Z M8 8 H17 V17 H8 Z",
            f: "",
            t: "M8 8 H17 V17 H8 Z"
        },
        "section": {
            s: "M4 4 H16 V16 H4 Z M4 12.5 L16 7.5",
            f: "",
            t: "M4 12.5 L16 7.5 V16 H4 Z"
        },
        "measure": {
            s: "M3.5 13.5 L13.5 3.5 L16.5 6.5 L6.5 16.5 Z "
               + "M6.9 10.1 L8.4 11.6 M9.4 7.6 L10.9 9.1 M11.9 5.1 L13.4 6.6",
            f: "",
            t: ""
        },
        // A block in the middle with its four corners pulled away from it.
        "explode": {
            s: "M2.5 2.5 H6.5 V6.5 H2.5 Z M13.5 2.5 H17.5 V6.5 H13.5 Z "
               + "M2.5 13.5 H6.5 V17.5 H2.5 Z M13.5 13.5 H17.5 V17.5 H13.5 Z "
               + "M8.6 8.6 L7.4 7.4 M11.4 8.6 L12.6 7.4 M8.6 11.4 L7.4 12.6 M11.4 11.4 L12.6 12.6",
            f: "",
            t: "M8.5 8.5 H11.5 V11.5 H8.5 Z"
        },
        // The exploded view's handles, for the panel's legend.
        "handleArrow": {
            s: "M3 10 H14.5 M11 6.2 L15 10 L11 13.8",
            f: "",
            t: ""
        },
        "handleBall": {
            s: "",
            f: "M10 6.4 A3.6 3.6 0 1 0 10.01 6.4 Z",
            t: ""
        },
        "handleRing": {
            // Most of a circle, and an arrowhead growing from the arc's end along its turn.
            s: "M13.00 4.80 A6 6 0 1 1 6.14 5.40 M5.12 8.22 L6.14 5.40 L3.19 5.92",
            f: "",
            t: ""
        },
        // A datum: where the handle sits and turns about.
        "pivot": {
            s: "M10 2.5 V6.8 M10 13.2 V17.5 M2.5 10 H6.8 M13.2 10 H17.5 M10 5.2 A4.8 4.8 0 1 0 10.01 5.2 Z",
            f: "M10 8.6 A1.4 1.4 0 1 0 10.01 8.6 Z",
            t: ""
        },
        // Mice and trackpads for the interaction guide: which button, or the wheel.
        "mouseLeft": {
            s: "M10 2.5 C6.4 2.5 5 4.6 5 7.5 V12.5 C5 15.4 6.8 17.5 10 17.5 C13.2 17.5 15 15.4 15 12.5 V7.5 C15 4.6 13.6 2.5 10 2.5 Z M10 2.5 V8 M5 8 H15",
            f: "M10 2.5 C6.4 2.5 5 4.6 5 7.5 V8 H10 Z",
            t: ""
        },
        "mouseRight": {
            s: "M10 2.5 C6.4 2.5 5 4.6 5 7.5 V12.5 C5 15.4 6.8 17.5 10 17.5 C13.2 17.5 15 15.4 15 12.5 V7.5 C15 4.6 13.6 2.5 10 2.5 Z M10 2.5 V8 M5 8 H15",
            f: "M10 2.5 C13.6 2.5 15 4.6 15 7.5 V8 H10 Z",
            t: ""
        },
        "mouseMiddle": {
            s: "M10 2.5 C6.4 2.5 5 4.6 5 7.5 V12.5 C5 15.4 6.8 17.5 10 17.5 C13.2 17.5 15 15.4 15 12.5 V7.5 C15 4.6 13.6 2.5 10 2.5 Z M5 8 H15",
            f: "M9 3.8 H11 V7 H9 Z",
            t: ""
        },
        "trackpadScroll": {
            s: "M2.5 4.5 H17.5 V15.5 H2.5 Z M10 6.2 V8 M8.8 7 L10 5.8 L11.2 7 M10 12 V13.8 M8.8 12.8 L10 14 L11.2 12.8",
            f: "M7.6 10 A1.3 1.3 0 1 0 7.61 10 Z M12.4 10 A1.3 1.3 0 1 0 12.41 10 Z",
            t: ""
        },
        "trackpadPinch": {
            s: "M2.5 4.5 H17.5 V15.5 H2.5 Z M5.5 7.5 L7.2 9.2 M14.5 12.5 L12.8 10.8",
            f: "M8.2 10 A1.3 1.3 0 1 0 8.21 10 Z M11.8 10 A1.3 1.3 0 1 0 11.81 10 Z",
            t: ""
        },
        "viewCube": {
            s: "M10 2.8 L16.5 6.4 V13.6 L10 17.2 L3.5 13.6 V6.4 Z M3.5 6.4 L10 10 L16.5 6.4 M10 10 V17.2",
            f: "",
            t: "M10 2.8 L16.5 6.4 L10 10 L3.5 6.4 Z"
        },
        // Small chrome glyphs: disclosure, collapse/expand and close.
        "chevronRight": {
            s: "M8 5.5 L12.5 10 L8 14.5",
            f: "",
            t: ""
        },
        "chevronDown": {
            s: "M5.5 8 L10 12.5 L14.5 8",
            f: "",
            t: ""
        },
        "chevronUp": {
            s: "M5.5 12 L10 7.5 L14.5 12",
            f: "",
            t: ""
        },
        // An arrow down onto a tray: a newer version to fetch.
        "download": {
            s: "M10 3.5 V12 M6.5 8.6 L10 12.1 L13.5 8.6 M4.5 15.5 H15.5",
            f: "",
            t: ""
        },
        "check": {
            s: "M4.8 10.4 L8.4 14 L15.2 6.6",
            f: "",
            t: ""
        },
        // A circle with a mark: something went wrong.
        "alert": {
            s: "M10 3 A7 7 0 1 0 10.01 3 Z M10 6.4 V10.8",
            f: "M10 12.6 A1 1 0 1 0 10.01 12.6 Z",
            t: ""
        },
        // The colour theme: follow the system (a display), light (a sun), dark (a moon).
        "themeSystem": {
            s: "M3 4.5 H17 V13.5 H3 Z M7 16.5 H13 M10 13.5 V16.5",
            f: "",
            t: "M10 4.5 H17 V13.5 H10 Z"
        },
        "themeLight": {
            s: "M10 6.8 A3.2 3.2 0 1 0 10.01 6.8 Z M15.30 10.00 L17.20 10.00 M13.75 13.75 L15.09 15.09 M10.00 15.30 L10.00 17.20 M6.25 13.75 L4.91 15.09 M4.70 10.00 L2.80 10.00 M6.25 6.25 L4.91 4.91 M10.00 4.70 L10.00 2.80 M13.75 6.25 L15.09 4.91",
            f: "",
            t: ""
        },
        "themeDark": {
            s: "M15.6 12.4 A6.4 6.4 0 1 1 7.6 3.9 A5.1 5.1 0 0 0 15.6 12.4 Z",
            f: "",
            t: ""
        },
        // Tabs: a window with two tabs along its top, the first in front.
        "tabs": {
            s: "M3 7.5 H17 V16 H3 Z M3 7.5 V4.5 H9 V7.5 M9 7.5 V4.5 H14.5 V7.5",
            f: "",
            t: "M3 4.5 H9 V7.5 H3 Z"
        },
        // Saved views: two snapshots, one behind the other.
        "views": {
            s: "M3 7 H14 V16.5 H3 Z M6 7 V4 H17 V13.5 H14",
            f: "",
            t: "M3 7 H14 V16.5 H3 Z"
        },
        // Rename: a pencil.
        "rename": {
            s: "M4.5 15.5 L5.2 12.4 L12.9 4.7 Q13.6 4 14.3 4.7 L15.3 5.7 Q16 6.4 15.3 7.1 L7.6 14.8 Z M11.6 6 L14 8.4",
            f: "",
            t: ""
        },
        // Add: a plus.
        "add": {
            s: "M10 4.5 V15.5 M4.5 10 H15.5",
            f: "",
            t: ""
        },
        // A drag grip: two columns of three dots.
        "grip": {
            s: "",
            f: "M8 5.2 A1.2 1.2 0 1 0 8.01 5.2 Z M12 5.2 A1.2 1.2 0 1 0 12.01 5.2 Z M8 8.8 A1.2 1.2 0 1 0 8.01 8.8 Z M12 8.8 A1.2 1.2 0 1 0 12.01 8.8 Z M8 12.4 A1.2 1.2 0 1 0 8.01 12.4 Z M12 12.4 A1.2 1.2 0 1 0 12.01 12.4 Z",
            t: ""
        },
        "expandAll": {
            s: "M6 5.5 L10 9.5 L14 5.5 M6 10.5 L10 14.5 L14 10.5",
            f: "",
            t: ""
        },
        "collapseAll": {
            s: "M6 9.5 L10 5.5 L14 9.5 M6 14.5 L10 10.5 L14 14.5",
            f: "",
            t: ""
        },
        "close": {
            s: "M6 6 L14 14 M14 6 L6 14",
            f: "",
            t: ""
        },
        // Measure modes. Two picks, then one, then the whole part.
        "measurePoints": {
            s: "M5.6 14.4 L14.4 5.6",
            f: "M4 14.2 A1.8 1.8 0 1 0 4.01 14.2 Z M16 2.2 A1.8 1.8 0 1 0 16.01 2.2 Z",
            t: ""
        },
        "measureSurfaces": {
            s: "M3 4.5 H17 M3 15.5 H17 M10 7 V13 M8.2 8.8 L10 7 L11.8 8.8 M8.2 11.2 L10 13 L11.8 11.2",
            f: "",
            t: ""
        },
        "measureAngle": {
            s: "M3.5 16 H16.5 M3.5 16 L12.5 4.5 M9.5 16 A6 6 0 0 0 7.2 11.3",
            f: "",
            t: ""
        },
        "measureEdge": {
            s: "M4.5 15.5 L15.5 4.5 M2.8 13.8 L6.2 17.2 M13.8 2.8 L17.2 6.2",
            f: "",
            t: ""
        },
        "measureRadius": {
            s: "M10 3 A7 7 0 1 0 10.01 3 Z M10 10 L15 5",
            f: "M10 8.8 A1.2 1.2 0 1 0 10.01 8.8 Z",
            t: ""
        },
        "measureArea": {
            s: "M4 4 H16 V16 H4 Z",
            f: "",
            t: "M4 4 H16 V16 H4 Z"
        },
        "measureVolume": {
            s: "M10 2.8 L16.5 6.4 V13.6 L10 17.2 L3.5 13.6 V6.4 Z M3.5 6.4 L10 10 L16.5 6.4 M10 10 V17.2",
            f: "",
            t: "M3.5 6.4 L10 10 V17.2 L3.5 13.6 Z"
        },
        "measureBounds": {
            s: "M3 7 V3 H7 M13 3 H17 V7 M17 13 V17 H13 M7 17 H3 V13 M7.5 7.5 H12.5 V12.5 H7.5 Z",
            f: "",
            t: ""
        },
        // Pin: a pushpin, for keeping a measurement in the view.
        "pin": {
            s: "M7.5 3.5 H12.5 L12 8 L14.5 11 H5.5 L8 8 Z M10 11 V16.5",
            f: "",
            t: ""
        },
        // Copy: two sheets.
        "copy": {
            s: "M7.5 7.5 H16 V16 H7.5 Z M4 12.5 V4 H12.5",
            f: "",
            t: ""
        },
        // Save over: a circling arrow, for updating a saved entry with what is there now.
        "saveOver": {
            s: "M15.6 8.2 A6 6 0 1 0 15.8 12.4 M16 4.2 V8.4 H11.8",
            f: "",
            t: ""
        },
        "capture": {
            s: "M3 7.5 H6.2 L7.7 5.5 H12.3 L13.8 7.5 H17 V15.5 H3 Z",
            f: "",
            t: "M10 8.9 A2.7 2.7 0 1 0 10.01 8.9 Z"
        }
    })

    readonly property var glyph: glyphs[name] !== undefined ? glyphs[name] : { s: "", f: "", t: "" }

    // Drawn in a 20 px box and scaled to the item's size, the stroke kept at its width.
    readonly property real drawScale: Math.max(0.01, Math.min(width, height) / 20)

    Shape {
        width: 20
        height: 20
        x: (root.width - 20 * root.drawScale) / 2
        y: (root.height - 20 * root.drawScale) / 2
        transform: Scale { xScale: root.drawScale; yScale: root.drawScale }
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth / root.drawScale
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: root.glyph.s }
        }

        ShapePath {
            strokeColor: "transparent"
            fillColor: root.color
            PathSvg { path: root.glyph.f }
        }

        ShapePath {
            strokeColor: "transparent"
            fillColor: Qt.rgba(root.color.r, root.color.g, root.color.b, 0.3)
            PathSvg { path: root.glyph.t }
        }
    }
}
