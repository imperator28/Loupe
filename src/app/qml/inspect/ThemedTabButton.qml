import QtQuick
import QtQuick.Controls.Basic

TabButton {
    id: control

    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"
    readonly property color subduedForeground: theme && theme.dark ? "#aeb8c2" : "#53636c"
    readonly property color activeFill: theme ? theme.selection : "#a8ddd7"
    readonly property color outline: theme ? theme.border : "#b9c5cb"

    implicitHeight: 30
    leftPadding: 10
    rightPadding: 10
    topPadding: 5
    bottomPadding: 5

    contentItem: Label {
        text: control.text
        color: control.enabled ? control.foreground : control.subduedForeground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 4
        color: control.checked ? control.activeFill : "transparent"
        border.color: control.checked ? control.outline : "transparent"
        border.width: 1
        opacity: control.enabled ? 1 : 0.58
    }
}
