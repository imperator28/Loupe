import QtQuick
import QtQuick.Controls.Basic

MenuSeparator {
    id: control

    property QtObject theme

    implicitHeight: 9
    padding: 4

    contentItem: Rectangle {
        implicitHeight: 1
        color: control.theme ? control.theme.border : "#b9c5cb"
    }
}
