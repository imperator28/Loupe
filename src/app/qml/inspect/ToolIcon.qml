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
        "capture": {
            s: "M3 7.5 H6.2 L7.7 5.5 H12.3 L13.8 7.5 H17 V15.5 H3 Z",
            f: "",
            t: "M10 8.9 A2.7 2.7 0 1 0 10.01 8.9 Z"
        }
    })

    readonly property var glyph: glyphs[name] !== undefined ? glyphs[name] : { s: "", f: "", t: "" }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: root.color
            strokeWidth: root.strokeWidth
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
