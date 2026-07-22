import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color fieldFill: theme && theme.dark ? theme.surface : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme && theme.borderPanel !== undefined ? theme.borderPanel
                                                                               : theme ? theme.border : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    color: enabled ? foreground : (theme ? theme.muted : "transparent")
    placeholderTextColor: theme ? theme.muted : "transparent"
    selectionColor: theme ? theme.selection : "transparent"
    selectedTextColor: foreground
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground
    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight + 2 : 32
    leftPadding: 8
    rightPadding: 8

    background: Item {
        implicitHeight: control.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.fieldFill
            border.color: control.activeFocus ? control.accentFill : control.outline
            border.width: 1
            opacity: control.enabled ? 1 : 0.45

            Behavior on border.color {
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
            visible: control.activeFocus
            opacity: 0.55
        }
    }
}
