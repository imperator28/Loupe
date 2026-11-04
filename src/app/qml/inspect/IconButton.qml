import QtQuick
import QtQuick.Controls.Basic

// An icon-only button: one of ToolIcon's drawn glyphs in a square hover target
// (28 px by default, design.md's icon-only size). Never a text character.
ThemedToolButton {
    id: control

    property string glyph: ""
    property real iconSize: 16
    property color iconColor: enabled ? foreground : subduedForeground

    implicitWidth: 28
    implicitHeight: 28
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    contentItem: Item {
        ToolIcon {
            anchors.centerIn: parent
            width: control.iconSize
            height: control.iconSize
            name: control.glyph
            color: control.iconColor
        }
    }
}
