import QtQuick
import QtQuick.Controls.Basic

// A divider between groups of entries: a hairline inset to the entries' text, with the
// same 4 px above and below it as the menu keeps at its own top and bottom.
MenuSeparator {
    id: control

    property QtObject theme

    // A hidden divider takes no room, like a hidden entry.
    implicitHeight: visible ? 9 : 0
    height: visible ? implicitHeight : 0
    topPadding: 4
    bottomPadding: 4
    leftPadding: 12
    rightPadding: 12

    contentItem: Rectangle {
        implicitHeight: 1
        color: control.theme ? (control.theme.borderPanel !== undefined ? control.theme.borderPanel : control.theme.border)
                             : "transparent"
    }
}
