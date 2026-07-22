import QtQuick
import QtQuick.Controls.Basic

ProgressBar {
    id: control

    property QtObject theme
    readonly property color trackFill: theme ? theme.surfaceSubtle : "transparent"
    readonly property color barFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius1 !== undefined ? theme.radius1 : 4
    readonly property bool travelOk: theme && theme.reducedMotion !== undefined ? !theme.reducedMotion : true
    readonly property int pulseDuration: theme && theme.durIndeterminatePulse !== undefined
                                         ? theme.durIndeterminatePulse : 0
    readonly property int travelDuration: theme && theme.durIndeterminateTravel !== undefined
                                          ? theme.durIndeterminateTravel : 0

    implicitWidth: 160
    implicitHeight: 4

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: control.cornerRadius
        color: control.trackFill
    }

    contentItem: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight

        Rectangle {
            visible: !control.indeterminate
            width: control.visualPosition * parent.width
            height: parent.height
            radius: control.cornerRadius
            color: control.barFill
        }

        Rectangle {
            id: indeterminateBand
            visible: control.indeterminate
            width: parent.width * 0.3
            height: parent.height
            radius: control.cornerRadius
            color: control.barFill

            XAnimator on x {
                running: control.indeterminate && control.visible && control.travelOk
                loops: Animation.Infinite
                from: -indeterminateBand.width
                to: control.width
                duration: control.travelDuration
                easing.type: Easing.Linear
            }

            OpacityAnimator on opacity {
                running: control.indeterminate && control.visible && !control.travelOk
                loops: Animation.Infinite
                from: 0.4
                to: 1.0
                duration: control.pulseDuration
                easing.type: Easing.InOutQuad
            }
        }
    }
}
