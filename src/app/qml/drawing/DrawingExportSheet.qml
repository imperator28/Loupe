import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

// The queue and its export settings, kept out of the main flow.
//
// Configuring a drawing and exporting a batch are different jobs done at different moments.
// Stacking them in one column meant the format, destination, queue and export button sat
// between the user and the next drawing, and pushed the preview off screen. They live here
// instead, behind a queue button, so the workspace stays about the drawing being configured.
Popup {
    id: root

    property QtObject draft
    property QtObject theme
    property var previews: ({})

    readonly property color foreground: theme ? theme.foreground : "transparent"

    modal: true
    focus: true
    // Escape closes it, matching every other transient surface in the app.
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: theme ? theme.spacing3 : 12
    width: Math.min(520, parent ? parent.width - 40 : 520)
    height: Math.min(680, parent ? parent.height - 40 : 680)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? (parent.height - height) / 2 : 0

    background: Rectangle {
        color: root.theme ? root.theme.surfaceRaised : "transparent"
        border.color: root.theme ? root.theme.border : "transparent"
        border.width: 1
        radius: root.theme ? root.theme.radius3 : 8
    }

    contentItem: ColumnLayout {
        spacing: root.theme ? root.theme.spacing3 : 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Drawing queue")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme ? root.theme.fontTitle : 14
            }
            Inspect.ThemedButton {
                objectName: "drawingSheetClose"
                theme: root.theme
                text: qsTr("Close")
                onClicked: root.close()
            }
        }

        DrawingQueue {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            draft: root.draft
            theme: root.theme
            previews: root.previews
        }

        DrawingOutputPanel {
            Layout.fillWidth: true
            draft: root.draft
            theme: root.theme
        }
    }
}
