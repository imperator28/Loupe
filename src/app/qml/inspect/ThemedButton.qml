import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property QtObject theme
    property bool selectedAppearance: checked
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
    readonly property color fill: theme ? theme.control : "#dde5e9"
    readonly property color hoverFill: theme ? theme.surfaceSubtle : "#e8edf0"
    readonly property color outline: theme ? theme.border : "#b9c5cb"

    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    contentItem: Label {
        text: control.text
        color: control.enabled ? control.foreground : control.subduedForeground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.selectedAppearance ? (control.theme ? control.theme.selection : "#a8ddd7")
                               : control.down ? control.hoverFill : control.hovered ? control.hoverFill : control.fill
        border.color: control.outline
        border.width: 1
        opacity: control.enabled ? 1 : 0.58
    }
}
