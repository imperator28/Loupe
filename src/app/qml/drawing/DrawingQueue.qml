import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import "../inspect" as Inspect

// The drawing queue: several drawings per part, each its own row.
Inspect.ElevatedPanel {
    id: root

    property QtObject draft
    // drawingId -> the preview object captured when that drawing was queued. Held in the view
    // because a thumbnail is a view concern, and re-projecting one per row would cost a
    // hidden-line run apiece.
    property var previews: ({})
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
                            Accessible.name: qsTr("Move %1 earlier").arg(queueRow.modelData.name)
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
                            Accessible.name: qsTr("Move %1 later").arg(queueRow.modelData.name)
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

                    // Thumbnail of the drawing as it was queued, so a row is identifiable
                    // without selecting it and waiting for a re-projection.
                    Rectangle {
                        id: thumbnail
                        Layout.preferredWidth: 72
                        Layout.preferredHeight: 54
                        color: root.theme ? root.theme.surface : "transparent"
                        border.color: root.theme ? root.theme.border : "transparent"
                        radius: root.theme ? root.theme.radius1 : 4
                        clip: true

                        readonly property var shot: root.previews
                            ? root.previews[queueRow.modelData.drawingId] : null
                        readonly property real shotScale: {
                            if (!shot || shot.empty || !(shot.widthMm > 0) || !(shot.heightMm > 0)) return 0
                            return Math.min((width - 8) / shot.widthMm, (height - 8) / shot.heightMm)
                        }

                        Shape {
                            id: thumbnailShape
                            anchors.centerIn: parent
                            width: thumbnail.shotScale > 0 ? thumbnail.shot.widthMm * thumbnail.shotScale : 0
                            height: thumbnail.shotScale > 0 ? thumbnail.shot.heightMm * thumbnail.shotScale : 0
                            visible: thumbnail.shotScale > 0

                            ShapePath {
                                strokeColor: root.theme ? root.theme.accent : "transparent"
                                strokeWidth: 1
                                fillColor: "transparent"
                                PathMultiline {
                                    paths: root.thumbnailPaths(queueRow.modelData.drawingId,
                                                               thumbnail.shotScale,
                                                               thumbnailShape.height)
                                }
                            }
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: thumbnail.shotScale <= 0
                            text: "—"
                            color: root.theme ? root.theme.muted : "transparent"
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
                            // Keyed on whether the user has actually typed, not on focus.
                            // Gating on focus meant a field that gained focus before the plan
                            // row arrived stayed permanently empty -- clicking it looked like
                            // it had cleared the name.
                            target: filenameEditor
                            property: "text"
                            value: root.baseNameFor(queueRow.modelData.drawingId)
                            when: !queueRow.filenameEdited
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.statusFor(queueRow.modelData.drawingId)
                            // An auto-numbered name is a guess about which drawing is which,
                            // so it reads as a prompt rather than as ordinary status.
                            color: root.errorFor(queueRow.modelData.drawingId).length > 0
                                   ? root.theme.error
                                   : root.autoNumberedFor(queueRow.modelData.drawingId)
                                     ? root.theme.warning : root.theme.muted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                        }
                    }

                    Inspect.ThemedToolButton {
                        objectName: "drawingQueueRemove-" + queueRow.modelData.drawingId
                        theme: root.theme
                        text: "×"
                        Accessible.name: qsTr("Remove %1 from the queue").arg(queueRow.modelData.name)
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

    // Same flattening and the same Y flip the main preview uses, so a thumbnail cannot
    // disagree with the drawing it stands for.
    function thumbnailPaths(drawingId, scale, height) {
        const shot = root.previews ? root.previews[drawingId] : null
        if (!shot || !(scale > 0) || shot.empty) return []
        const result = []
        for (const layer of shot.layers) {
            for (const contour of layer.contours) {
                const points = contour.points
                const polyline = []
                for (let index = 0; index + 1 < points.length; index += 2) {
                    polyline.push(Qt.point((points[index] - shot.minX) * scale,
                                           height - (points[index + 1] - shot.minY) * scale))
                }
                if (polyline.length > 1) result.push(polyline)
            }
        }
        return result
    }

    function autoNumberedFor(drawingId) {
        const row = root.planRowFor(drawingId)
        return row ? row.autoNumbered === true : false
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
