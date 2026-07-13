import QtQuick
import QtQuick.Dialogs

FileDialog {
    id: root
    signal fileSelected(url path)
    title: qsTr("Open STEP assembly")
    nameFilters: [qsTr("STEP files (*.step *.stp)")]
    onAccepted: root.fileSelected(selectedFile)
}
