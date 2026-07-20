import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

Rectangle {
    id: root

    property QtObject controller
    property QtObject draft
    property QtObject theme
    readonly property color foreground: theme && theme.dark ? "#e6edf3" : "#172127"

    function revealNode(nodeId) {
        if (!root.draft || nodeId.length === 0) {
            componentList.currentIndex = -1
            return
        }
        const items = root.draft.components
        for (let row = 0; row < items.length; ++row) {
            if (items[row].nodeId !== nodeId) continue
            if (!items[row].name.toLowerCase().includes(componentSearch.text.toLowerCase())
                    && !items[row].path.toLowerCase().includes(componentSearch.text.toLowerCase())) {
                componentSearch.text = ""
            }
            componentList.currentIndex = row
            componentList.positionViewAtIndex(row, ListView.Center)
            return
        }
    }

    color: theme ? theme.surfaceRaised : "#ffffff"
    border.color: theme ? theme.border : "#b9c5cb"
    radius: 6

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Components")
                color: root.foreground
                font.bold: true
                font.pixelSize: 16
            }
            Label {
                text: root.draft ? qsTr("%1 files").arg(root.draft.checkedCount) : qsTr("0 files")
                color: root.theme ? root.theme.muted : "#53636c"
            }
        }

        Inspect.ThemedTextField {
            id: componentSearch
            theme: root.theme
            Layout.fillWidth: true
            placeholderText: qsTr("Search components")
        }

        ListView {
            id: componentList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 2
            model: root.draft ? root.draft.components : []
            reuseItems: true

            delegate: Rectangle {
                id: componentRow
                required property var modelData
                required property int index
                width: ListView.view.width
                height: visible ? 36 : 0
                visible: modelData.name.toLowerCase().includes(componentSearch.text.toLowerCase())
                         || modelData.path.toLowerCase().includes(componentSearch.text.toLowerCase())
                color: root.draft && root.draft.previewNodeId === modelData.nodeId
                       ? root.theme.selection : "transparent"
                radius: 3

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Math.min(42, componentRow.modelData.depth * 10)
                    anchors.rightMargin: 6
                    spacing: 6

                    CheckBox {
                        checked: {
                            const revision = root.draft ? root.draft.selectionRevision : 0
                            return root.draft && root.draft.isChecked(componentRow.modelData.nodeId)
                        }
                        enabled: componentRow.modelData.exportable
                        onToggled: if (root.draft)
                            root.draft.setChecked(componentRow.modelData.nodeId, checked)
                        Accessible.name: qsTr("Include %1").arg(componentRow.modelData.name)
                    }
                    Label {
                        Layout.fillWidth: true
                        text: componentRow.modelData.name
                        color: root.foreground
                        elide: Text.ElideRight
                    }
                }

                HoverHandler {
                    onHoveredChanged: {
                        if (!root.draft) return
                        if (hovered) root.draft.hoveredNodeId = componentRow.modelData.nodeId
                        else if (root.draft.hoveredNodeId === componentRow.modelData.nodeId) root.draft.hoveredNodeId = ""
                    }
                }
                TapHandler {
                    onTapped: if (root.draft) {
                        root.draft.focusedNodeId = componentRow.modelData.nodeId
                        if (root.controller) root.controller.activeNodeId = componentRow.modelData.nodeId
                        componentList.currentIndex = componentRow.index
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
