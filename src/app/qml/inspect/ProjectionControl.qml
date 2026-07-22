import QtQuick
import QtQuick.Controls

// A single dropdown for camera projection, peer to the display-mode dropdown
// in StepViewport so the two read as one category ("View" controls) rather
// than a button/menu pair with no shared visual language.
ThemedComboBox {
    id: root

    property string projectionMode: "orthographic"
    signal projectionRequested(string mode)

    implicitWidth: 132
    model: [qsTr("Orthographic"), qsTr("Perspective")]
    currentIndex: root.projectionMode === "perspective" ? 1 : 0
    Accessible.name: qsTr("Projection")
    onActivated: index => root.projectionRequested(index === 1 ? "perspective" : "orthographic")

    ThemedToolTip {
        theme: root.theme
        text: qsTr("Projection")
        visible: root.hovered
    }
}
