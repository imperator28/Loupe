import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property QtObject taskController
    property QtObject theme
    readonly property color foreground: theme ? theme.foreground : "transparent"
    readonly property color subduedForeground: theme ? theme.muted : "transparent"
    signal closeRequested()
    signal captureRequested()
    implicitWidth: 320
    implicitHeight: content.implicitHeight + 32
    radius: root.theme.radius4
    color: root.theme.surfaceRaised
    border.color: root.theme.border

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Capture PNG"); font.bold: true; Layout.fillWidth: true; color: root.foreground }
            ThemedToolButton { theme: root.theme; text: "×"; Accessible.name: qsTr("Close capture"); onClicked: root.closeRequested() }
        }
        Label { text: qsTr("Background"); color: root.subduedForeground }
        RowLayout {
            Layout.fillWidth: true
            ThemedRadioButton { theme: root.theme; text: qsTr("Transparent"); checked: !root.taskController || root.taskController.transparentBackground; onToggled: if (checked) root.taskController.transparentBackground = true }
            ThemedRadioButton { theme: root.theme; text: qsTr("Viewport solid"); checked: root.taskController && !root.taskController.transparentBackground; onToggled: if (checked) root.taskController.transparentBackground = false }
        }
        Label { text: qsTr("Scale"); color: root.subduedForeground }
        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme ? root.theme.spacing2 : 8
            Repeater {
                model: [2, 3, 5]
                delegate: ThemedButton {
                    required property int modelData
                    theme: root.theme
                    text: qsTr("%1×").arg(modelData)
                    selectedAppearance: root.taskController
                                        && Math.abs(root.taskController.scale - modelData) < 0.001
                    enabled: root.taskController && modelData <= root.taskController.maximumScale
                    Layout.fillWidth: true
                    onClicked: root.taskController.scale = modelData
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            Label { text: qsTr("Custom"); Layout.fillWidth: true; color: root.foreground }
            ThemedSpinBox {
                theme: root.theme
                editable: true
                from: root.taskController ? Math.round(root.taskController.minimumScale * 100) : 1
                to: root.taskController ? Math.floor(root.taskController.maximumScale * 100) : 1600
                stepSize: 1
                value: root.taskController ? Math.round(root.taskController.scale * 100) : 100
                textFromValue: function(value) { return qsTr("%1×").arg((value / 100).toFixed(2)) }
                valueFromText: function(text) { return Number(text.replace("×", "")) * 100 }
                onValueModified: root.taskController.scale = value / 100
            }
        }
        ThemedSwitch {
            theme: root.theme
            text: qsTr("Include active measurement")
            checked: !root.taskController || root.taskController.includeMeasurements
            onToggled: root.taskController.includeMeasurements = checked
        }
        ThemedSwitch {
            theme: root.theme
            text: qsTr("Include section caps")
            checked: !root.taskController || root.taskController.includeSectionCaps
            onToggled: root.taskController.includeSectionCaps = checked
        }
        Label {
            text: root.taskController ? qsTr("Output: %1 × %2 PNG").arg(root.taskController.resolvedWidth).arg(root.taskController.resolvedHeight)
                                      : qsTr("Resolved output dimensions update from the active viewport.")
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.subduedForeground
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: root.theme ? root.theme.spacing2 : 8
            visible: root.taskController && root.taskController.inProgress

            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: root.taskController ? root.taskController.statusMessage : qsTr("Capturing PNG…")
                    elide: Text.ElideRight
                    color: root.foreground
                    font.bold: true
                }
                Label {
                    text: root.taskController ? qsTr("%1%").arg(Math.round(root.taskController.progress * 100)) : ""
                    color: root.subduedForeground
                }
            }
            ThemedProgressBar {
                objectName: "captureProgressBar"
                theme: root.theme
                Layout.fillWidth: true
                value: root.taskController ? root.taskController.progress : 0
            }
        }
        Label {
            Layout.fillWidth: true
            visible: root.taskController && !root.taskController.inProgress
                     && root.taskController.statusMessage.length > 0
            text: root.taskController ? root.taskController.statusMessage : ""
            wrapMode: Text.WordWrap
            color: root.taskController && root.taskController.lastSaveSucceeded ? root.theme.accent : root.theme.error
        }
        ThemedButton {
            theme: root.theme
            text: qsTr("Save PNG…")
            Layout.fillWidth: true
            enabled: root.taskController && !root.taskController.inProgress
                     && root.taskController.resolvedWidth > 0 && root.taskController.resolvedHeight > 0
            onClicked: root.captureRequested()
        }
    }
}
