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

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Export bucket")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
            Label {
                text: root.draft ? root.draft.checkedCount : 0
                color: root.theme ? root.theme.muted : "transparent"
            }
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

                        Label {
                            anchors.centerIn: parent
                            text: "\u22ee\u22ee"
                            color: root.theme ? root.theme.muted : "transparent"
                            font.pixelSize: 18
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
                        Inspect.ThemedToolButton {
                            theme: root.theme
                            text: "\u2191"
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
                        Inspect.ThemedToolButton {
                            theme: root.theme
                            text: "\u2193"
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
                            text: bucketRow.modelData.name
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
                        Inspect.ThemedTextField {
                            id: filenameEditor
                            theme: root.theme
                            Layout.fillWidth: true
                            enabled: !root.draft || !root.draft.exporting
                            onTextEdited: bucketRow.filenameEdited = true
                            onEditingFinished: {
                                if (!bucketRow.filenameEdited) return
                                root.draft.setFilenameOverride(bucketRow.modelData.nodeId, text)
                                bucketRow.filenameEdited = false
                            }
                            Accessible.name: qsTr("Output filename for %1").arg(bucketRow.modelData.name)
                        }
                        Binding {
                            target: filenameEditor
                            property: "text"
                            value: bucketRow.modelData.filename
                            when: !filenameEditor.activeFocus
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

                    Inspect.ThemedToolButton {
                        theme: root.theme
                        text: "\u00d7"
                        enabled: !root.draft || !root.draft.exporting
                        onClicked: root.draft.setChecked(bucketRow.modelData.nodeId, false)
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
