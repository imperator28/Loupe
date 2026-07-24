import QtQuick
import QtQuick.Controls.Basic

MenuBarItem {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color stateLayer: theme && theme.accentTint !== undefined ? theme.accentTint : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    leftPadding: 12
    rightPadding: 12

    contentItem: Text {
        text: control.text
        color: control.foreground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: control.stateLayer
        opacity: control.pressed || control.highlighted ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation { duration: control.fadeDuration }
        }
    }
}
