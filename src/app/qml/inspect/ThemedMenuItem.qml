import QtQuick
import QtQuick.Controls.Basic

MenuItem {
    id: control

    property QtObject theme
    readonly property color normalTextColor: theme ? theme.foreground : "#172127"
    readonly property color disabledTextColor: theme ? theme.muted : "#53636c"
    readonly property color highlightedTextColor: normalTextColor
    readonly property color itemFill: highlighted && theme ? theme.selection : "transparent"

    implicitWidth: Math.max(180, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 34
    leftPadding: checkable ? 34 : 12
    rightPadding: 12

    indicator: Item {
        x: 10
        y: (control.height - height) / 2
        width: 16
        height: 16
        visible: control.checkable

        Rectangle {
            anchors.fill: parent
            radius: 3
            color: control.checked && control.theme ? control.theme.accent : "transparent"
            border.color: control.enabled
                          ? (control.theme ? control.theme.border : "#b9c5cb")
                          : control.disabledTextColor
        }

        Text {
            anchors.centerIn: parent
            text: control.checked ? "\u2713" : ""
            color: control.theme && control.theme.dark ? "#101418" : "#ffffff"
            font.bold: true
            font.pixelSize: 12
        }
    }

    contentItem: Text {
        text: control.text
        color: !control.enabled ? control.disabledTextColor
                                : control.highlighted ? control.highlightedTextColor
                                                      : control.normalTextColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: control.itemFill
    }
}
