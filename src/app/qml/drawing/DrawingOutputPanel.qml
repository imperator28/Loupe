import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../inspect" as Inspect

// Format, destination, and the export action for the whole queue.
ColumnLayout {
    id: root

    property QtObject draft
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"

    spacing: 7

    Label { text: qsTr("Destination"); color: root.theme.muted }
    RowLayout {
        Layout.fillWidth: true
        Inspect.ThemedTextField {
            objectName: "drawingDestination"
            theme: root.theme
            Layout.fillWidth: true
            text: root.draft ? root.draft.destination : ""
            placeholderText: qsTr("Choose a folder")
            onEditingFinished: if (root.draft) root.draft.destination = text
        }
        Inspect.ThemedButton {
            theme: root.theme
            text: qsTr("Browse")
            onClicked: destinationDialog.open()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Format"); color: root.theme.muted }
            Inspect.ThemedComboBox {
                objectName: "drawingFormat"
                theme: root.theme
                Layout.fillWidth: true
                enabled: !root.draft || !root.draft.exporting
                readonly property var formats: ["DXF", "SVG", "PDF"]
                model: formats
                currentIndex: root.draft ? Math.max(0, formats.indexOf(root.draft.format)) : 0
                onActivated: if (root.draft) root.draft.format = formats[currentIndex]
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Reference line"); color: root.theme.muted }
            Inspect.ThemedCheckBox {
                objectName: "drawingFiducial"
                theme: root.theme
                text: qsTr("50 mm fiducial")
                checked: root.draft ? root.draft.includeScaleFiducial : false
                enabled: !root.draft || !root.draft.exporting
                onClicked: if (root.draft) root.draft.setIncludeScaleFiducial(checked)
                Inspect.ThemedToolTip {
                    theme: root.theme
                    visible: parent.hovered
                    // Worth the words: DXF R12 cannot declare its own units, so a measured
                    // reference line is the only in-file proof the scale survived.
                    text: qsTr("Adds a known-length line on its own layer. Measure it at the cutter to confirm the scale.")
                }
            }
        }
    }

    Label {
        Layout.fillWidth: true
        objectName: "drawingPlanError"
        visible: root.draft && root.draft.planError.length > 0
        text: root.draft ? root.draft.planError : ""
        color: root.theme.error
        wrapMode: Text.Wrap
    }

    Inspect.ThemedButton {
        objectName: "drawingExportButton"
        theme: root.theme
        primary: true
        Layout.fillWidth: true
        text: root.draft && root.draft.exporting
              ? qsTr("Writing %1%").arg(Math.round(root.draft.exportProgress * 100))
              : root.draft ? qsTr("Export %1 drawing(s)").arg(root.draft.queueCount)
                           : qsTr("Export drawings")
        enabled: root.draft && root.draft.canExport
        onClicked: root.draft.exportReviewedPlan()
        Inspect.ThemedToolTip {
            theme: root.theme
            visible: parent.hovered
            text: !root.draft ? ""
                  : !root.draft.documentReady ? qsTr("Wait for geometry refinement to finish")
                  : root.draft.canExport ? qsTr("Write and validate every queued drawing")
                  : qsTr("Choose a destination and resolve queue errors")
        }
    }

    Inspect.ThemedProgressBar {
        theme: root.theme
        Layout.fillWidth: true
        visible: root.draft && root.draft.exporting
        from: 0
        to: 1
        value: root.draft ? root.draft.exportProgress : 0
        Accessible.name: root.draft ? root.draft.exportStage : ""
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.draft && root.draft.exporting
        Label {
            Layout.fillWidth: true
            text: root.draft ? root.draft.exportStage : ""
            color: root.theme.muted
            elide: Text.ElideMiddle
        }
        Inspect.ThemedButton {
            theme: root.theme
            text: qsTr("Cancel")
            onClicked: root.draft.cancelExport()
        }
    }

    Label {
        Layout.fillWidth: true
        objectName: "drawingExportSummary"
        visible: root.draft && !root.draft.exporting && root.draft.exportSummary.length > 0
        text: root.draft ? root.draft.exportSummary : ""
        color: root.draft && root.draft.exportSucceeded ? root.theme.accent : root.theme.error
        wrapMode: Text.Wrap
    }

    FolderDialog {
        id: destinationDialog
        title: qsTr("Choose drawing destination")
        onAccepted: if (root.draft) root.draft.setDestinationUrl(selectedFolder)
    }
}
