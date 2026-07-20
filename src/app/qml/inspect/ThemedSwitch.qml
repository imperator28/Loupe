import QtQuick
import QtQuick.Controls.Basic

Switch {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color mutedForeground: theme ? theme.muted : "#53636c"
    palette.text: foreground
    palette.buttonText: foreground
    palette.windowText: foreground

    spacing: 10
    implicitHeight: Math.max(28, contentItem.implicitHeight)

    indicator: Rectangle {
        implicitWidth: 40
        implicitHeight: 22
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? (control.theme ? control.theme.accent : "#087b74")
                               : control.theme ? control.theme.surface : "#f5f7f8"
        border.color: control.theme ? control.theme.border : "#b9c5cb"

        Rectangle {
            width: 16
            height: 16
            radius: 8
            x: control.checked ? parent.width - width - 3 : 3
            y: 3
            color: control.checked && control.theme && control.theme.dark ? control.theme.surface : "#ffffff"
            border.color: control.theme ? control.theme.border : "#b9c5cb"
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
