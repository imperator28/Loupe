import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Opening history: every file opened on this computer and how long it took, newest
// first, as a table to read or to copy as CSV next to another CAD program's times.
Dialog {
    id: root

    objectName: "openHistoryDialog"
    property QtObject theme
    property QtObject history
    property var formatDuration: function(ms) { return ms }
    readonly property var records: history ? history.records : []

    modal: true
    dim: false
    anchors.centerIn: Overlay.overlay
    width: Math.min(780, Overlay.overlay ? Overlay.overlay.width - 48 : 780)
    height: Math.min(560, Overlay.overlay ? Overlay.overlay.height - 48 : 560)
    padding: 24
    closePolicy: Popup.CloseOnEscape
    header: null
    footer: null
    background: Rectangle {
        radius: root.theme.radius4
        color: root.theme.surface3
        border.color: root.theme.border
    }

    // One column's width: the file takes what is left.
    readonly property var columns: [
        { key: "file", title: qsTr("File"), width: -1 },
        { key: "parts", title: qsTr("Parts"), width: 56 },
        { key: "size", title: qsTr("Size"), width: 72 },
        { key: "viewMs", title: qsTr("Ready to view"), width: 96 },
        { key: "previewMs", title: qsTr("All parts"), width: 80 },
        { key: "fullMs", title: qsTr("Fully detailed"), width: 96 },
        { key: "openedAt", title: qsTr("Opened"), width: 112 }
    ]

    function cellText(record, key) {
        if (key === "file") return record.file
        if (key === "parts") return Number(record.parts).toLocaleString(Qt.locale(), "f", 0)
        if (key === "size") {
            const mb = Number(record.sizeBytes) / (1024 * 1024)
            return mb >= 10 ? qsTr("%1 MB").arg(mb.toFixed(0)) : qsTr("%1 MB").arg(mb.toFixed(1))
        }
        if (key === "openedAt") {
            const date = new Date(record.openedAt)
            return isNaN(date.getTime()) ? "" : Qt.formatDateTime(date, "d MMM, hh:mm")
        }
        return root.formatDuration(Number(record[key]))
    }

    contentItem: ColumnLayout {
        spacing: 12

        Label {
            text: qsTr("Opening History")
            font.bold: true
            font.pixelSize: root.theme.fontTitle
            color: root.theme.foreground
        }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: root.theme.muted
            font.pixelSize: root.theme.fontCaption
            text: qsTr("Every file opened on this computer, newest first. Times run from opening the file: ready to view when the first parts are on screen, all parts when every part is shown, fully detailed when import finishes. Opens from Loupe's cache are marked.")
        }

        // The table: a header row, then the records.
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: root.theme.radius2
            color: root.theme.surfaceRaised
            border.color: root.theme.border
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 32
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    spacing: 8
                    Repeater {
                        model: root.columns
                        delegate: Label {
                            required property var modelData
                            Layout.fillWidth: modelData.width < 0
                            Layout.preferredWidth: modelData.width < 0 ? 120 : modelData.width
                            horizontalAlignment: modelData.key === "file" ? Text.AlignLeft : Text.AlignRight
                            text: modelData.title
                            color: root.theme.muted
                            font.pixelSize: root.theme.fontCaption
                            font.bold: true
                            elide: Text.ElideRight
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.theme.border }

                ListView {
                    id: rows
                    objectName: "openHistoryRows"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    model: root.records
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: rows.contentHeight > rows.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff }

                    Label {
                        anchors.centerIn: parent
                        visible: rows.count === 0
                        color: root.theme.muted
                        text: qsTr("No files opened yet.")
                    }

                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        width: ListView.view.width
                        height: 34
                        color: index % 2 === 1 ? root.theme.surfaceSubtle : "transparent"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 8
                            Repeater {
                                model: root.columns
                                delegate: RowLayout {
                                    required property var modelData
                                    Layout.fillWidth: modelData.width < 0
                                    Layout.preferredWidth: modelData.width < 0 ? 120 : modelData.width
                                    spacing: 6
                                    Label {
                                        Layout.fillWidth: true
                                        horizontalAlignment: parent.modelData.key === "file" ? Text.AlignLeft : Text.AlignRight
                                        text: root.cellText(row.modelData, parent.modelData.key)
                                        color: parent.modelData.key === "file" || parent.modelData.key === "viewMs"
                                               ? root.theme.foreground : root.theme.muted
                                        font.bold: parent.modelData.key === "viewMs"
                                        font.features: { "tnum": 1 }
                                        elide: parent.modelData.key === "file" ? Text.ElideMiddle : Text.ElideRight
                                    }
                                    // Opens from the cache are faster; say so where it matters.
                                    Rectangle {
                                        visible: parent.modelData.key === "file" && row.modelData.cached === true
                                        implicitWidth: cacheLabel.implicitWidth + 10
                                        implicitHeight: cacheLabel.implicitHeight + 2
                                        radius: height / 2
                                        color: root.theme.accentTint
                                        Label {
                                            id: cacheLabel
                                            anchors.centerIn: parent
                                            text: qsTr("cache")
                                            color: root.theme.accent
                                            font.pixelSize: root.theme.fontCaption
                                        }
                                    }
                                }
                            }
                        }
                        ThemedToolTip {
                            theme: root.theme
                            text: row.modelData.path
                            visible: rowHover.hovered
                        }
                        HoverHandler { id: rowHover }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ThemedButton {
                objectName: "openHistoryClear"
                theme: root.theme
                destructive: true
                enabled: root.records.length > 0
                text: qsTr("Clear History")
                onClicked: if (root.history) root.history.clear()
            }
            Item { Layout.fillWidth: true }
            ThemedButton {
                objectName: "openHistoryCopy"
                theme: root.theme
                enabled: root.records.length > 0
                text: copiedTimer.running ? qsTr("Copied") : qsTr("Copy as CSV")
                onClicked: {
                    if (root.history) root.history.copyCsvToClipboard()
                    copiedTimer.restart()
                }
                Timer { id: copiedTimer; interval: 1500 }
            }
            ThemedButton {
                theme: root.theme
                primary: true
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
