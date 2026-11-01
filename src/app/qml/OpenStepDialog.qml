import QtQuick
import QtQuick.Dialogs

FileDialog {
    id: root
    // Where the chosen file goes is the window's call: an empty tab, or a new one.
    signal stepFileChosen(url file)
    title: qsTr("Open STEP assembly")
    nameFilters: [qsTr("STEP files (*.step *.stp)")]
    onAccepted: stepFileChosen(selectedFile)
}
