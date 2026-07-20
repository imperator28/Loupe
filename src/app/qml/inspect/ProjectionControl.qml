import QtQuick
import QtQuick.Controls

Row {
    id: root

    property QtObject theme
    property string projectionMode: "orthographic"
    signal projectionRequested(string mode)
    spacing: 2

    ButtonGroup {
        id: projectionGroup
        exclusive: true
    }

    ThemedButton {
        ButtonGroup.group: projectionGroup
        theme: root.theme
        text: qsTr("Ortho")
        checkable: true
        checked: root.projectionMode === "orthographic"
        Accessible.name: qsTr("Orthographic projection")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Orthographic projection")
        onClicked: root.projectionRequested("orthographic")
    }

    ThemedButton {
        ButtonGroup.group: projectionGroup
        theme: root.theme
        text: qsTr("Perspective")
        checkable: true
        checked: root.projectionMode === "perspective"
        Accessible.name: qsTr("Perspective projection")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Perspective projection")
        onClicked: root.projectionRequested("perspective")
    }
}
