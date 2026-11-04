import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

// What's New: after an update, once per release, the release's highlights in the manner of
// a macOS app's welcome sheet -- a large centred title, a column of icon, title and one
// line each, and one Continue button. Help > What's New in Loupe shows it again.
Dialog {
    id: root

    objectName: "whatsNewDialog"
    property QtObject theme
    property QtObject updates: Updates
    readonly property var items: updates ? updates.whatsNewItems : []
    readonly property string version: updates ? updates.whatsNewVersion : ""

    modal: true
    // Qt's own dimmer drew as a grey block on Windows; the window's ModalBackdrop blurs and
    // tints it instead (Main.qml).
    dim: false
    anchors.centerIn: Overlay.overlay
    width: Math.min(460, Overlay.overlay ? Overlay.overlay.width - 48 : 460)
    height: Math.min(implicitHeight, Overlay.overlay ? Overlay.overlay.height - 48 : implicitHeight)
    padding: 0
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    // Dismissed however it goes: not again for this release.
    onClosed: if (updates) updates.markWhatsNewSeen()


    background: Rectangle {
        radius: root.theme ? root.theme.radius4 : 10
        color: root.theme ? root.theme.surface3 : "transparent"
        border.color: root.theme ? root.theme.border : "transparent"
    }

    contentItem: ColumnLayout {
        spacing: 0

        // The title block.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 36
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            spacing: 6
            Label {
                objectName: "whatsNewTitle"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: qsTr("What's New in Loupe")
                color: root.theme ? root.theme.foreground : "transparent"
                font.bold: true
                font.pixelSize: root.theme ? Math.round(root.theme.fontTitle * 1.75) : 26
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                visible: root.version.length > 0
                text: qsTr("Version %1").arg(root.version)
                color: root.theme ? root.theme.muted : "transparent"
            }
        }

        // The highlights; they scroll when the window is short.
        Flickable {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.topMargin: 28
            Layout.preferredHeight: rows.implicitHeight
            clip: true
            contentWidth: width
            contentHeight: rows.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            interactive: contentHeight > height
            ScrollBar.vertical: ScrollBar { policy: list.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

            ColumnLayout {
                id: rows
                width: list.width
                spacing: 20
                Repeater {
                    model: root.items
                    delegate: RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.leftMargin: 44
                        Layout.rightMargin: 44
                        spacing: 18
                        ToolIcon {
                            Layout.alignment: Qt.AlignTop
                            Layout.topMargin: 2
                            width: 30
                            height: 30
                            Layout.preferredWidth: 30
                            Layout.preferredHeight: 30
                            name: parent.modelData.icon
                            strokeWidth: 1.75
                            color: root.theme ? root.theme.accent : "transparent"
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                Layout.fillWidth: true
                                text: parent.parent.modelData.title
                                wrapMode: Text.WordWrap
                                color: root.theme ? root.theme.foreground : "transparent"
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: parent.parent.modelData.body
                                wrapMode: Text.WordWrap
                                color: root.theme ? root.theme.muted : "transparent"
                                lineHeight: 1.15
                            }
                        }
                    }
                }
            }
        }

        ThemedButton {
            objectName: "whatsNewContinue"
            theme: root.theme
            Layout.fillWidth: true
            Layout.topMargin: 32
            Layout.leftMargin: 40
            Layout.rightMargin: 40
            Layout.bottomMargin: 32
            implicitHeight: 36
            primary: true
            text: qsTr("Continue")
            focus: true
            onClicked: root.close()
        }
    }
}
