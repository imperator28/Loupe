import QtQuick
import QtQuick.Controls.Basic

SpinBox {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color mutedForeground: theme ? theme.muted : "transparent"
    readonly property color fieldFill: theme && theme.dark ? theme.surface : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme && theme.borderPanel !== undefined ? theme.borderPanel
                                                                               : theme ? theme.border : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durInstant !== undefined ? theme.durInstant : 0
    readonly property int ringWidth: theme && theme.focusRingWidth !== undefined ? theme.focusRingWidth : 2
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    implicitHeight: theme && theme.controlHeight !== undefined ? theme.controlHeight + 2 : 32
    leftPadding: 8
    rightPadding: 46

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        color: control.enabled ? control.foreground : control.mutedForeground
        selectionColor: control.theme ? control.theme.selection : "transparent"
        selectedTextColor: control.foreground
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: control.width - width - 4
        y: 4
        width: 20
        height: (control.height - 8) / 2
        radius: control.cornerRadius - 2
        color: control.up.pressed ? (control.theme ? control.theme.surfaceSubtle : "transparent") : "transparent"

        Text {
            anchors.centerIn: parent
            text: "▴"
            color: control.enabled ? control.foreground : control.mutedForeground
            font.pixelSize: 10
        }
    }

    down.indicator: Rectangle {
        x: control.width - width - 4
        y: control.height / 2
        width: 20
        height: (control.height - 8) / 2
        radius: control.cornerRadius - 2
        color: control.down.pressed ? (control.theme ? control.theme.surfaceSubtle : "transparent") : "transparent"

        Text {
            anchors.centerIn: parent
            text: "▾"
            color: control.enabled ? control.foreground : control.mutedForeground
            font.pixelSize: 10
        }
    }

    background: Item {
        implicitWidth: 110
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
            x: control.width - 28
            y: 1
            width: 1
            height: control.height - 2
            color: control.theme ? control.theme.border : "transparent"
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
