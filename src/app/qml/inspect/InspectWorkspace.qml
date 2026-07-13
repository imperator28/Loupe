import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

Item {
    id: root
    property QtObject controller
    property string activeTask: ""

    RowLayout {
        anchors.fill: parent
        spacing: 10

        Rectangle {
            id: treePanel
            color: "#182027"
            radius: 8
            Layout.preferredWidth: 248
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 8
                Label {
                    text: qsTr("Assembly tree")
                    color: "#e6edf3"
                    font.bold: true
                    Layout.fillWidth: true
                }
                ListView {
                    id: assemblyTree
                    model: root.controller ? root.controller.assemblyTree : null
                    clip: true
                    reuseItems: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    delegate: ItemDelegate {
                        required property string name
                        required property string stableId
                        required property int definitionQuantity
                        width: assemblyTree.width
                        text: definitionQuantity > 0 ? qsTr("%1  %2×").arg(name).arg(definitionQuantity) : name
                        highlighted: root.controller && root.controller.activeNodeId === stableId
                        onClicked: root.controller.setActiveNodeId(stableId)
                    }
                }
            }
        }

        Rectangle {
            id: viewport
            color: "#101418"
            radius: 8
            Layout.fillWidth: true
            Layout.fillHeight: true
            Loader {
                id: viewportLoader
                anchors.fill: parent
                active: root.controller && root.controller.documentState === AppState.TreeReady
                source: "StepViewport.qml"
                onLoaded: item.controller = root.controller
            }
            Label {
                anchors.centerIn: parent
                visible: !root.controller || root.controller.documentState !== AppState.TreeReady
                text: root.controller && root.controller.documentState === AppState.Opening ? qsTr("Importing assembly…")
                    : root.controller && root.controller.documentState === AppState.TreeReady ? qsTr("Assembly loaded")
                    : qsTr("Open a STEP assembly to inspect it")
                color: "#e6edf3"
            }
            InspectionDock {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 24
                onToolTriggered: function(tool) {
                    if (tool === "fit") root.controller.fitView()
                    else if (tool === "isolate") root.controller.isolateActiveNode()
                    else if (tool === "ghost") root.controller.ghostActiveNode()
                    else if (tool === "section" || tool === "measure" || tool === "capture") root.activeTask = tool
                }
            }
        }

        ContextPanel {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            controller: root.controller
        }
    }

    Loader {
        id: taskPanelLoader
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
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
}
