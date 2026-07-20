import QtQuick
import QtQuick.Controls.Basic

RadioButton {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color mutedForeground: theme ? theme.muted : "#53636c"
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    spacing: 8
    implicitHeight: Math.max(26, contentItem.implicitHeight)

    indicator: Rectangle {
        implicitWidth: 18
        implicitHeight: 18
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 9
        color: control.theme ? control.theme.surface : "#f5f7f8"
        border.color: control.checked && control.theme ? control.theme.accent
                                                       : control.theme ? control.theme.border : "#b9c5cb"
        border.width: control.checked ? 2 : 1

        Rectangle {
            anchors.centerIn: parent
            width: 8
            height: 8
            radius: 4
            visible: control.checked
            color: control.theme ? control.theme.accent : "#087b74"
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? control.foreground : control.mutedForeground
        verticalAlignment: Text.AlignVCenter
    }
}
