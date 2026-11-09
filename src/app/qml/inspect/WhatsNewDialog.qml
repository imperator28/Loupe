import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Loupe.App

// What's New: once after an update, and from Help > What's New in Loupe. The releases are
// listed down the left, newest first -- this one, and the ones before it -- and the chosen
// release's highlights fill the right, a glyph, a title and a line each. Releases since the
// version the user came from (the one the in-app updater replaced) are marked, so an update
// over several releases shows what each brought. Up and Down step through them.
Dialog {
    id: root

    objectName: "whatsNewDialog"
    property QtObject theme
    property QtObject updates: Updates
    readonly property var releases: updates && updates.whatsNewReleases ? updates.whatsNewReleases : []
    property int currentIndex: 0
    readonly property var release: currentIndex >= 0 && currentIndex < releases.length ? releases[currentIndex] : null
    readonly property var items: release ? release.items : []
    readonly property int unseenCount: {
        let count = 0
        for (const entry of releases) if (entry.unseen) ++count
        return count
    }
    // The oldest release since the one updated from: "New since 0.1.4" reads against it.
    readonly property bool showList: releases.length > 1

    modal: true
    // Qt's own dimmer drew as a grey block on Windows; the window's ModalBackdrop blurs and
    // tints it instead (Main.qml).
    dim: false
    anchors.centerIn: Overlay.overlay
    width: Math.min(showList ? 760 : 480, Overlay.overlay ? Overlay.overlay.width - 48 : 760)
    height: Math.min(520, Overlay.overlay ? Overlay.overlay.height - 48 : 520)
    padding: 0
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    onAboutToShow: currentIndex = 0
    // Dismissed however it goes: not again for this release.
    onClosed: if (updates) updates.markWhatsNewSeen()

    function step(delta) {
        const next = Math.max(0, Math.min(releases.length - 1, currentIndex + delta))
        if (next !== currentIndex) currentIndex = next
    }
    function releaseNote(entry) {
        if (!entry) return ""
        if (entry.current) return qsTr("This version")
        if (entry.unseen) return qsTr("New since you updated")
        return qsTr("Earlier")
    }

    background: Rectangle {
        radius: root.theme ? root.theme.radius4 : 10
        color: root.theme ? root.theme.surface3 : "transparent"
        border.color: root.theme ? root.theme.border : "transparent"
    }

    contentItem: FocusScope {
        focus: true
        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)

        RowLayout {
            anchors.fill: parent
            spacing: 0

            // ---- The releases ----
            Rectangle {
                id: sidebar
                objectName: "whatsNewReleaseList"
                visible: root.showList
                Layout.fillHeight: true
                Layout.preferredWidth: Math.round(Math.min(208, root.width * 0.3))
                // One step down from the dialog's surface, and square where it meets the content.
                color: root.theme ? root.theme.surface2 : "transparent"
                radius: root.theme ? root.theme.radius4 : 10
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: parent.radius
                    color: parent.color
                }
                Rectangle {
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    width: 1
                    color: root.theme ? root.theme.border : "transparent"
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: root.theme ? root.theme.spacing6 : 24
                    anchors.bottomMargin: root.theme ? root.theme.spacing4 : 16
                    spacing: root.theme ? root.theme.spacing4 : 16

                    Label {
                        Layout.fillWidth: true
                        Layout.leftMargin: root.theme ? root.theme.spacing4 : 16
                        Layout.rightMargin: root.theme ? root.theme.spacing4 : 16
                        text: qsTr("What's New")
                        color: root.theme ? root.theme.foreground : "transparent"
                        font.pixelSize: root.theme ? root.theme.fontTitle : 15
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.2
                        elide: Text.ElideRight
                    }

                    ListView {
                        id: releaseList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.releases
                        currentIndex: root.currentIndex
                        boundsBehavior: Flickable.StopAtBounds
                        keyNavigationEnabled: false
                        spacing: 2
                        ScrollBar.vertical: ScrollBar { policy: releaseList.contentHeight > releaseList.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }
                        delegate: ItemDelegate {
                            id: releaseRow
                            required property var modelData
                            required property int index
                            readonly property bool selected: index === root.currentIndex
                            objectName: "whatsNewRelease-" + modelData.version
                            width: releaseList.width
                            height: 46
                            leftPadding: (root.theme ? root.theme.spacing4 : 16) + 2
                            rightPadding: root.theme ? root.theme.spacing3 : 12
                            onClicked: root.currentIndex = index
                            Accessible.name: qsTr("Loupe %1, %2").arg(modelData.version).arg(root.releaseNote(modelData))

                            // Selected: the accent tint and a leading accent bar; hovered: the tint
                            // alone, lighter -- as the assembly tree marks its rows.
                            background: Item {
                                Rectangle {
                                    anchors.fill: parent
                                    anchors.leftMargin: root.theme ? root.theme.spacing2 : 8
                                    anchors.rightMargin: root.theme ? root.theme.spacing2 : 8
                                    radius: root.theme ? root.theme.radius1 : 4
                                    color: root.theme ? root.theme.accentTint : "transparent"
                                    opacity: releaseRow.selected ? 1 : releaseRow.hovered ? 0.5 : 0
                                    Behavior on opacity { NumberAnimation { duration: root.theme ? root.theme.durFast : 0 } }
                                }
                                Rectangle {
                                    visible: releaseRow.selected
                                    x: root.theme ? root.theme.spacing2 : 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 2
                                    height: parent.height - 12
                                    radius: 1
                                    color: root.theme ? root.theme.accent : "transparent"
                                }
                            }
                            contentItem: RowLayout {
                                spacing: root.theme ? root.theme.spacing2 : 8
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 1
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTr("Loupe %1").arg(releaseRow.modelData.version)
                                        color: root.theme ? root.theme.foreground : "transparent"
                                        font.pixelSize: root.theme ? root.theme.fontBody : 13
                                        font.weight: releaseRow.selected ? Font.DemiBold : Font.Medium
                                        font.features: { "tnum": 1 }
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: root.releaseNote(releaseRow.modelData)
                                        color: root.theme ? root.theme.muted : "transparent"
                                        font.pixelSize: root.theme ? root.theme.fontCaption : 11
                                        elide: Text.ElideRight
                                    }
                                }
                                // New to this user: a dot in the accent.
                                Rectangle {
                                    objectName: "whatsNewUnseenDot"
                                    visible: releaseRow.modelData.unseen === true
                                    Layout.alignment: Qt.AlignVCenter
                                    width: 7
                                    height: 7
                                    radius: 3.5
                                    color: root.theme ? root.theme.accent : "transparent"
                                }
                            }
                        }
                    }
                }
            }

            // ---- The chosen release ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: root.theme ? root.theme.spacing6 + 4 : 28
                    Layout.leftMargin: 36
                    Layout.rightMargin: 36
                    spacing: 4
                    Label {
                        objectName: "whatsNewTitle"
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: root.release ? qsTr("Loupe %1").arg(root.release.version) : qsTr("What's New in Loupe")
                        color: root.theme ? root.theme.foreground : "transparent"
                        font.pixelSize: root.theme ? Math.round(root.theme.fontTitle * 1.6) : 24
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.4
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: text.length > 0
                        text: root.showList ? root.releaseNote(root.release) : qsTr("What's New in Loupe")
                        color: root.theme ? root.theme.muted : "transparent"
                    }
                }

                // The highlights; they scroll when the window is short.
                Flickable {
                    id: list
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.topMargin: root.theme ? root.theme.spacing6 : 24
                    clip: true
                    contentWidth: width
                    contentHeight: rows.implicitHeight + 8
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: contentHeight > height
                    ScrollBar.vertical: ScrollBar { policy: list.interactive ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                    ColumnLayout {
                        id: rows
                        objectName: "whatsNewItems"
                        width: list.width
                        spacing: 18
                        // A short fade as another release is chosen: what changed is the content.
                        opacity: 1
                        Repeater {
                            model: root.items
                            delegate: RowLayout {
                                id: itemRow
                                required property var modelData
                                Layout.fillWidth: true
                                Layout.leftMargin: 36
                                Layout.rightMargin: 36
                                spacing: 16
                                ToolIcon {
                                    Layout.alignment: Qt.AlignTop
                                    Layout.topMargin: 1
                                    width: 26
                                    height: 26
                                    Layout.preferredWidth: 26
                                    Layout.preferredHeight: 26
                                    name: itemRow.modelData.icon
                                    strokeWidth: 1.75
                                    color: root.theme ? root.theme.accent : "transparent"
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Label {
                                        Layout.fillWidth: true
                                        text: itemRow.modelData.title
                                        wrapMode: Text.WordWrap
                                        color: root.theme ? root.theme.foreground : "transparent"
                                        font.weight: Font.DemiBold
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        Layout.maximumWidth: 460
                                        text: itemRow.modelData.body
                                        wrapMode: Text.WordWrap
                                        color: root.theme ? root.theme.muted : "transparent"
                                        lineHeight: 1.18
                                    }
                                }
                            }
                        }
                    }
                }

                // Older and newer, for those who read through; Continue closes.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: root.theme ? root.theme.spacing4 : 16
                    Layout.leftMargin: 28
                    Layout.rightMargin: 28
                    Layout.bottomMargin: root.theme ? root.theme.spacing6 : 24
                    spacing: root.theme ? root.theme.spacing2 : 8
                    ThemedButton {
                        objectName: "whatsNewOlder"
                        theme: root.theme
                        visible: root.showList
                        enabled: root.currentIndex < root.releases.length - 1
                        flat: true
                        implicitHeight: 32
                        text: root.currentIndex < root.releases.length - 1
                              ? qsTr("Loupe %1").arg(root.releases[root.currentIndex + 1].version) : qsTr("Older")
                        leftPadding: 26
                        ToolIcon {
                            x: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 14
                            height: 14
                            rotation: 180
                            name: "chevronRight"
                            color: root.theme ? root.theme.foreground : "transparent"
                            opacity: parent.enabled ? 1 : 0.4
                        }
                        Accessible.name: qsTr("Older release")
                        onClicked: root.step(1)
                    }
                    Item { Layout.fillWidth: true }
                    ThemedButton {
                        objectName: "whatsNewContinue"
                        theme: root.theme
                        implicitHeight: 32
                        Layout.preferredWidth: 120
                        primary: true
                        text: qsTr("Continue")
                        onClicked: root.close()
                    }
                }
            }
        }
    }

    onCurrentIndexChanged: if (opened) contentFade.restart()
    SequentialAnimation {
        id: contentFade
        NumberAnimation { target: rows; property: "opacity"; to: 0.35; duration: 0 }
        NumberAnimation { target: rows; property: "opacity"; to: 1; duration: root.theme ? root.theme.durStandard : 0; easing.type: Easing.OutCubic }
        ScriptAction { script: list.contentY = 0 }
    }
}
