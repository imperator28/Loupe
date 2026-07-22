import QtQuick

// A seamless raised surface that implies height with light rather than a hard
// outline: a subtle vertical gradient (lit from above), a faint top-edge
// highlight, and a whisper contact hairline. Drop-in for a panel Rectangle —
// children parent into it as usual. Set `wellSurface: true` for recessed
// content areas (e.g. the 3D viewport) that paint their own background.
Rectangle {
    id: root

    property QtObject theme
    property bool wellSurface: false
    // Flat mode: a small floating surface (e.g. a HUD toolbar) that reads as a
    // simple raised strip, not a large gradient card. Solid fill, hairline,
    // no gradient or top highlight.
    property bool flat: false
    property real cornerRadius: theme && theme.radius3 !== undefined ? theme.radius3 : 8

    radius: cornerRadius
    color: wellSurface && theme ? theme.viewport
           : flat && theme ? theme.surface3
           : "transparent"
    gradient: (wellSurface || flat) ? null : panelGradient
    border.color: theme && theme.panelHairline !== undefined ? theme.panelHairline : "transparent"
    border.width: 1

    Gradient {
        id: panelGradient
        GradientStop { position: 0.0; color: root.theme && root.theme.panelGradientTop !== undefined ? root.theme.panelGradientTop : "transparent" }
        GradientStop { position: 1.0; color: root.theme && root.theme.panelGradientBottom !== undefined ? root.theme.panelGradientBottom : "transparent" }
    }

    // Top-edge highlight: the thin lit line that reads as a raised surface
    // catching light from above. Inset by the corner radius so it follows the
    // rounded corners cleanly.
    Rectangle {
        visible: !root.wellSurface && !root.flat
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: root.cornerRadius
        anchors.rightMargin: root.cornerRadius
        anchors.topMargin: 1
        height: 1
        color: root.theme && root.theme.panelHighlight !== undefined ? root.theme.panelHighlight : "transparent"
    }
}
