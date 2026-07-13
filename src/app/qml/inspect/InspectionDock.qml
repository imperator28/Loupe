import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    signal toolTriggered(string tool)
    property var tools: [
        { key: "fit", label: qsTr("Fit") }, { key: "isolate", label: qsTr("Isolate") },
        { key: "section", label: qsTr("Section") }, { key: "measure", label: qsTr("Measure") },
        { key: "ghost", label: qsTr("Ghost") }, { key: "capture", label: qsTr("Capture") }
    ]
    implicitHeight: 56
    radius: 12
    color: "#182027"
    border.color: "#34414b"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4
        Repeater {
            model: root.tools
            delegate: ToolButton {
                required property var modelData
                text: modelData.label
                Layout.minimumWidth: 44
                Layout.minimumHeight: 44
                ToolTip.visible: hovered
                ToolTip.text: modelData.label
                Accessible.name: modelData.label
                onClicked: root.toolTriggered(modelData.key)
            }
        }
    }
}
