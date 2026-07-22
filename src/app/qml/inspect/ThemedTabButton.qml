import QtQuick
import QtQuick.Controls.Basic

TabButton {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    readonly property color activeFill: theme && theme.dark && theme.surface3 !== undefined ? theme.surface3
                                                                                            : theme ? theme.surfaceRaised : "transparent"
    readonly property color hoverFill: theme ? theme.surfaceSubtle : "transparent"
    readonly property color outline: theme ? theme.border : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    leftPadding: 12
    rightPadding: 12
    topPadding: 4
    bottomPadding: 4

    contentItem: Label {
        text: control.text
        color: !control.enabled ? control.subduedForeground
               : control.checked ? control.foreground
                                 : control.subduedForeground
        font.weight: control.checked ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Item {
        implicitHeight: control.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.checked ? control.activeFill
                   : control.hovered ? control.hoverFill
                                     : "transparent"
            border.color: control.checked ? control.outline : "transparent"
            border.width: 1
            opacity: control.enabled ? 1 : 0.45

            Behavior on color {
                ColorAnimation { duration: control.fadeDuration }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -(control.ringWidth + 1)
            radius: control.cornerRadius + control.ringWidth + 1
            color: "transparent"
            border.color: control.accentFill
            border.width: control.ringWidth
            visible: control.visualFocus
        }
    }
}
