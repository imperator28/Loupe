import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    color: enabled ? foreground : (theme ? theme.muted : "#53636c")
    placeholderTextColor: theme ? theme.muted : "#53636c"
    selectionColor: theme ? theme.selection : "#a8ddd7"
    selectedTextColor: foreground
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground
    implicitHeight: 32
    leftPadding: 8
    rightPadding: 8

    background: Rectangle {
        radius: 4
        color: control.theme ? control.theme.surface : "#f5f7f8"
        border.color: control.activeFocus && control.theme ? control.theme.accent
                                                          : control.theme ? control.theme.border : "#b9c5cb"
        opacity: control.enabled ? 1 : 0.58
    }
}
