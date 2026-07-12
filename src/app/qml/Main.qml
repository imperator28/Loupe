import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

ApplicationWindow {
    id: root
    width: 1280
    height: 800
    visible: true
    title: qsTr("Loupe")
    property ApplicationController controller: ApplicationController {}

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 12

            Label {
                text: qsTr("Loupe")
                font.bold: true
            }
            TabBar {
                id: workspaceSwitcher
                currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1
                onCurrentIndexChanged: root.controller.setWorkspace(currentIndex === 0 ? AppState.Inspect : AppState.Export)
                TabButton { text: qsTr("Inspect") }
                TabButton { text: qsTr("Export") }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: qsTr("Bounds · mm")
                Accessible.name: qsTr("Review source units")
                onClicked: unitReview.open()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1

        Item {
            RowLayout {
                anchors.fill: parent
                Rectangle {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: "#101418"
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("Full assembly")
                        color: "#e6edf3"
                    }
                }
                Rectangle {
                    Layout.preferredWidth: 300
                    Layout.fillHeight: true
                    color: "#182027"
                    Label {
                        anchors.centerIn: parent
                        text: root.controller.inspectorMode === AppState.Document ? qsTr("Document properties") : qsTr("Component properties")
                        color: "#e6edf3"
                    }
                }
            }
        }
        Item {
            Label {
                anchors.centerIn: parent
                text: qsTr("Export workspace")
            }
        }
    }

    Popup {
        id: unitReview
        modal: true
        anchors.centerIn: Overlay.overlay
        padding: 20
        contentItem: Label { text: qsTr("Review source units before export.") }
    }
}
