import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    property QtObject theme
    property bool selectedAppearance: checked
    property bool primary: false
    // Text-only red emphasis for a recoverable destructive action (e.g. a
    // confirm-delete button). Ordinary Cancel is never red — see design.md.
    property bool destructive: false
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    readonly property color fill: theme ? theme.control : "transparent"
    readonly property color outline: theme ? theme.border : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property color accentForegroundColor: theme && theme.accentForeground !== undefined ? theme.accentForeground : foreground
    readonly property color selectionFill: theme ? theme.selection : "transparent"
    readonly property color destructiveForeground: theme ? theme.error : "transparent"
    readonly property color hoverFill: theme && theme.dark ? Qt.lighter(fill, 1.16) : Qt.darker(fill, 1.05)
    readonly property color pressFill: theme && theme.dark ? Qt.lighter(fill, 1.28) : Qt.darker(fill, 1.10)
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight : 30
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    contentItem: Label {
        text: control.text
        color: !control.enabled ? control.subduedForeground
               : control.primary ? control.accentForegroundColor
               : control.destructive ? control.destructiveForeground
                                     : control.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Item {
        implicitHeight: control.implicitHeight

        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.primary
                   ? (control.down ? Qt.darker(control.accentFill, 1.08) : control.accentFill)
                   : control.selectedAppearance ? control.selectionFill
                   : control.down ? control.pressFill
                   : control.hovered ? control.hoverFill
                                     : control.fill
            border.color: control.primary ? "transparent" : control.outline
            border.width: control.primary ? 0 : 1
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
