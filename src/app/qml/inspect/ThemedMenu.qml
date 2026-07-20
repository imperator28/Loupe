import QtQuick
import QtQuick.Controls.Basic

Menu {
    id: control

    property QtObject theme
    readonly property color fill: theme ? theme.surfaceRaised : "#ffffff"
    readonly property color outline: theme ? theme.border : "#b9c5cb"

    implicitWidth: Math.max(180, contentItem.implicitWidth + leftPadding + rightPadding)
    topPadding: 4
    bottomPadding: 4
    leftPadding: 1
    rightPadding: 1

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: contentHeight > control.availableHeight
        currentIndex: control.currentIndex
    }

    background: Rectangle {
        radius: 4
        color: control.fill
        border.color: control.outline
        border.width: 1
    }
}
