import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color mutedForeground: theme ? theme.muted : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property color trackOff: theme ? theme.surfaceSubtle : "transparent"
    readonly property color trackOutline: theme && theme.borderPanel !== undefined ? theme.borderPanel
                                                                                    : theme ? theme.border : "transparent"
    readonly property color thumbFill: theme && theme.switchThumb !== undefined ? theme.switchThumb : foreground
    readonly property int slideDuration: theme && theme.durFast !== undefined ? theme.durFast : 0
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    spacing: 10
    implicitHeight: Math.max(28, contentItem.implicitHeight)

    indicator: Item {
        implicitWidth: 38
        implicitHeight: 22
        x: control.leftPadding
        y: (control.height - height) / 2

        Rectangle {
            id: track
            anchors.fill: parent
            radius: height / 2
            color: control.checked ? control.accentFill : control.trackOff
            border.color: control.checked ? "transparent" : control.trackOutline
            border.width: 1
            opacity: control.enabled ? 1 : 0.45

            Behavior on color {
                ColorAnimation { duration: control.fadeDuration }
            }
        }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            x: control.checked ? parent.width - width - 2 : 2
            y: 2
            color: control.thumbFill
            opacity: control.enabled ? 1 : 0.75

            Behavior on x {
                NumberAnimation {
                    duration: control.slideDuration
                    easing.type: Easing.BezierSpline
                    easing.bezierCurve: control.theme && control.theme.easeMove !== undefined
                                        ? control.theme.easeMove : [0.2, 0.0, 0.0, 1.0, 1.0, 1.0]
                }
            }
        }

        Rectangle {
            anchors.fill: parent
            anchors.margins: -(control.ringWidth + 1)
            radius: height / 2
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
        wrapMode: Text.Wrap
    }
}
