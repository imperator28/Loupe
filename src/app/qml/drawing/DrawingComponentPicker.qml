import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../inspect" as Inspect

// Part picker for the drawing workspace.
//
// Structurally the Export picker, minus the checkboxes. A checkbox cannot express one part
// queued three times, so a part is chosen here and queued from the setup panel; the row
// shows how many drawings it already has instead of a checked state.
Inspect.ElevatedPanel {
    id: root

    property QtObject controller
    property QtObject draft
    property var collapsedNodeIds: []
    property var componentsById: ({})
    // The part under the cursor, so the 3D view can show it without the user committing to it.
    property string hoveredNodeId: ""
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
        // Row heights collapse to 0 via a property binding, not a model change, and
        // ListView does not always reflow the rows below the toggled one until something
        // forces a relayout -- otherwise leaving a stale gap.
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
                text: qsTr("Parts")
                color: root.foreground
                font.bold: true
                font.pixelSize: root.theme.fontTitle
            }
            Label {
                text: root.draft ? qsTr("%1 queued").arg(root.draft.queueCount) : qsTr("0 queued")
                color: root.theme ? root.theme.muted : "transparent"
            }
        }

        Inspect.ThemedTextField {
            id: componentSearch
            theme: root.theme
            Layout.fillWidth: true
            placeholderText: qsTr("Search parts")
        }

        ListView {
            id: componentList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            // The inter-row gap lives inside each delegate's height, not here:
            // ListView.spacing adds a gap after every model item including ones collapsed
            // to height 0, which accumulates into a large blank span below a collapsed node.
            spacing: 0
            model: root.draft ? root.draft.components : []
            reuseItems: true

            delegate: Item {
                id: componentRow
                objectName: "drawingComponentRow-" + modelData.nodeId
                required property var modelData
                required property int index
                readonly property bool rowSelected: root.draft
                                                    && root.draft.candidateNodeId === modelData.nodeId
                width: ListView.view.width
                height: visible ? 34 : 0
                visible: root.isVisibleInTree(modelData)

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 32
                    color: componentRow.rowSelected ? root.theme.accentTint : "transparent"
                    radius: root.theme.radius1

                    Behavior on color {
                        ColorAnimation { duration: root.theme.durInstant }
                    }

                    Rectangle {
                        visible: componentRow.rowSelected
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 2
                        color: root.theme.accent
                    }

                    HoverHandler {
                        onHoveredChanged: {
                            if (hovered) root.hoveredNodeId = componentRow.modelData.nodeId
                            else if (root.hoveredNodeId === componentRow.modelData.nodeId)
                                root.hoveredNodeId = ""
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 6 + Math.min(10, componentRow.modelData.depth) * 12
                        anchors.rightMargin: 6
                        spacing: root.theme.spacing1

                        Inspect.ThemedToolButton {
                            id: disclosureButton
                            objectName: "drawingComponentDisclosure-" + componentRow.modelData.nodeId
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

                        Label {
                            Layout.fillWidth: true
                            text: componentRow.modelData.name
                            color: componentRow.modelData.exportable ? root.foreground : root.theme.muted
                            elide: Text.ElideRight
                            Accessible.name: qsTr("Draw %1").arg(text)

                            TapHandler {
                                objectName: "drawingComponentTap-" + componentRow.modelData.nodeId
                                onTapped: {
                                    if (!root.draft || !componentRow.modelData.exportable) return
                                    root.draft.candidateNodeId = componentRow.modelData.nodeId
                                    if (root.controller)
                                        root.controller.activeNodeId = componentRow.modelData.nodeId
                                    componentList.currentIndex = componentRow.index
                                }
                            }
                        }

                        // How many drawings this part already has. A count rather than a
                        // checkbox is the whole point: the queue can hold several of one part.
                        Rectangle {
                            visible: componentRow.modelData.drawingCount > 0
                            Layout.preferredWidth: Math.max(20, queuedCount.implicitWidth + 10)
                            Layout.preferredHeight: 18
                            radius: root.theme.radius1
                            color: root.theme.accentTint

                            Label {
                                id: queuedCount
                                anchors.centerIn: parent
                                text: componentRow.modelData.drawingCount
                                color: root.theme.accent
                                font.pixelSize: root.theme.fontCaption
                            }

                            Inspect.ThemedToolTip {
                                theme: root.theme
                                visible: queuedCountHover.hovered
                                text: qsTr("%1 drawing(s) queued for this part")
                                          .arg(componentRow.modelData.drawingCount)
                            }
                            HoverHandler { id: queuedCountHover }
                        }
                    }
                }
            }
            ScrollBar.vertical: ScrollBar {}
        }
    }
}
