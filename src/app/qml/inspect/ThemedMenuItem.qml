import QtQuick
import QtQuick.Controls.Basic

MenuItem {
    id: control

    property QtObject theme
    readonly property color normalTextColor: theme ? theme.foreground : "transparent"
    readonly property color disabledTextColor: theme ? theme.muted : "transparent"
    readonly property color accentFill: theme ? theme.accent : "transparent"
    readonly property color accentForegroundColor: theme && theme.accentForeground !== undefined ? theme.accentForeground : normalTextColor
    readonly property color highlightedTextColor: accentForegroundColor
    readonly property int cornerRadius: theme && theme.radius1 !== undefined ? theme.radius1 : 4

    implicitWidth: Math.max(180, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: 28
    leftPadding: checkable ? 34 : 12
    rightPadding: subMenu ? 28 : 12

    indicator: Item {
        x: 10
        y: (control.height - height) / 2
        width: 16
        height: 16
        visible: control.checkable

        Rectangle {
            anchors.fill: parent
            radius: control.cornerRadius
            color: control.checked && !control.highlighted && control.theme ? control.theme.accent : "transparent"
            border.color: control.checked ? "transparent"
                          : control.highlighted ? control.accentForegroundColor
                          : control.enabled
                            ? (control.theme && control.theme.borderStrong !== undefined ? control.theme.borderStrong
                                                                                        : control.theme ? control.theme.border : "transparent")
                            : control.disabledTextColor
            border.width: control.checked ? 0 : 1
        }

        Text {
            anchors.centerIn: parent
            text: control.checked ? "✓" : ""
            color: control.highlighted ? control.accentForegroundColor
                                       : (control.theme && control.theme.accentForeground !== undefined ? control.theme.accentForeground : control.normalTextColor)
            font.bold: true
            font.pixelSize: 12
        }
    }

    arrow: Text {
        x: control.width - width - 10
        y: (control.height - height) / 2
        visible: control.subMenu
        text: "›"
        font.pixelSize: 16
        color: control.highlighted ? control.highlightedTextColor : control.normalTextColor
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
        radius: control.cornerRadius
        color: control.highlighted ? control.accentFill : "transparent"
    }
}
