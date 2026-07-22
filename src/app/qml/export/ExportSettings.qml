import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../inspect" as Inspect

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
                theme: root.theme
                Layout.fillWidth: true
                model: ["STEP", "STL"]
                currentIndex: root.draft && root.draft.format === "STL" ? 1 : 0
                onActivated: if (root.draft) root.draft.format = currentText
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Coordinates"); color: root.theme.muted }
            Inspect.ThemedComboBox {
                theme: root.theme
                Layout.fillWidth: true
                model: [qsTr("Assembly"), qsTr("Local")]
                currentIndex: root.draft && root.draft.coordinates === "Local" ? 1 : 0
                onActivated: if (root.draft) root.draft.coordinates = currentText
            }
        }
    }

    Label { text: qsTr("File naming"); color: root.theme.muted }
    Inspect.ThemedComboBox {
        id: namingMode
        theme: root.theme
        Layout.fillWidth: true
        model: [qsTr("Keep component name"), qsTr("Sequential name"), qsTr("Prefix component name")]
        currentIndex: !root.draft || root.draft.namingStrategy === "keep" ? 0
                      : root.draft.namingStrategy === "sequence" ? 1 : 2
        onActivated: if (root.draft) root.draft.namingStrategy = currentIndex === 1
                     ? "sequence" : currentIndex === 2 ? "prefix" : "keep"
    }
    Inspect.ThemedTextField {
        theme: root.theme
        Layout.fillWidth: true
        visible: namingMode.currentIndex !== 0
        text: root.draft ? root.draft.namingValue : ""
        placeholderText: namingMode.currentIndex === 1 ? qsTr("Base name") : qsTr("Filename prefix")
        onTextEdited: if (root.draft) root.draft.namingValue = text
    }

    Label {
        Layout.fillWidth: true
        visible: root.draft && root.draft.planError.length > 0
        text: root.draft ? root.draft.planError : ""
        color: root.theme.error
        wrapMode: Text.Wrap
    }

    Inspect.ThemedButton {
        theme: root.theme
        primary: true
        Layout.fillWidth: true
        text: root.draft && root.draft.exporting
              ? qsTr("Exporting %1%").arg(Math.round(root.draft.exportProgress * 100))
              : root.draft ? qsTr("Export %1 files").arg(root.draft.checkedCount) : qsTr("Export files")
        enabled: root.draft && root.draft.canExport
        onClicked: root.draft.exportReviewedPlan()
        Inspect.ThemedToolTip {
            theme: root.theme
            visible: parent.hovered
            text: !root.draft ? ""
                  : !root.draft.documentReady ? qsTr("Wait for geometry refinement to finish")
                  : root.draft.canExport ? qsTr("Write and validate the reviewed bucket")
                  : qsTr("Choose a destination and resolve bucket errors")
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
        visible: root.draft && !root.draft.exporting && root.draft.exportSummary.length > 0
        text: root.draft ? root.draft.exportSummary : ""
        color: root.draft && root.draft.exportSucceeded ? root.theme.accent : root.theme.error
        wrapMode: Text.Wrap
    }

    FolderDialog {
        id: destinationDialog
        title: qsTr("Choose export destination")
        onAccepted: if (root.draft) root.draft.setDestinationUrl(selectedFolder)
    }
}
