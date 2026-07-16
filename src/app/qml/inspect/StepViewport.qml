import QtQuick
import QtQuick.Controls
import QtQuick3D
import Loupe.App

Item {
    id: root
    focus: true

    property QtObject controller
    property QtObject theme
    property var modelByNode: ({})
    property var geometryByNode: ({})
    property var edgeModelByNode: ({})
    property var edgeGeometryByNode: ({})
    property var boundsByNode: ({})
    property string hoveredNodeId: ""
    property int renderMode: 0
    property bool captureInProgress: false
    property bool captureUiHidden: false
    property bool hasSceneBounds: false
    property vector3d sceneMinimum: Qt.vector3d(0, 0, 0)
    property vector3d sceneMaximum: Qt.vector3d(0, 0, 0)
    signal toolRequested(string tool)
    signal openFileRequested()

    ViewportVisualTheme {
        id: viewportVisualTheme
        theme: root.theme
    }

    onControllerChanged: {
        if (controller && view.width > 0 && view.height > 0)
            controller.capture.setViewportSize(view.width, view.height)
    }

    function fitCamera() {
        if (!hasSceneBounds) {
            navigation.reset()
            return
        }
        navigation.fitBounds(sceneMinimum, sceneMaximum)
    }

    function fitSelection() {
        if (!controller || !controller.activeNodeId || !boundsByNode[controller.activeNodeId]) {
            fitCamera()
            return
        }
        const bounds = boundsByNode[controller.activeNodeId]
        navigation.fitBounds(bounds.minimum, bounds.maximum)
    }

    function alignToSection(oppositeSide) {
        if (!controller) return
        const section = controller.section
        const normal = section.usingSelectedPlane ? section.planeNormal
                     : section.axisName === "X" ? Qt.vector3d(1, 0, 0)
                     : section.axisName === "Y" ? Qt.vector3d(0, 1, 0)
                     : Qt.vector3d(0, 0, 1)
        navigation.alignToNormal(oppositeSide ? Qt.vector3d(-normal.x, -normal.y, -normal.z) : normal)
        Qt.callLater(fitVisibleGeometry)
    }

    function setStandardView(normal) {
        navigation.alignToNormal(normal)
        Qt.callLater(fitVisibleGeometry)
    }

    function fitVisibleGeometry() {
        let minimum = Qt.vector3d(0, 0, 0)
        let maximum = Qt.vector3d(0, 0, 0)
        let hasVisibleGeometry = false
        for (let id in geometryByNode) {
            const geometry = geometryByNode[id]
            const currentMinimum = Qt.vector3d(geometry.minimumCoordinate(0), geometry.minimumCoordinate(1), geometry.minimumCoordinate(2))
            const currentMaximum = Qt.vector3d(geometry.maximumCoordinate(0), geometry.maximumCoordinate(1), geometry.maximumCoordinate(2))
            if (!isFinite(currentMinimum.x) || !isFinite(currentMinimum.y) || !isFinite(currentMinimum.z)
                    || !isFinite(currentMaximum.x) || !isFinite(currentMaximum.y) || !isFinite(currentMaximum.z)) continue
            if (!hasVisibleGeometry) {
                minimum = currentMinimum
                maximum = currentMaximum
                hasVisibleGeometry = true
            } else {
                minimum = Qt.vector3d(Math.min(minimum.x, currentMinimum.x), Math.min(minimum.y, currentMinimum.y), Math.min(minimum.z, currentMinimum.z))
                maximum = Qt.vector3d(Math.max(maximum.x, currentMaximum.x), Math.max(maximum.y, currentMaximum.y), Math.max(maximum.z, currentMaximum.z))
            }
        }
        if (hasVisibleGeometry) navigation.fitBounds(minimum, maximum)
    }

    function sectionDragScale() {
        if (!hasSceneBounds || view.height <= 0) return 0.05
        const width = sceneMaximum.x - sceneMinimum.x
        const height = sceneMaximum.y - sceneMinimum.y
        const depth = sceneMaximum.z - sceneMinimum.z
        return Math.max(0.001, Math.sqrt(width * width + height * height + depth * depth) / view.height)
    }

    function includeGeometryBounds(nodeId, geometry) {
        const minimum = Qt.vector3d(geometry.minimumCoordinate(0), geometry.minimumCoordinate(1), geometry.minimumCoordinate(2))
        const maximum = Qt.vector3d(geometry.maximumCoordinate(0), geometry.maximumCoordinate(1), geometry.maximumCoordinate(2))
        const existing = boundsByNode[nodeId]
        boundsByNode[nodeId] = existing ? {
            minimum: Qt.vector3d(Math.min(existing.minimum.x, minimum.x), Math.min(existing.minimum.y, minimum.y), Math.min(existing.minimum.z, minimum.z)),
            maximum: Qt.vector3d(Math.max(existing.maximum.x, maximum.x), Math.max(existing.maximum.y, maximum.y), Math.max(existing.maximum.z, maximum.z))
        } : { minimum: minimum, maximum: maximum }
        if (!hasSceneBounds) {
            sceneMinimum = minimum
            sceneMaximum = maximum
            hasSceneBounds = true
        } else {
            sceneMinimum = Qt.vector3d(Math.min(sceneMinimum.x, minimum.x), Math.min(sceneMinimum.y, minimum.y), Math.min(sceneMinimum.z, minimum.z))
            sceneMaximum = Qt.vector3d(Math.max(sceneMaximum.x, maximum.x), Math.max(sceneMaximum.y, maximum.y), Math.max(sceneMaximum.z, maximum.z))
        }
        fitTimer.restart()
    }

    function clearMeshes() {
        for (let id in modelByNode) modelByNode[id].destroy()
        for (let id in geometryByNode) geometryByNode[id].destroy()
        for (let id in edgeModelByNode) edgeModelByNode[id].destroy()
        for (let id in edgeGeometryByNode) edgeGeometryByNode[id].destroy()
        modelByNode = ({})
        geometryByNode = ({})
        edgeModelByNode = ({})
        edgeGeometryByNode = ({})
        boundsByNode = ({})
        hasSceneBounds = false
        sceneMinimum = Qt.vector3d(0, 0, 0)
        sceneMaximum = Qt.vector3d(0, 0, 0)
        fitCamera()
    }

    function applyPresentation() {
        for (let id in modelByNode) {
            modelByNode[id].visible = !controller || controller.isNodeVisible(modelByNode[id].nodeId)
        }
        applyRenderMode()
    }

    function applyRenderMode() {
        for (let id in modelByNode) modelByNode[id].visible = renderMode !== 2 && (!controller || controller.isNodeVisible(modelByNode[id].nodeId))
        for (let id in edgeModelByNode) {
            const edgeModel = edgeModelByNode[id]
            const selected = controller && controller.activeNodeId === edgeModel.nodeId
            edgeModel.visible = (renderMode !== 0 || selected) && (!controller || controller.isNodeVisible(edgeModel.nodeId))
        }
    }

    function applyAppearance() {
        if (!controller) return
        for (let id in modelByNode) {
            const model = modelByNode[id]
            model.sourceColor = controller.resolvedAppearanceColor(model.nodeId, model.importColor)
        }
    }

    function applySection() {
        if (!root.controller) return
        const section = root.controller.section
        const axis = section.axisName === "X" ? 0 : section.axisName === "Y" ? 1 : 2
        for (let id in geometryByNode) {
            if (section.usingSelectedPlane)
                geometryByNode[id].setSectionPlane(section.enabled, section.planeNormal.x, section.planeNormal.y, section.planeNormal.z, section.planeOffset, section.flipped)
            else
                geometryByNode[id].setSection(section.enabled, axis, section.position, section.flipped)
            geometryByNode[id].setSectionOptions(section.capEnabled, section.sliceOnly,
                                                  section.sliceDisplay !== "outline", section.sliceDisplay !== "filled")
        }
        for (let id in edgeGeometryByNode) {
            if (section.usingSelectedPlane)
                edgeGeometryByNode[id].setSectionPlane(section.enabled, section.planeNormal.x, section.planeNormal.y, section.planeNormal.z, section.planeOffset, section.flipped)
            else
                edgeGeometryByNode[id].setSection(section.enabled, axis, section.position, section.flipped)
            edgeGeometryByNode[id].setSectionOptions(section.capEnabled, section.sliceOnly,
                                                      section.sliceDisplay !== "outline", section.sliceDisplay !== "filled")
        }
    }

    function appendMesh(nodeId, segmentKey, sourceColor, meshJson) {
        if (geometryByNode[segmentKey]) {
            const geometry = geometryByNode[segmentKey]
            if (!geometry.replaceWorkerMesh(meshJson)) return
            const model = modelByNode[segmentKey]
            if (model) {
                model.importColor = sourceColor || model.importColor
                model.sourceColor = controller ? controller.resolvedAppearanceColor(nodeId, model.importColor) : model.importColor
            }
            applyPresentation()
            applySection()
            return
        }
        const geometry = geometryComponent.createObject(sceneRoot)
        if (!geometry.appendWorkerMesh(meshJson)) {
            geometry.destroy()
            return
        }
        const importedColor = sourceColor || "#67d5c0"
        const model = modelComponent.createObject(sceneRoot, { "geometry": geometry, "nodeId": nodeId, "importColor": importedColor,
                                                    "sourceColor": controller ? controller.resolvedAppearanceColor(nodeId, importedColor) : importedColor })
        geometryByNode[segmentKey] = geometry
        modelByNode[segmentKey] = model
        includeGeometryBounds(nodeId, geometry)
        applyPresentation()
        applySection()
    }

    function appendEdges(nodeId, edgeJson) {
        if (edgeGeometryByNode[nodeId]) {
            edgeGeometryByNode[nodeId].replaceWorkerEdges(edgeJson)
            applyPresentation()
            applySection()
            return
        }
        const geometry = edgeGeometryComponent.createObject(sceneRoot)
        if (!geometry.replaceWorkerEdges(edgeJson)) {
            geometry.destroy()
            return
        }
        const model = edgeModelComponent.createObject(sceneRoot, { "geometry": geometry, "nodeId": nodeId })
        edgeGeometryByNode[nodeId] = geometry
        edgeModelByNode[nodeId] = model
        applyPresentation()
        applySection()
    }

    function pickAt(x, y) {
        const hit = view.pick(x, y)
        if (!hit.objectHit || !hit.objectHit.nodeId) {
            root.controller.setActiveNodeId("")
            return false
        }
        root.controller.acceptViewPick(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                       hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z)
        return true
    }

    function setOrbitPivotAt(x, y) {
        const hit = view.pick(x, y)
        if (hit.objectHit && hit.scenePosition)
            navigation.setOrbitPivot(hit.scenePosition)
    }

    function updateHover(x, y) {
        const hit = view.pick(x, y)
        const nodeId = hit.objectHit && hit.objectHit.nodeId ? hit.objectHit.nodeId : ""
        if (hoveredNodeId !== nodeId) hoveredNodeId = nodeId
    }

    function captureToFile(fileUrl) {
        if (!root.controller || !fileUrl) return
        const capture = root.controller.capture
        root.captureInProgress = capture.includeMeasurements && root.controller.measurement.resultLabel.length > 0
        root.captureUiHidden = true
        viewportVisualTheme.transparentCapture = capture.transparentBackground
        if (!capture.includeSectionCaps)
            for (let id in geometryByNode)
                geometryByNode[id].setSectionOptions(false, false, true, true)

        Qt.callLater(function() {
            root.grabToImage(function(result) {
                capture.saveImage(result.image, fileUrl)
                viewportVisualTheme.transparentCapture = false
                root.captureInProgress = false
                root.captureUiHidden = false
                root.applySection()
            }, Qt.size(capture.resolvedWidth, capture.resolvedHeight))
        })
    }

    Component { id: geometryComponent; MeshGeometry {} }
    Component { id: edgeGeometryComponent; CadEdgeGeometry {} }
    Timer { id: fitTimer; interval: 120; onTriggered: root.fitCamera() }
    Component {
        id: modelComponent
        Model {
            id: modelItem
            property string nodeId: ""
            property color importColor: "#67d5c0"
            property color sourceColor: "#67d5c0"
            property bool selected: root.controller && root.controller.activeNodeId === nodeId
            property bool hovered: !selected && root.hoveredNodeId === nodeId
            pickable: true
            materials: PrincipledMaterial {
                baseColor: modelItem.selected ? viewportVisualTheme.selectionColor
                                              : modelItem.hovered ? viewportVisualTheme.hoverColor
                                                                  : modelItem.sourceColor
                roughness: 0.34
                emissiveFactor: modelItem.selected ? viewportVisualTheme.selectionEmissive
                                                   : modelItem.hovered ? viewportVisualTheme.hoverEmissive
                                                                       : "#000000"
            }
        }
    }
    Component {
        id: edgeModelComponent
        Model {
            id: edgeModelItem
            property string nodeId: ""
            property bool selected: root.controller && root.controller.activeNodeId === nodeId
            pickable: false
            materials: PrincipledMaterial {
                lighting: PrincipledMaterial.NoLighting
                baseColor: edgeModelItem.selected ? viewportVisualTheme.selectionEdgeColor
                                                   : viewportVisualTheme.edgeColor
                roughness: 1.0
                cullMode: Material.NoCulling
            }
        }
    }

    ViewportNavigation {
        id: navigation
        cameraRig: cameraRig
        camera: camera
    }

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment {
            id: sceneEnvironment
            backgroundMode: viewportVisualTheme.backgroundMode
            clearColor: viewportVisualTheme.canvasBackground
        }
        camera: camera
        Node {
            id: cameraRig
            PerspectiveCamera { id: camera; position: Qt.vector3d(0, 0, 120) }
            DirectionalLight { eulerRotation: Qt.vector3d(0, 0, 0); brightness: viewportVisualTheme.keyLightBrightness }
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -35, 0); brightness: viewportVisualTheme.fillLightBrightness }
        Node { id: sceneRoot }
        onWidthChanged: if (root.controller) root.controller.capture.setViewportSize(width, height)
        onHeightChanged: if (root.controller) root.controller.capture.setViewportSize(width, height)
        Component.onCompleted: if (root.controller) root.controller.capture.setViewportSize(width, height)
    }

    MouseArea {
        id: input
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
        preventStealing: true
        property real pressX: 0
        property real pressY: 0
        property int pressButton: Qt.NoButton
        property bool dragging: false
        property bool orbitPivotSet: false
        hoverEnabled: true

        onPressed: function(mouse) {
            root.forceActiveFocus()
            pressX = mouse.x
            pressY = mouse.y
            pressButton = mouse.button
            dragging = false
            orbitPivotSet = false
        }
        onPositionChanged: function(mouse) {
            if (pressButton === Qt.NoButton) {
                root.updateHover(mouse.x, mouse.y)
                return
            }
            const deltaX = mouse.x - pressX
            const deltaY = mouse.y - pressY
            if (Math.abs(deltaX) > 3 || Math.abs(deltaY) > 3) dragging = true
            if (!dragging) return
            if (pressButton === Qt.MiddleButton || (pressButton === Qt.LeftButton && (mouse.modifiers & Qt.ShiftModifier)))
                navigation.pan(deltaX, deltaY)
            else if (pressButton === Qt.LeftButton) {
                if (!orbitPivotSet) {
                    root.setOrbitPivotAt(pressX, pressY)
                    orbitPivotSet = true
                }
                navigation.orbit(deltaX, deltaY)
            }
            pressX = mouse.x
            pressY = mouse.y
        }
        onReleased: function(mouse) {
            if (!dragging && pressButton === Qt.LeftButton) root.pickAt(mouse.x, mouse.y)
            if (!dragging && pressButton === Qt.RightButton) {
                if (root.pickAt(mouse.x, mouse.y)) componentContextMenu.popup(mouse.x, mouse.y)
                else viewportContextMenu.popup(mouse.x, mouse.y)
            }
            pressButton = Qt.NoButton
        }
        onExited: root.hoveredNodeId = ""
        onWheel: function(wheel) { navigation.zoom(wheel.angleDelta.y) }
    }

    Item {
        id: measurementPickOverlay
        visible: root.controller && root.controller.measurement.pickedPoints.length > 0
        anchors.fill: parent
        z: 4
        enabled: false

        Repeater {
            model: root.controller ? root.controller.measurement.pickedPoints : []
            delegate: Rectangle {
                required property var modelData
                readonly property point screenPosition: view.mapFrom3DScene(modelData)
                width: 24
                height: 24
                radius: 12
                x: screenPosition.x - width / 2
                y: screenPosition.y - height / 2
                color: viewportVisualTheme.selectionColor
                border.color: viewportVisualTheme.markerBorder
                border.width: 2
                visible: screenPosition.x >= -width && screenPosition.x <= parent.width
                         && screenPosition.y >= -height && screenPosition.y <= parent.height

                Label {
                    anchors.centerIn: parent
                    text: index + 1
                    color: viewportVisualTheme.sectionLabel
                    font.bold: true
                }
            }
        }
    }

    PinchHandler {
        target: null
        property real previousScale: 1
        onActiveChanged: previousScale = 1
        onScaleChanged: {
            if (!active) return
            navigation.zoomByFactor(scale / previousScale)
            previousScale = scale
        }
    }

    Item {
        id: sectionManipulator
        visible: !root.captureUiHidden && root.controller && root.controller.section.enabled
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        width: 42
        height: 96
        z: 5

        Rectangle {
            width: 2
            height: 62
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            color: root.theme.accent
        }
        Rectangle {
            width: 12
            height: 12
            radius: 6
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top + 3
            color: root.theme.accent
            border.color: viewportVisualTheme.sectionBorder
            border.width: 1
        }
        Rectangle {
            width: 12
            height: 12
            radius: 6
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom - 3
            color: root.theme.accent
            border.color: viewportVisualTheme.sectionBorder
            border.width: 1
        }
        Rectangle {
            id: sectionHandle
            width: 30
            height: 30
            radius: 15
            anchors.centerIn: parent
            color: root.theme.accent
            border.color: viewportVisualTheme.sectionBorder
            border.width: 2
            Label {
                anchors.centerIn: parent
                text: qsTr("Offset")
                color: viewportVisualTheme.sectionLabel
                font.pixelSize: 9
                font.bold: true
            }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.SizeVerCursor
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            property real startY: 0
            property real startPosition: 0
            property bool dragging: false

            onPressed: function(mouse) {
                startY = mouse.y
                startPosition = root.controller.section.position
                dragging = true
            }
            onPositionChanged: function(mouse) {
                if (!dragging) return
                const multiplier = (mouse.modifiers & Qt.ShiftModifier) ? 0.1 : 1.0
                root.controller.section.position = startPosition - (mouse.y - startY) * root.sectionDragScale() * multiplier
            }
            onCanceled: {
                if (dragging) root.controller.section.position = startPosition
                dragging = false
            }
            onReleased: dragging = false
            onDoubleClicked: root.controller.section.position = 0
        }
    }

    Menu {
        id: componentContextMenu
        MenuItem { text: qsTr("Fit selection"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.fitSelection() }
        MenuSeparator {}
        MenuItem {
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Restore") : qsTr("Isolate")
            enabled: root.controller && root.controller.activeNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        MenuItem { text: qsTr("Hide"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        MenuItem { text: qsTr("Hide others"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideOtherNodes() }
        MenuItem { text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
        MenuItem { text: qsTr("Restore"); enabled: root.controller && root.controller.canRestoreVisibility; onTriggered: root.controller.restoreVisibility() }
        MenuSeparator {}
        MenuItem { text: qsTr("Properties"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.toolRequested("properties") }
    }

    Menu {
        id: viewportContextMenu
        MenuItem { text: qsTr("Fit assembly"); onTriggered: root.fitCamera() }
        MenuItem { text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
    }

    ThemedButton {
        id: displayButton
        theme: root.theme
        visible: !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        text: renderMode === 0 ? qsTr("Solid") : renderMode === 1 ? qsTr("Solid + Edges") : qsTr("Edges Only")
        Accessible.name: qsTr("Display style")
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Display style")
        onClicked: displayMenu.open()
        Menu {
            id: displayMenu
            y: parent.height
            MenuItem { text: qsTr("Solid"); checkable: true; checked: root.renderMode === 0; onTriggered: root.renderMode = 0 }
            MenuItem { text: qsTr("Solid + Edges"); checkable: true; checked: root.renderMode === 1; onTriggered: root.renderMode = 1 }
            MenuItem { text: qsTr("Edges Only"); checkable: true; checked: root.renderMode === 2; onTriggered: root.renderMode = 2 }
        }
    }

    Item {
        id: viewCube
        visible: !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: displayButton.height + 18
        anchors.rightMargin: 12
        width: 94
        height: 94

        Rectangle {
            x: 10
            y: 30
            width: 50
            height: 50
            radius: 4
            color: root.theme.surfaceRaised
            border.color: root.theme.border
        }
        ThemedButton {
            theme: root.theme
            x: 10
            y: 4
            width: 50
            height: 24
            text: qsTr("T")
            Accessible.name: qsTr("Top view")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Top view")
            onClicked: root.setStandardView(Qt.vector3d(0, 1, 0))
        }
        ThemedButton {
            theme: root.theme
            x: 10
            y: 30
            width: 50
            height: 50
            text: qsTr("F")
            Accessible.name: qsTr("Front view")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Front view")
            onClicked: root.setStandardView(Qt.vector3d(0, 0, 1))
        }
        ThemedButton {
            theme: root.theme
            x: 62
            y: 30
            width: 30
            height: 50
            text: qsTr("R")
            Accessible.name: qsTr("Right view")
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Right view")
            onClicked: root.setStandardView(Qt.vector3d(1, 0, 0))
        }
    }

    Shortcut { sequence: "F"; onActivated: root.fitCamera() }
    Shortcut { sequence: "Shift+F"; onActivated: root.fitSelection() }
    Shortcut { sequence: "I"; onActivated: if (root.controller) root.controller.toggleIsolateActiveNode() }
    Shortcut { sequence: "H"; onActivated: if (root.controller) root.controller.hideActiveNode() }
    Shortcut { sequence: "Shift+H"; onActivated: if (root.controller) root.controller.hideOtherNodes() }
    Shortcut { sequence: "Ctrl+Shift+H"; onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "Meta+Shift+H"; onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "M"; onActivated: root.toolRequested("measure") }
    Shortcut { sequence: "S"; onActivated: root.toolRequested("section") }
    Shortcut { sequence: "1"; onActivated: root.renderMode = 0 }
    Shortcut { sequence: "2"; onActivated: root.renderMode = 1 }
    Shortcut { sequence: "3"; onActivated: root.renderMode = 2 }
    Shortcut { sequence: StandardKey.Open; onActivated: root.openFileRequested() }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (root.controller && root.controller.canRestoreVisibility) root.controller.restoreVisibility()
            else root.toolRequested("close")
        }
    }

    Rectangle {
        visible: root.captureInProgress
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.margins: 16
        radius: 4
        color: "#0e171dcc"
        border.color: "#67d5c0"
        implicitWidth: measurementOverlay.implicitWidth + 18
        implicitHeight: measurementOverlay.implicitHeight + 12
        Label {
            id: measurementOverlay
            anchors.centerIn: parent
            text: root.controller ? root.controller.measurement.resultLabel : ""
            color: "#dffbf4"
            font.bold: true
        }
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.clearMeshes() }
        function onMeshReady(nodeId, segmentKey, sourceColor, meshJson) { root.appendMesh(nodeId, segmentKey, sourceColor, meshJson) }
        function onEdgeReady(nodeId, edgeJson) { root.appendEdges(nodeId, edgeJson) }
        function onFitRequested() { root.fitCamera() }
        function onVisibilityChanged() { root.applyPresentation() }
        function onComponentPropertiesChanged() { root.applyAppearance() }
        function onActiveNodeIdChanged() { root.applyRenderMode() }
    }
    onRenderModeChanged: applyRenderMode()
    Connections {
        target: root.controller ? root.controller.section : null
        function onChanged() { root.applySection() }
    }
}
