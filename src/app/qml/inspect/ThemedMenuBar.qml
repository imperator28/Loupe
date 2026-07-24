import QtQuick
import QtQuick.Controls.Basic

MenuBar {
    id: control

    property QtObject theme

    delegate: ThemedMenuBarItem {
        theme: control.theme
    }

    background: Rectangle {
        color: control.theme ? control.theme.surfaceRaised : "transparent"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: control.theme ? control.theme.border : "transparent"
        }
    }
}
