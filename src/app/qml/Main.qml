import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App
import "inspect" as Inspect

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
            Button {
                text: qsTr("Open STEP…")
                Accessible.name: qsTr("Open a STEP file")
                onClicked: openStepDialog.open()
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
                text: qsTr("Units: %1").arg(root.controller.effectiveUnit)
                Accessible.name: qsTr("Review source units")
                onClicked: unitReview.open()
            }
        }
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.controller.workspace === AppState.Inspect ? 0 : 1

        Inspect.InspectWorkspace { controller: root.controller }
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

    OpenStepDialog {
        id: openStepDialog
        onFileSelected: path => root.controller.openFile(path)
    }
}
