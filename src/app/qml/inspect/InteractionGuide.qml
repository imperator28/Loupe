import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    objectName: "interactionGuidePopup"
    property QtObject theme
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(640, Overlay.overlay.width - root.theme.spacing4 * 2)
    height: Math.min(560, Overlay.overlay.height - root.theme.spacing4 * 2)
    padding: root.theme.spacing4
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }

    component GuideRow: RowLayout {
        required property string input
        required property string action
        Layout.fillWidth: true
        spacing: root.theme.spacing3

        Label {
            Layout.preferredWidth: 176
            Layout.minimumWidth: 136
            text: parent.input
            color: root.theme.foreground
            font.pixelSize: root.theme.fontCaption
            font.bold: true
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: parent.action
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            wrapMode: Text.Wrap
        }
    }

    contentItem: ColumnLayout {
        width: root.availableWidth
        spacing: root.theme.spacing3

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Interaction")
                color: root.theme.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Navigation, selection, and tool controls")
            color: root.theme.muted
            font.pixelSize: root.theme.fontBody
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: root.theme.spacing4

                GridLayout {
                    Layout.fillWidth: true
                    columns: width >= 520 ? 2 : 1
                    columnSpacing: root.theme.spacing6
                    rowSpacing: root.theme.spacing4

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root.theme.spacing2
                        Label {
                            text: qsTr("Mouse and trackpad")
                            color: root.theme.foreground
                            font.bold: true
                            font.pixelSize: root.theme.fontSection
                        }
                        GuideRow { input: qsTr("Left drag"); action: qsTr("Rotate around the cursor pivot") }
                        GuideRow { input: qsTr("Middle drag"); action: qsTr("Pan") }
                        GuideRow { input: qsTr("Shift + left drag"); action: qsTr("Pan") }
                        GuideRow { input: qsTr("Mouse wheel"); action: qsTr("Zoom") }
                        GuideRow { input: qsTr("Two-finger scroll"); action: qsTr("Pan") }
                        GuideRow { input: qsTr("Pinch"); action: qsTr("Zoom") }
                        GuideRow { input: qsTr("Right click"); action: qsTr("Open context actions") }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: root.theme.spacing2
                        Label {
                            text: qsTr("Selection and view")
                            color: root.theme.foreground
                            font.bold: true
                            font.pixelSize: root.theme.fontSection
                        }
                        GuideRow { input: qsTr("Click a body"); action: qsTr("Select") }
                        GuideRow { input: qsTr("Shift/Ctrl/Cmd + click"); action: qsTr("Add or remove from selection") }
                        GuideRow { input: qsTr("Click the background"); action: qsTr("Clear selection") }
                        GuideRow { input: qsTr("Click a view-cube face"); action: qsTr("Align to a standard view") }
                        GuideRow { input: qsTr("1 / 2 / 3"); action: qsTr("Solid / Solid + Edges / Edges Only") }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: root.theme.border
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: root.theme.spacing2
                    Label {
                        text: qsTr("Keyboard")
                        color: root.theme.foreground
                        font.bold: true
                        font.pixelSize: root.theme.fontSection
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: width >= 520 ? 2 : 1
                        columnSpacing: root.theme.spacing6
                        rowSpacing: root.theme.spacing2
                        GuideRow { input: qsTr("F"); action: qsTr("Fit assembly") }
                        GuideRow { input: qsTr("Shift + F"); action: qsTr("Fit selection") }
                        GuideRow { input: qsTr("I"); action: qsTr("Isolate selection") }
                        GuideRow { input: qsTr("H / Shift + H"); action: qsTr("Hide / Hide others") }
                        GuideRow { input: qsTr("Ctrl/Cmd + Shift + H"); action: qsTr("Show all") }
                        GuideRow { input: qsTr("M / S"); action: qsTr("Measure / Section") }
                        GuideRow { input: qsTr("Esc"); action: qsTr("Cancel a section drag, restore visibility, or close a tool") }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            ThemedButton {
                text: qsTr("Close")
                theme: root.theme
                onClicked: root.close()
            }
        }
    }
}
