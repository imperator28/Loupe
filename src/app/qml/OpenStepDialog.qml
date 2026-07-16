import QtQuick
import QtQuick.Dialogs

FileDialog {
    id: root
    property var controller
    title: qsTr("Open STEP assembly")
    nameFilters: [qsTr("STEP files (*.step *.stp)")]
    onAccepted: {
        if (controller) controller.openFile(selectedFile)
    }
}
