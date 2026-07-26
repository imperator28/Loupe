import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

// The drawing queue: several drawings per part, each its own row.
Inspect.ElevatedPanel {
    id: root

    property QtObject draft
    readonly property color foreground: theme ? theme.foreground : "transparent"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing2

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Drawing queue")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
            Label {
                objectName: "drawingQueueCount"
                text: root.draft ? root.draft.queueCount : 0
                color: root.theme ? root.theme.muted : "transparent"
            }
        }

        ListView {
            id: queueList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: root.draft ? root.draft.queue : []
            reuseItems: true

            delegate: Rectangle {
                id: queueRow
                objectName: "drawingQueueRow-" + modelData.drawingId
                required property var modelData
                required property int index
                property bool filenameEdited: false
                width: ListView.view.width
                height: 84
                color: root.theme
                       ? (modelData.selected ? root.theme.accentTint : root.theme.surfaceSubtle)
                       : "transparent"
                border.color: root.theme
                              ? (modelData.selected ? root.theme.accent : root.theme.border)
                              : "transparent"
                radius: root.theme.radius1

                TapHandler {
                    // Selecting a row loads it back into the preview, so what is on screen
                    // is the drawing being looked at.
                    onTapped: if (root.draft) root.draft.selectDrawing(queueRow.modelData.drawingId)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 5

                    ColumnLayout {
                        Layout.preferredWidth: 24
                        Layout.fillHeight: true
                        spacing: 2

                        Inspect.ThemedToolButton {
                            theme: root.theme
                            text: "↑"
                            enabled: queueRow.index > 0 && (!root.draft || !root.draft.exporting)
                            onClicked: root.draft.moveDrawing(queueRow.modelData.drawingId,
                                                              queueRow.index - 1)
                            Inspect.ThemedToolTip {
                                theme: root.theme
                                text: qsTr("Move earlier")
                                visible: parent.hovered
                            }
                        }
                        Inspect.ThemedToolButton {
                            theme: root.theme
                            text: "↓"
                            enabled: queueRow.index + 1 < queueList.count
                                     && (!root.draft || !root.draft.exporting)
                            onClicked: root.draft.moveDrawing(queueRow.modelData.drawingId,
                                                              queueRow.index + 1)
                            Inspect.ThemedToolTip {
                                theme: root.theme
                                text: qsTr("Move later")
                                visible: parent.hovered
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            // Part, view and scale together: without the view a row of three
                            // drawings of one part would read as three identical rows.
                            text: qsTr("%1 · %2 · %3").arg(queueRow.modelData.name)
                                                      .arg(queueRow.modelData.viewLabel)
                                                      .arg(queueRow.modelData.scaleLabel)
                            color: root.foreground
                            elide: Text.ElideRight
                            font.bold: true
                            font.pixelSize: 12
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Inspect.ThemedTextField {
                                id: filenameEditor
                                theme: root.theme
                                Layout.fillWidth: true
                                enabled: !root.draft || !root.draft.exporting
                                onTextEdited: queueRow.filenameEdited = true
                                onEditingFinished: {
                                    if (!queueRow.filenameEdited) return
                                    root.draft.setFilenameOverride(queueRow.modelData.drawingId, text)
                                    queueRow.filenameEdited = false
                                }
                                Accessible.name: qsTr("Output name for %1").arg(queueRow.modelData.name)
                            }
                            // The extension belongs to the chosen format, not to the name. It
                            // was previously inside the editable text, so editing the name and
                            // leaving the extension in place produced "part-top.dxf.dxf".
                            Label {
                                text: root.extensionFor(queueRow.modelData.drawingId)
                                color: root.theme ? root.theme.muted : "transparent"
                                font.pixelSize: 11
                            }
                        }
                        Binding {
                            // Bound only while unfocused, so a live edit is never clobbered
                            // by a model refresh mid-typing.
                            target: filenameEditor
                            property: "text"
                            value: root.baseNameFor(queueRow.modelData.drawingId)
                            when: !filenameEditor.activeFocus
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.statusFor(queueRow.modelData.drawingId)
                            color: root.errorFor(queueRow.modelData.drawingId).length > 0
                                   ? root.theme.error : root.theme.muted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                        }
                    }

                    Inspect.ThemedToolButton {
                        objectName: "drawingQueueRemove-" + queueRow.modelData.drawingId
                        theme: root.theme
                        text: "×"
                        enabled: !root.draft || !root.draft.exporting
                        onClicked: root.draft.removeDrawing(queueRow.modelData.drawingId)
                        Inspect.ThemedToolTip {
                            theme: root.theme
                            text: qsTr("Remove from queue")
                            visible: parent.hovered
                        }
                    }
                }

                onModelDataChanged: filenameEdited = false
            }

            ScrollBar.vertical: ScrollBar {}
        }

        Label {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.draft && root.draft.queueCount === 0
            text: qsTr("Preview a view, then add it to the queue. A part can be queued at several views.")
            color: root.theme ? root.theme.muted : "transparent"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }

    // The plan rows carry filename, status and error; the queue rows carry the
    // configuration. Looked up by drawing ID rather than by index, because a plan error can
    // leave the two lists momentarily different lengths.
    function planRowFor(drawingId) {
        if (!root.draft) return null
        const rows = root.draft.planRows
        for (const row of rows) if (row.drawingId === drawingId) return row
        return null
    }

    function filenameFor(drawingId) {
        const row = root.planRowFor(drawingId)
        return row ? row.filename : ""
    }

    // Split on the last dot only: a part legitimately named "590421-00-01" is full of dots
    // that are not an extension.
    function baseNameFor(drawingId) {
        const name = root.filenameFor(drawingId)
        const dot = name.lastIndexOf(".")
        return dot > 0 ? name.substring(0, dot) : name
    }

    function extensionFor(drawingId) {
        const name = root.filenameFor(drawingId)
        const dot = name.lastIndexOf(".")
        return dot > 0 ? name.substring(dot) : ""
    }

    function statusFor(drawingId) {
        const row = root.planRowFor(drawingId)
        if (!row) return ""
        return row.error.length > 0 ? row.error : row.status
    }

    function errorFor(drawingId) {
        const row = root.planRowFor(drawingId)
        return row ? row.error : ""
    }
}
