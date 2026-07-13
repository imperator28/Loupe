import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

Item {
    id: root
    property QtObject controller
    property string activeTask: ""

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
            onToolTriggered: function(tool) {
                if (tool === "section" || tool === "measure" || tool === "capture") root.activeTask = tool
            }
        }
    }

    Loader {
        id: taskPanelLoader
        anchors.horizontalCenter: viewport.horizontalCenter
        anchors.bottom: viewport.bottom
        anchors.bottomMargin: 96
        active: root.activeTask !== ""
        source: root.activeTask === "measure" ? "MeasurementPanel.qml"
              : root.activeTask === "section" ? "SectionPanel.qml"
              : "CapturePanel.qml"
        onLoaded: {
            if (root.activeTask === "measure") item.taskController = root.controller.measurement
            else if (root.activeTask === "section") item.taskController = root.controller.section
            else item.taskController = root.controller.capture
            item.closeRequested.connect(function() { root.activeTask = "" })
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
