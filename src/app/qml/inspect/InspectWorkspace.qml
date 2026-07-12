import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

Item {
    property var controller

    Rectangle {
        id: viewport
        anchors.fill: parent
        color: "#101418"
        Label {
            anchors.centerIn: parent
            text: qsTr("Full assembly")
            color: "#e6edf3"
        }
        InspectionDock {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
        }
    }

    ContextPanel {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 300
        controller: parent.controller
    }
}
