import QtQuick

// A draggable divider in the gap beside one of a workspace's side panels. It owns the
// panel's width: bind the panel's Layout.preferredWidth to `effectiveWidth`.
//
// Place it as a sibling of the RowLayout holding the panel (not inside the panel, whose
// content may clip, nor inside the layout, which would lay it out). It sits centred in
// the layout's spacing on the panel's inner edge: the right edge of a left panel, the
// left edge of a right one.
//
// The width is the user's, remembered per `key` in `preferences.panelWidths`, held between
// `minimumWidth` and `maximumWidth`. Bind `maximumWidth` to the room actually left, so a
// smaller window narrows the panel instead of squeezing the centre below its minimum;
// the stored width is kept, and the panel grows back when the room returns. A double
// click restores the default.
MouseArea {
    id: splitter

    property QtObject theme
    property QtObject preferences
    property string key
    property Item panel
    // The RowLayout's spacing: the gap the divider sits in.
    property real gap: 12
    // The panel is left of the divider (true) or right of it (false).
    property bool panelOnLeft: true
    property real defaultWidth: 300
    property real minimumWidth: 200
    property real maximumWidth: 600

    // What the user asked for, unclamped by the window: the remembered width, or the live
    // one during a drag.
    readonly property real storedWidth: {
        const widths = preferences ? preferences.panelWidths : null
        return widths && widths[key] !== undefined ? widths[key] : defaultWidth
    }
    property real dragWidth: -1
    readonly property real requestedWidth: dragWidth >= 0 ? dragWidth : storedWidth
    // Held to the static limits only. A neighbour can size itself against this without
    // depending on the room left, which depends on the neighbour: no binding loop.
    readonly property real staticWidth: Math.max(minimumWidth, requestedWidth)
    readonly property real effectiveWidth: clampWidth(requestedWidth)

    function clampWidth(width) {
        return Math.max(minimumWidth, Math.min(Math.max(minimumWidth, maximumWidth), width))
    }

    visible: panel !== null && panel.visible
    x: panel ? panel.parent.x + panel.x + (panelOnLeft ? panel.width + gap / 2 : -gap / 2) - width / 2 : 0
    y: panel ? panel.parent.y + panel.y : 0
    height: panel ? panel.height : 0
    width: Math.max(8, gap)
    z: 10
    cursorShape: Qt.SplitHCursor
    hoverEnabled: true
    preventStealing: true
    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Resize panel")

    property real startX: 0
    property real startWidth: 0
    onPressed: function(mouse) {
        startX = mapToItem(null, mouse.x, 0).x
        startWidth = effectiveWidth
        dragWidth = startWidth
    }
    onPositionChanged: function(mouse) {
        if (!pressed) return
        const delta = mapToItem(null, mouse.x, 0).x - startX
        dragWidth = clampWidth(startWidth + (panelOnLeft ? delta : -delta))
    }
    onReleased: {
        if (preferences && dragWidth >= 0) preferences.setPanelWidth(key, Math.round(dragWidth))
        dragWidth = -1
    }
    onCanceled: dragWidth = -1
    onDoubleClicked: {
        dragWidth = -1
        if (preferences) preferences.clearPanelWidth(key)
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: splitter.theme && splitter.theme.radius3 !== undefined ? splitter.theme.radius3 : 8
        anchors.bottomMargin: anchors.topMargin
        width: 2
        radius: 1
        color: splitter.theme ? splitter.theme.accent : "transparent"
        opacity: splitter.pressed ? 0.8 : splitter.containsMouse ? 0.4 : 0
        Behavior on opacity {
            NumberAnimation { duration: splitter.theme && splitter.theme.durInstant !== undefined ? splitter.theme.durInstant : 0 }
        }
    }
}
