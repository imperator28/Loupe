import QtQuick
import QtQuick.Controls.Basic

ToolButton {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    readonly property color stateLayer: theme && theme.accentTint !== undefined ? theme.accentTint : "transparent"
    readonly property color checkedFill: theme ? theme.selection : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    implicitWidth: Math.max(implicitHeight, contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: 8
    rightPadding: 8
    topPadding: 5
    bottomPadding: 5

    contentItem: Label {
        text: control.text
        color: control.enabled ? control.foreground : control.subduedForeground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Item {
        implicitHeight: control.implicitHeight

        // Persistent selected fill for checkable (toggle) buttons.
        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.checkedFill
            visible: control.checked
            opacity: control.enabled ? 1 : 0.45
        }

        // Hover/press state layer — a translucent tint animated by opacity so
        // the transition is smooth and monotonic (no color interpolation flash)
        // and reads correctly on any surface, flat or gradient.
        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.stateLayer
            opacity: !control.enabled ? 0
                     : control.down ? 1.0
                     : control.hovered ? 0.55
                                       : 0.0

            Behavior on opacity {
                NumberAnimation { duration: control.fadeDuration }
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
