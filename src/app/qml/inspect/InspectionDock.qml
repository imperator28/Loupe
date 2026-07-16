import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

Rectangle {
    id: root
    signal toolTriggered(string tool)
    property QtObject theme
    property int viewerPresentation: AppState.Full
    property var tools: [
        { key: "fit", label: qsTr("Fit") }, { key: "isolate", label: qsTr("Isolate"), active: viewerPresentation === AppState.Isolate },
        { key: "hide", label: qsTr("Hide") },
        { key: "section", label: qsTr("Section") }, { key: "measure", label: qsTr("Measure") },
        { key: "capture", label: qsTr("Capture") }
    ]
    implicitWidth: controls.implicitWidth + 12
    implicitHeight: 56
    radius: 6
    color: root.theme.surfaceRaised
    border.color: root.theme.border

    RowLayout {
        id: controls
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4
        Repeater {
            model: root.tools
            delegate: ThemedToolButton {
                required property var modelData
                theme: root.theme
                text: modelData.label
                checkable: modelData.key === "isolate"
                checked: modelData.active === true
                palette.buttonText: root.theme.onSurface
                palette.text: root.theme.onSurface
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
