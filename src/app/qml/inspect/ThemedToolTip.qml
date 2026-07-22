import QtQuick
import QtQuick.Controls.Basic

ToolTip {
    id: control

    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color fill: theme && theme.surface3 !== undefined ? theme.surface3
                                                                        : theme ? theme.surfaceRaised : "transparent"
    readonly property color outline: theme ? theme.border : "transparent"
    readonly property int cornerRadius: theme && theme.radius2 !== undefined ? theme.radius2 : 6
    readonly property int fadeDuration: theme && theme.durFast !== undefined ? theme.durFast : 0

    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: -(height + 6)
    delay: 420
    timeout: 7000
    topPadding: 5
    bottomPadding: 5
    leftPadding: 9
    rightPadding: 9

    contentItem: Text {
        text: control.text
        color: control.foreground
        font.pixelSize: control.theme && control.theme.fontCaption !== undefined ? control.theme.fontCaption : 11
        wrapMode: Text.Wrap
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: control.fill
        border.color: control.outline
        border.width: 1
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: control.fadeDuration }
    }

    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Math.round(control.fadeDuration * 0.7) }
    }
}
