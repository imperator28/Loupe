import QtQuick
import QtQuick.Controls.Basic

SpinBox {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color mutedForeground: theme ? theme.muted : "#53636c"
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    implicitHeight: 32
    leftPadding: 8
    rightPadding: 56

    contentItem: TextInput {
        z: 2
        text: control.textFromValue(control.value, control.locale)
        color: control.enabled ? control.foreground : control.mutedForeground
        selectionColor: control.theme ? control.theme.selection : "#a8ddd7"
        selectedTextColor: control.foreground
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: control.width - width
        height: control.height
        width: 28
        color: control.up.pressed ? (control.theme ? control.theme.surfaceSubtle : "#e8edf0") : "transparent"
        border.color: control.theme ? control.theme.border : "#b9c5cb"
        Text { anchors.centerIn: parent; text: "+"; color: control.foreground; font.bold: true }
    }

    down.indicator: Rectangle {
        x: control.width - 56
        height: control.height
        width: 28
        color: control.down.pressed ? (control.theme ? control.theme.surfaceSubtle : "#e8edf0") : "transparent"
        border.color: control.theme ? control.theme.border : "#b9c5cb"
        Text { anchors.centerIn: parent; text: "-"; color: control.foreground; font.bold: true }
    }

    background: Rectangle {
        radius: 4
        color: control.theme ? control.theme.surface : "#f5f7f8"
        border.color: control.activeFocus && control.theme ? control.theme.accent
                                                          : control.theme ? control.theme.border : "#b9c5cb"
        opacity: control.enabled ? 1 : 0.58
    }
}
