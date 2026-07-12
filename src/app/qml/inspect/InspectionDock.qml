import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: dock
    implicitHeight: 56
    radius: 12
    color: "#182027"
    border.color: "#34414b"
    property var toolNames: [qsTr("Fit"), qsTr("Isolate"), qsTr("Section"), qsTr("Measure"), qsTr("Ghost"), qsTr("Capture")]

    RowLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4
        Repeater {
            model: dock.toolNames
            delegate: ToolButton {
                required property string modelData
                text: modelData
                Layout.minimumWidth: 44
                Layout.minimumHeight: 44
                ToolTip.visible: hovered
                ToolTip.text: modelData
                Accessible.name: modelData
            }
        }
    }
}
