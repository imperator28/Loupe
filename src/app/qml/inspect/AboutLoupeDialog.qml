import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    objectName: "aboutLoupePopup"
    property QtObject theme
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, Overlay.overlay.width - root.theme.spacing4 * 2)
    padding: root.theme.spacing4
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    background: ElevatedPanel { theme: root.theme; cornerRadius: root.theme.radius4 }

    contentItem: ColumnLayout {
        width: root.availableWidth
        spacing: root.theme.spacing3

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 92
            Layout.preferredHeight: 92
            source: "qrc:/branding/loupe-app-icon.png"
            sourceSize: Qt.size(92, 92)
            fillMode: Image.PreserveAspectFit
            mipmap: true
            smooth: true
            Accessible.name: qsTr("Loupe app icon")
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Loupe")
            font.bold: true
            font.pixelSize: root.theme.fontTitle + 2
            color: root.theme.foreground
        }
        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Version 0.1.0")
            color: root.theme.muted
            font.pixelSize: root.theme.fontBody
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: root.theme.border
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Designed by Jiyu in 2026")
            color: root.theme.muted
            font.pixelSize: root.theme.fontBody
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Built with Qt and Open CASCADE Technology")
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            wrapMode: Text.Wrap
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
