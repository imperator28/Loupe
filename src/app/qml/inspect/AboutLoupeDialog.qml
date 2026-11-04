import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

// About Loupe, as desktop apps commonly lay it out: the icon, name and version (with the
// release channel when it is not a stable release), the actions about this version --
// its release notes, What's New, checking for a newer one -- and quiet links to the
// project and its licences. Copy Version Info is for bug reports.
Dialog {
    id: root

    objectName: "aboutLoupePopup"
    property QtObject theme
    property string version: ""
    signal whatsNewRequested()

    readonly property string repository: "https://github.com/imperator28/Loupe"
    // Beta and release-candidate builds say so; a stable release needs no tag.
    readonly property string channel: version.indexOf("-beta") >= 0 ? qsTr("Beta")
                                    : version.indexOf("-rc") >= 0 ? qsTr("Release candidate") : ""
    readonly property string releaseUrl: version.length > 0 ? repository + "/releases/tag/v" + version
                                                            : repository + "/releases"

    modal: true
    dim: false
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(400, Overlay.overlay.width - root.theme.spacing4 * 2)
    padding: 0
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null


    background: Rectangle {
        radius: root.theme.radius4
        color: root.theme.surface3
        border.color: root.theme.border
    }

    // A quiet text link: accent, underlined on hover.
    component LinkLabel: Label {
        id: link
        property string url
        color: root.theme.accent
        font.pixelSize: root.theme.fontCaption
        font.underline: linkHover.hovered
        HoverHandler { id: linkHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: Qt.openUrlExternally(link.url) }
        Accessible.role: Accessible.Link
    }

    // For Copy Version Info: the clipboard is reached through a text field.
    TextEdit { id: clipboardHelper; visible: false }

    contentItem: ColumnLayout {
        spacing: 0

        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 32
            Layout.preferredWidth: 88
            Layout.preferredHeight: 88
            source: "qrc:/branding/loupe-app-icon.png"
            sourceSize: Qt.size(88, 88)
            fillMode: Image.PreserveAspectFit
            mipmap: true
            smooth: true
            Accessible.name: qsTr("Loupe app icon")
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 14
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Loupe")
            font.bold: true
            font.pixelSize: Math.round(root.theme.fontTitle * 1.5)
            color: root.theme.foreground
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 4
            spacing: 8
            Label {
                objectName: "aboutVersion"
                text: root.version.length > 0 ? qsTr("Version %1").arg(root.version) : qsTr("Version unknown")
                color: root.theme.muted
            }
            // The channel as a small tag, the one place a pill belongs (design.md status tags).
            Rectangle {
                visible: root.channel.length > 0
                implicitWidth: channelLabel.implicitWidth + 14
                implicitHeight: channelLabel.implicitHeight + 4
                radius: height / 2
                color: root.theme.accentTint
                Label {
                    id: channelLabel
                    anchors.centerIn: parent
                    text: root.channel
                    color: root.theme.accent
                    font.pixelSize: root.theme.fontCaption
                    font.bold: true
                }
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.leftMargin: 32
            Layout.rightMargin: 32
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("A STEP inspector for fast CAD review. Designed by Jiyu in 2026.")
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
        }

        // This version: what changed, and whether there is a newer one.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.leftMargin: 32
            Layout.rightMargin: 32
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ThemedButton {
                    objectName: "aboutReleaseNotes"
                    theme: root.theme
                    Layout.fillWidth: true
                    text: qsTr("Release Notes")
                    onClicked: Qt.openUrlExternally(root.releaseUrl)
                }
                ThemedButton {
                    objectName: "aboutWhatsNew"
                    theme: root.theme
                    Layout.fillWidth: true
                    enabled: Updates.whatsNewItems.length > 0
                    text: qsTr("What's New")
                    onClicked: { root.close(); root.whatsNewRequested() }
                }
            }
            ThemedButton {
                objectName: "aboutCheckForUpdates"
                theme: root.theme
                Layout.fillWidth: true
                enabled: Updates.supported && Updates.state !== Updates.Checking
                         && Updates.state !== Updates.Downloading && Updates.state !== Updates.Installing
                text: qsTr("Check for Updates")
                onClicked: { Updates.checkForUpdates(true); root.close() }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 20
            spacing: 14
            LinkLabel { text: qsTr("All releases"); url: root.repository + "/releases" }
            LinkLabel { text: qsTr("Source on GitHub"); url: root.repository }
            LinkLabel { text: qsTr("Licences"); url: root.repository + "/blob/master/THIRD_PARTY_NOTICES.md" }
        }
        Label {
            Layout.fillWidth: true
            Layout.topMargin: 10
            Layout.leftMargin: 32
            Layout.rightMargin: 32
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Built with Qt and Open CASCADE Technology. Apache License 2.0.")
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 24
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            ThemedButton {
                objectName: "aboutCopyVersion"
                theme: root.theme
                text: copiedTimer.running ? qsTr("Copied") : qsTr("Copy Version Info")
                onClicked: {
                    clipboardHelper.text = qsTr("Loupe %1 (%2)").arg(root.version).arg(Qt.platform.os)
                    clipboardHelper.selectAll()
                    clipboardHelper.copy()
                    copiedTimer.restart()
                }
                Timer { id: copiedTimer; interval: 1500 }
            }
            Item { Layout.fillWidth: true }
            ThemedButton {
                theme: root.theme
                primary: true
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
