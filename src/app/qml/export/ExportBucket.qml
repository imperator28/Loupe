import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

Inspect.ElevatedPanel {
    id: root

    property QtObject draft
    readonly property color foreground: theme ? theme.foreground : "transparent"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing2

        Inspect.PanelTitle {
            theme: root.theme
            text: qsTr("Export bucket")
            detail: !root.draft ? "" : root.draft.fileCount === 1 ? qsTr("1 file") : qsTr("%1 files").arg(root.draft.fileCount)
        }

        ListView {
            id: bucketList
            property string draggedNodeId: ""
            property int draggedFromIndex: -1
            property int insertionIndex: -1
            property int insertionOwnerIndex: -1

            function updateInsertionAt(viewY) {
                if (count === 0) return
                const rowStride = 84 + spacing
                const contentPosition = contentY + Math.max(0, Math.min(height - 1, viewY))
                const rowIndex = Math.max(0, Math.min(count - 1,
                                                      Math.floor(contentPosition / rowStride)))
                const after = contentPosition - rowIndex * rowStride >= 42
                insertionOwnerIndex = rowIndex
                insertionIndex = rowIndex + (after ? 1 : 0)
            }

            function finishDrop() {
                if (!root.draft || draggedNodeId.length === 0 || insertionIndex < 0) return
                const nodeId = draggedNodeId
                let targetIndex = insertionIndex
                if (targetIndex > draggedFromIndex) --targetIndex
                targetIndex = Math.max(0, Math.min(count - 1, targetIndex))
                clearDrag()
                root.draft.moveBucketItemById(nodeId, targetIndex)
            }

            function clearDrag() {
                draggedNodeId = ""
                draggedFromIndex = -1
                insertionIndex = -1
                insertionOwnerIndex = -1
            }

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            interactive: draggedNodeId.length === 0
            model: root.draft ? root.draft.bucketRows : []
            reuseItems: true

            delegate: Rectangle {
                id: bucketRow
                required property var modelData
                required property int index
                property bool filenameEdited: false
                width: ListView.view.width
                height: 84
                color: root.theme ? root.theme.surfaceSubtle : "transparent"
                border.color: modelData.error.length > 0
                              ? root.theme.error : root.theme.border
                radius: root.theme.radius1

                Rectangle {
                    visible: bucketList.draggedNodeId.length > 0
                             && bucketList.insertionOwnerIndex === bucketRow.index
                    x: 2
                    y: bucketList.insertionIndex === bucketRow.index + 1
                       ? bucketRow.height - height : 0
                    width: bucketRow.width - 4
                    height: 3
                    radius: 1
                    color: root.theme ? root.theme.accent : "transparent"
                    z: 4
                }

                // Hovering a queued row previews it, the same interaction the component tree
                // offers. The bucket is where a file is checked before export, so it is
                // exactly where seeing the part matters most.
                HoverHandler {
                    onHoveredChanged: {
                        if (!root.draft) return
                        if (hovered) root.draft.hoveredNodeId = bucketRow.modelData.nodeId
                        else if (root.draft.hoveredNodeId === bucketRow.modelData.nodeId)
                            root.draft.hoveredNodeId = ""
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 5

                    Item {
                        id: dragHandle
                        Layout.preferredWidth: 24
                        Layout.fillHeight: true
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Drag %1 to reorder").arg(bucketRow.modelData.name)
                        activeFocusOnTab: true

                        Inspect.ToolIcon {
                            anchors.centerIn: parent
                            name: "grip"
                            color: root.theme ? root.theme.muted : "transparent"
                        }

                        MouseArea {
                            id: handleMouse
                            anchors.fill: parent
                            preventStealing: true
                            enabled: !root.draft || !root.draft.exporting
                            onPressed: {
                                bucketList.draggedNodeId = bucketRow.modelData.nodeId
                                bucketList.draggedFromIndex = bucketRow.index
                                bucketList.insertionIndex = bucketRow.index
                                bucketList.insertionOwnerIndex = bucketRow.index
                            }
                            onPositionChanged: function(mouse) {
                                if (!pressed) return
                                const viewPoint = bucketList.mapFromItem(dragHandle, mouse.x, mouse.y)
                                bucketList.updateInsertionAt(viewPoint.y)
                            }
                            onReleased: {
                                bucketList.finishDrop()
                            }
                            onCanceled: {
                                bucketList.clearDrag()
                            }
                        }
                    }

                    Column {
                        spacing: 2
                        Inspect.IconButton {
                            theme: root.theme
                            glyph: "chevronUp"
                            implicitWidth: 24
                            implicitHeight: 24
                            cornerRadius: root.theme.radius1
                            enabled: bucketRow.index > 0
                                     && (!root.draft || !root.draft.exporting)
                            onClicked: root.draft.moveBucketItemById(bucketRow.modelData.nodeId,
                                                                    bucketRow.index - 1)
                            Inspect.ThemedToolTip {
                                theme: root.theme
                                text: qsTr("Move earlier")
                                visible: parent.hovered
                            }
                        }
                        Inspect.IconButton {
                            theme: root.theme
                            glyph: "chevronDown"
                            implicitWidth: 24
                            implicitHeight: 24
                            cornerRadius: root.theme.radius1
                            enabled: bucketRow.index + 1 < bucketList.count
                                     && (!root.draft || !root.draft.exporting)
                            onClicked: root.draft.moveBucketItemById(bucketRow.modelData.nodeId,
                                                                    bucketRow.index + 1)
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
                            id: sourceNameLabel
                            Layout.fillWidth: true
                            // Copies of one part share a single file; the count says how many
                            // checked components that file stands for.
                            text: (bucketRow.modelData.copyCount || 1) > 1
                                  ? qsTr("%1 ×%2").arg(bucketRow.modelData.name).arg(bucketRow.modelData.copyCount)
                                  : bucketRow.modelData.name
                            color: root.foreground
                            elide: Text.ElideRight
                            font.bold: true
                            font.pixelSize: 12
                            Accessible.name: qsTr("Source component %1").arg(text)

                            HoverHandler {
                                id: sourceNameHover
                            }
                            Inspect.ThemedToolTip {
                                theme: root.theme
                                text: qsTr("Original component: %1").arg(bucketRow.modelData.name)
                                visible: sourceNameHover.hovered
                            }
                        }
                        // Only the name is edited; the suffix the format gives it (.step, .stl)
                        // stays put after it, so renaming never means typing it again.
                        Inspect.ThemedTextField {
                            id: filenameEditor
                            objectName: "bucketFilename-" + bucketRow.modelData.nodeId
                            theme: root.theme
                            Layout.fillWidth: true
                            readonly property string suffix: {
                                const match = bucketRow.modelData.filename.match(/\.(step|stp|stl)$/i)
                                return match ? match[0] : ""
                            }
                            readonly property string stem: suffix.length > 0
                                ? bucketRow.modelData.filename.slice(0, -suffix.length) : bucketRow.modelData.filename
                            rightPadding: suffixLabel.visible ? suffixLabel.implicitWidth + 14 : 8
                            enabled: !root.draft || !root.draft.exporting
                            onTextEdited: bucketRow.filenameEdited = true
                            onEditingFinished: {
                                if (!bucketRow.filenameEdited) return
                                root.draft.setFilenameOverride(bucketRow.modelData.nodeId, text)
                                bucketRow.filenameEdited = false
                            }
                            Accessible.name: qsTr("Output filename for %1").arg(bucketRow.modelData.name)
                            Label {
                                id: suffixLabel
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                visible: filenameEditor.suffix.length > 0
                                text: filenameEditor.suffix
                                color: root.theme ? root.theme.muted : "transparent"
                            }
                        }
                        // The name shown follows the plan except while it is being typed in.
                        // RestoreNone: restoring on focus put back the field's first, empty
                        // text, so clicking in to rename cleared the name.
                        Binding {
                            target: filenameEditor
                            property: "text"
                            value: filenameEditor.stem
                            when: !filenameEditor.activeFocus
                            restoreMode: Binding.RestoreNone
                        }
                        Label {
                            Layout.fillWidth: true
                            text: bucketRow.modelData.error.length > 0
                                  ? bucketRow.modelData.error : bucketRow.modelData.status
                            color: bucketRow.modelData.error.length > 0
                                   ? root.theme.error : root.theme.muted
                            elide: Text.ElideRight
                            font.pixelSize: 11
                        }
                    }

                    Inspect.IconButton {
                        theme: root.theme
                        glyph: "close"
                        enabled: !root.draft || !root.draft.exporting
                        onClicked: root.draft.removeBucketRow(bucketRow.modelData.nodeId)
                        Inspect.ThemedToolTip {
                            theme: root.theme
                            text: qsTr("Remove from export bucket")
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
            visible: root.draft && root.draft.checkedCount === 0
            text: qsTr("Check components to add standalone files")
            color: root.theme ? root.theme.muted : "transparent"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
    }
}
