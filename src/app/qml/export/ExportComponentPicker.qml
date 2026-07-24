import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

Inspect.ElevatedPanel {
    id: root

    property QtObject controller
    property QtObject draft
    property var collapsedNodeIds: []
    property var componentsById: ({})
    readonly property color foreground: theme ? theme.foreground : "transparent"

    function rebuildComponentIndex() {
        const nextIndex = ({})
        const items = root.draft ? root.draft.components : []
        for (const item of items) nextIndex[item.nodeId] = item
        root.componentsById = nextIndex
    }

    function isCollapsed(nodeId) {
        return root.collapsedNodeIds.indexOf(nodeId) >= 0
    }

    function toggleCollapsed(nodeId) {
        if (root.isCollapsed(nodeId))
            root.collapsedNodeIds = root.collapsedNodeIds.filter(function(id) { return id !== nodeId })
        else
            root.collapsedNodeIds = root.collapsedNodeIds.concat([nodeId])
        // Row heights collapse to 0 via a property binding, not a model
        // change, and ListView (especially with reuseItems) does not always
        // reflow rows below the toggled one until something forces a
        // relayout — otherwise leaving a stale gap where the children used
        // to be.
        componentList.forceLayout()
    }

    function expandToNode(nodeId) {
        let current = root.componentsById[nodeId]
        let nextCollapsed = root.collapsedNodeIds
        while (current && current.parentId.length > 0) {
            nextCollapsed = nextCollapsed.filter(function(id) { return id !== current.parentId })
            current = root.componentsById[current.parentId]
        }
        root.collapsedNodeIds = nextCollapsed
        componentList.forceLayout()
    }

    function isVisibleInTree(component) {
        const search = componentSearch.text.trim().toLowerCase()
        if (search.length > 0)
            return component.name.toLowerCase().includes(search)
                || component.path.toLowerCase().includes(search)

        let parentId = component.parentId
        while (parentId && parentId.length > 0) {
            if (root.isCollapsed(parentId)) return false
            const parent = root.componentsById[parentId]
            if (!parent) break
            parentId = parent.parentId
        }
        return true
    }

    function revealNode(nodeId) {
        if (!root.draft || nodeId.length === 0) {
            componentList.currentIndex = -1
            return
        }
        root.expandToNode(nodeId)
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

    Component.onCompleted: root.rebuildComponentIndex()
    onDraftChanged: {
        root.collapsedNodeIds = []
        root.rebuildComponentIndex()
    }

    Connections {
        target: root.draft
        function onComponentsChanged() {
            root.collapsedNodeIds = []
            root.rebuildComponentIndex()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.theme.spacing3
        spacing: root.theme.spacing2 + 2

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Components")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
            Label {
                text: root.draft ? qsTr("%1 files").arg(root.draft.checkedCount) : qsTr("0 files")
                color: root.theme ? root.theme.muted : "transparent"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: root.theme.spacing2

            Inspect.ThemedButton {
                theme: root.theme
                text: qsTr("Select all")
                onClicked: if (root.draft) root.draft.setAllChecked(true)
            }
            Inspect.ThemedButton {
                theme: root.theme
                text: qsTr("Deselect all")
                onClicked: if (root.draft) root.draft.setAllChecked(false)
            }
            Item { Layout.fillWidth: true }
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
            // The 2px inter-row gap lives inside each delegate's own height
            // (below) rather than here. ListView.spacing adds a gap after
            // every model item unconditionally, including ones collapsed to
            // height 0 — a subtree with many hidden descendants would each
            // still contribute that gap, accumulating into a large blank
            // span below a collapsed node.
            spacing: 0
            model: root.draft ? root.draft.components : []
            reuseItems: true

            delegate: Item {
                id: componentRow
                objectName: "exportComponentRow-" + modelData.nodeId
                required property var modelData
                required property int index
                readonly property bool rowFocused: root.draft && root.draft.focusedNodeId === modelData.nodeId
                readonly property bool rowPreviewed: root.draft && root.draft.previewNodeId === modelData.nodeId
                width: ListView.view.width
                // The 2px gap between rows lives here, as empty space above
                // this item's fixed-height visual (below), rather than as
                // ListView.spacing — see the comment on that property.
                height: visible ? 34 : 0
                visible: root.isVisibleInTree(modelData)

                Rectangle {
                    id: componentRowVisual
                    anchors.top: parent.top
                    width: parent.width
                    height: 32
                    color: componentRow.rowFocused || componentRow.rowPreviewed ? root.theme.accentTint : "transparent"
                    opacity: componentRow.rowFocused ? 1 : componentRow.rowPreviewed ? 0.7 : 1
                    radius: root.theme.radius1

                    Behavior on color {
                        ColorAnimation { duration: root.theme.durInstant }
                    }

                    Rectangle {
                        visible: componentRow.rowFocused
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: root.theme.accent
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6 + Math.min(10, componentRow.modelData.depth) * 12
                        anchors.rightMargin: 6
                        spacing: root.theme.spacing1

                        Inspect.ThemedToolButton {
                            id: disclosureButton
                            objectName: "exportComponentDisclosure-" + componentRow.modelData.nodeId
                            theme: root.theme
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 24
                            visible: componentRow.modelData.hasChildren
                            text: root.isCollapsed(componentRow.modelData.nodeId) ? "▸" : "▾"
                            Accessible.name: root.isCollapsed(componentRow.modelData.nodeId)
                                             ? qsTr("Expand %1").arg(componentRow.modelData.name)
                                             : qsTr("Collapse %1").arg(componentRow.modelData.name)
                            onClicked: root.toggleCollapsed(componentRow.modelData.nodeId)
                        }
                        Item {
                            Layout.preferredWidth: disclosureButton.visible ? 0 : 20
                            Layout.preferredHeight: 1
                        }

                        Inspect.ThemedCheckBox {
                            objectName: "exportComponentCheckBox-" + componentRow.modelData.nodeId
                            theme: root.theme
                            checked: {
                                const revision = root.draft ? root.draft.selectionRevision : 0
                                return root.draft && root.draft.isChecked(componentRow.modelData.nodeId)
                            }
                            enabled: componentRow.modelData.exportable
                            onClicked: if (root.draft)
                                root.draft.setChecked(componentRow.modelData.nodeId, checked)
                            Accessible.name: qsTr("Include %1").arg(componentRow.modelData.name)
                        }
                        Label {
                            id: componentName
                            Layout.fillWidth: true
                            text: componentRow.modelData.name
                            color: componentRow.modelData.exportable ? root.foreground : root.theme.muted
                            elide: Text.ElideRight

                            TapHandler {
                                onTapped: if (root.draft) {
                                    root.draft.focusedNodeId = componentRow.modelData.nodeId
                                    if (root.controller) root.controller.activeNodeId = componentRow.modelData.nodeId
                                    componentList.currentIndex = componentRow.index
                                }
                            }
                        }
                    }

                    HoverHandler {
                        onHoveredChanged: {
                            if (!root.draft) return
                            if (hovered) root.draft.hoveredNodeId = componentRow.modelData.nodeId
                            else if (root.draft.hoveredNodeId === componentRow.modelData.nodeId) root.draft.hoveredNodeId = ""
                        }
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
