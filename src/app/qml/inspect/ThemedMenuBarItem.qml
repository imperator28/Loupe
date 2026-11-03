import QtQuick
import QtQuick.Controls.Basic

MenuBarItem {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color stateLayer: theme && theme.accentTint !== undefined ? theme.accentTint : "transparent"
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

    // Inset from the bar's top and bottom, so the highlight is a rounded chip around the
    // title rather than a slab the bar's full height.
    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            radius: control.theme && control.theme.radius1 !== undefined ? control.theme.radius1 : 4
            color: control.stateLayer
            opacity: control.pressed || control.highlighted ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation { duration: control.fadeDuration }
            }
        }
    }
}
