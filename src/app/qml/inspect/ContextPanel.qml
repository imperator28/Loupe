import QtQuick
import QtQuick.Controls

Rectangle {
    property var controller
    color: "#182027"
    Label {
        anchors.centerIn: parent
        text: controller && controller.activeNodeId !== "" ? qsTr("Component properties") : qsTr("Document properties")
        color: "#e6edf3"
    }
}
