import QtQuick
import QtQuick.Controls.Basic

// The app's slider: a 4 px track filled in the accent up to an 18 px accent knob.
Slider {
    id: control

    property QtObject theme

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight + 2 : 34

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 160
        implicitHeight: 4
        width: control.availableWidth
        height: 4
        radius: 2
        color: control.theme ? control.theme.control : "transparent"
        opacity: control.enabled ? 1 : 0.45
        Rectangle {
            width: parent.width * control.visualPosition
            height: parent.height
            radius: parent.radius
            color: control.theme ? control.theme.accent : "transparent"
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: 18
        implicitHeight: 18
        radius: 9
        color: !control.theme ? "transparent"
               : control.pressed ? Qt.darker(control.theme.accent, 1.08) : control.theme.accent
        border.color: control.theme ? control.theme.surfaceRaised : "transparent"
        border.width: 2
        opacity: control.enabled ? 1 : 0.45
    }
}
