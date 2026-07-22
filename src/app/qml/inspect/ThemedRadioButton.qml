import QtQuick
import QtQuick.Controls.Basic

RadioButton {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color mutedForeground: theme ? theme.muted : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property color accentForegroundColor: theme && theme.accentForeground !== undefined ? theme.accentForeground : foreground
    readonly property color wellFill: theme && theme.dark ? theme.surface : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme && theme.borderPanel !== undefined ? theme.borderPanel
                                                                               : theme ? theme.border : "transparent"
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    spacing: 8
    implicitHeight: Math.max(26, contentItem.implicitHeight)

    indicator: Item {
        implicitWidth: 16
        implicitHeight: 16
        x: control.leftPadding
        y: (control.height - height) / 2

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: control.checked ? control.accentFill : control.wellFill
            border.color: control.checked ? "transparent" : control.outline
            border.width: 1
            opacity: control.enabled ? 1 : 0.45

            Behavior on color {
                ColorAnimation { duration: control.fadeDuration }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 6
                height: 6
                radius: 3
                visible: control.checked
                color: control.accentForegroundColor
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -(control.ringWidth + 1)
            radius: width / 2
            color: "transparent"
            border.color: control.accentFill
            border.width: control.ringWidth
            visible: control.visualFocus
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? control.foreground : control.mutedForeground
        verticalAlignment: Text.AlignVCenter
    }
}
