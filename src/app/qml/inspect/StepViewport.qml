import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import Loupe.App

Item {
    id: root
    focus: true

    property QtObject controller
    property QtObject theme
    property bool presentationOnly: false
    property bool selectionEnabled: true
    property bool componentHoverEnabled: true
    property bool contextActionsEnabled: !presentationOnly
    property bool requireDisplayFilter: false
    property string displayOnlyNodeId: ""
    property string externalHighlightNodeId: ""
    property var modelByNode: ({})
    property var geometryByNode: ({})
    property var edgeModelByNode: ({})
    property var edgeGeometryByNode: ({})
    property var sectionOverlayModelByNode: ({})
    property var sectionOverlayGeometryByNode: ({})
    property var xrayModelBySegment: ({})
    property var boundsByNode: ({})
    property string hoveredNodeId: ""
    property int renderMode: 0
    property bool measurementActive: false
    readonly property bool showComponentSelection: !measurementActive || (controller &&
            (controller.measurement.mode === 6 || controller.measurement.mode === 7))
    property bool captureInProgress: false
    property bool captureUiHidden: false
    property bool hasSceneBounds: false
    property var sectionApplyQueue: []
    property int sectionApplyIndex: 0
    property var sectionApplyRequest: null
    property int sectionBusyCount: 0
    property int sectionFinalizeTotal: 0
    readonly property int maximumConcurrentSectionBuilds: 1
    property var hoverTopology: null
    property var acceptedTopologies: []
    readonly property bool hasAcceptedFaceHighlight: {
        for (const pick of acceptedTopologies) {
            if (pick.entityKind === "face") return true
        }
        return false
    }
    property bool sectionWasEnabled: false
    property bool sectionWasInteracting: false
    property string lastSectionPlaneKey: ""
    property vector3d sceneMinimum: Qt.vector3d(0, 0, 0)
    property vector3d sceneMaximum: Qt.vector3d(0, 0, 0)
    readonly property vector3d effectiveSectionNormal: controller
            ? controller.section.effectiveNormal : Qt.vector3d(0, 0, 1)
    readonly property vector3d removedSectionNormal: sceneSectionManipulator.removedSideNormal(
            effectiveSectionNormal, controller && controller.section.flipped)
    readonly property vector3d sceneCenter: Qt.vector3d((sceneMinimum.x + sceneMaximum.x) * 0.5,
                                                        (sceneMinimum.y + sceneMaximum.y) * 0.5,
                                                        (sceneMinimum.z + sceneMaximum.z) * 0.5)
    readonly property real sceneDiagonal: {
        const x = sceneMaximum.x - sceneMinimum.x
        const y = sceneMaximum.y - sceneMinimum.y
        const z = sceneMaximum.z - sceneMinimum.z
        return Math.max(0.001, Math.sqrt(x * x + y * y + z * z))
    }
    // CAD edge polylines sit directly on their shaded faces. Nudge them less
    // than a pixel toward the active camera to remove depth-fighting speckles,
    // while retaining the normal depth test that hides occluded edges.
    readonly property vector3d edgeOverlayOffset: {
        const x = navigation.cameraPosition.x - navigation.activePivot.x
        const y = navigation.cameraPosition.y - navigation.activePivot.y
        const z = navigation.cameraPosition.z - navigation.activePivot.z
        const length = Math.sqrt(x * x + y * y + z * z)
        if (length < 0.000001) return Qt.vector3d(0, 0, 0)
        const amount = Math.max(sceneDiagonal * 0.0000025, sectionWorldPerPixel * 0.35)
        return Qt.vector3d(x / length * amount, y / length * amount, z / length * amount)
    }
    readonly property vector3d sectionAnchor: {
        if (!controller) return sceneCenter
        const normal = effectiveSectionNormal
        const distance = controller.section.effectiveOffset
                - (normal.x * sceneCenter.x + normal.y * sceneCenter.y + normal.z * sceneCenter.z)
        return Qt.vector3d(sceneCenter.x + normal.x * distance,
                           sceneCenter.y + normal.y * distance,
                           sceneCenter.z + normal.z * distance)
    }
    readonly property real sectionWorldPerPixel: {
        if (navigation.projectionMode === "orthographic")
            return 1 / Math.max(0.000001, navigation.orthographicMagnification)
        const x = navigation.cameraPosition.x - sectionAnchor.x
        const y = navigation.cameraPosition.y - sectionAnchor.y
        const z = navigation.cameraPosition.z - sectionAnchor.z
        const distance = Math.max(0.001, Math.sqrt(x * x + y * y + z * z))
        return 2 * distance * Math.tan(navigation.perspectiveFieldOfView * Math.PI / 360)
                / Math.max(1, view.height)
    }
    readonly property real sectionArrowLength: Math.max(sceneDiagonal * 0.035,
                                                         Math.min(sceneDiagonal * 0.3, sectionWorldPerPixel * 88))
    readonly property vector3d sectionArrowTip: Qt.vector3d(
            sectionAnchor.x + removedSectionNormal.x * sectionArrowLength,
            sectionAnchor.y + removedSectionNormal.y * sectionArrowLength,
            sectionAnchor.z + removedSectionNormal.z * sectionArrowLength)
    readonly property point sectionAnchorScreen: {
        const navigationRevision = navigation.revision
        if (navigationRevision < 0 || !view.camera) return Qt.point(-10000, -10000)
        const point = view.mapFrom3DScene(sectionAnchor)
        return Qt.point(point.x, point.y)
    }
    readonly property point sectionArrowTipScreen: {
        const navigationRevision = navigation.revision
        if (navigationRevision < 0 || !view.camera) return Qt.point(-10000, -10000)
        const point = view.mapFrom3DScene(sectionArrowTip)
        return Qt.point(point.x, point.y)
    }
    readonly property real sectionProjectedLength: {
        const x = sectionArrowTipScreen.x - sectionAnchorScreen.x
        const y = sectionArrowTipScreen.y - sectionAnchorScreen.y
        return Math.sqrt(x * x + y * y)
    }
    readonly property bool useSectionFallback: sectionProjectedLength < 28
            || !isFinite(sectionProjectedLength)
    readonly property var sectionOffsetRange: calculateSectionOffsetRange()
    readonly property int sectionFinalizeRemaining: Math.max(0,
            sectionApplyQueue.length - sectionApplyIndex + sectionBusyCount)
    readonly property int sectionFinalizeCompleted: Math.max(0, sectionFinalizeTotal
            - Math.min(sectionFinalizeTotal, sectionFinalizeRemaining))
    readonly property bool sectionNeedsExactDisplay: controller && controller.section.enabled
            && (controller.section.sliceOnly || controller.section.capEnabled)
    readonly property bool sectionFinalizing: sectionNeedsExactDisplay
            && sectionFinalizeTotal > 0 && sectionFinalizeRemaining > 0
    signal toolRequested(string tool)
    signal openFileRequested()

    ViewportVisualTheme {
        id: viewportVisualTheme
        theme: root.theme
    }

    PointerInputRouter {
        id: pointerInputRouter
    }

    onControllerChanged: {
        updateCaptureViewportSize()
    }

    Connections {
        target: root.controller
        function onSelectionChanged() { root.applyRenderMode() }
    }

    function fitCamera() {
        if (displayOnlyNodeId.length > 0) {
            fitDisplayFilter()
            return
        }
        if (!hasSceneBounds) {
            navigation.reset()
            return
        }
        navigation.fitBounds(sceneMinimum, sceneMaximum)
    }

    function captureDevicePixelRatio() {
        return Screen.devicePixelRatio > 0 ? Screen.devicePixelRatio : 1
    }

    function updateCaptureViewportSize() {
        if (!controller || view.width <= 0 || view.height <= 0) return
        const pixelRatio = captureDevicePixelRatio()
        controller.capture.setViewportSize(Math.round(view.width * pixelRatio),
                                           Math.round(view.height * pixelRatio))
    }

    function nodeMatchesSelection(nodeId, selectionId) {
        if (selectionId.length === 0) return false
        if (nodeId === selectionId) return true
        return controller && controller.exportWorkspace
                ? controller.exportWorkspace.containsNode(selectionId, nodeId) : false
    }

    function nodeVisibleInPreview(nodeId) {
        if (requireDisplayFilter && displayOnlyNodeId.length === 0) return false
        return displayOnlyNodeId.length === 0 || nodeMatchesSelection(nodeId, displayOnlyNodeId)
    }

    function fitDisplayFilter() {
        if (!hasSceneBounds || displayOnlyNodeId.length === 0) return
        let minimum = Qt.vector3d(0, 0, 0)
        let maximum = Qt.vector3d(0, 0, 0)
        let found = false
        for (let nodeId in boundsByNode) {
            if (!nodeMatchesSelection(nodeId, displayOnlyNodeId)) continue
            const bounds = boundsByNode[nodeId]
            if (!found) {
                minimum = bounds.minimum
                maximum = bounds.maximum
                found = true
            } else {
                minimum = Qt.vector3d(Math.min(minimum.x, bounds.minimum.x), Math.min(minimum.y, bounds.minimum.y), Math.min(minimum.z, bounds.minimum.z))
                maximum = Qt.vector3d(Math.max(maximum.x, bounds.maximum.x), Math.max(maximum.y, bounds.maximum.y), Math.max(maximum.z, bounds.maximum.z))
            }
        }
        if (found) navigation.fitBounds(minimum, maximum)
    }

    function fitSelection() {
        if (!controller || controller.selectedNodeCount === 0) {
            fitCamera()
            return
        }
        let minimum = Qt.vector3d(0, 0, 0)
        let maximum = Qt.vector3d(0, 0, 0)
        let found = false
        for (let nodeId in boundsByNode) {
            if (!controller.isNodeSelected(nodeId)) continue
            const bounds = boundsByNode[nodeId]
            if (!found) {
                minimum = bounds.minimum
                maximum = bounds.maximum
                found = true
            } else {
                minimum = Qt.vector3d(Math.min(minimum.x, bounds.minimum.x), Math.min(minimum.y, bounds.minimum.y), Math.min(minimum.z, bounds.minimum.z))
                maximum = Qt.vector3d(Math.max(maximum.x, bounds.maximum.x), Math.max(maximum.y, bounds.maximum.y), Math.max(maximum.z, bounds.maximum.z))
            }
        }
        if (found) navigation.fitBounds(minimum, maximum)
        else fitCamera()
    }

    function alignToSection(oppositeSide) {
        if (!controller) return
        if (oppositeSide) controller.section.flipped = !controller.section.flipped
        const normal = effectiveSectionNormal
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

    function calculateSectionOffsetRange() {
        if (!controller || !hasSceneBounds) return { from: -1, to: 1 }
        const normal = effectiveSectionNormal
        let minimum = Number.POSITIVE_INFINITY
        let maximum = Number.NEGATIVE_INFINITY
        const xs = [sceneMinimum.x, sceneMaximum.x]
        const ys = [sceneMinimum.y, sceneMaximum.y]
        const zs = [sceneMinimum.z, sceneMaximum.z]
        for (let x of xs) {
            for (let y of ys) {
                for (let z of zs) {
                    const offset = normal.x * x + normal.y * y + normal.z * z
                    minimum = Math.min(minimum, offset)
                    maximum = Math.max(maximum, offset)
                }
            }
        }
        const baseOffset = controller.section.effectiveOffset - controller.section.position
        const padding = Math.max((maximum - minimum) * 0.1, sceneDiagonal * 0.01)
        return { from: minimum - baseOffset - padding, to: maximum - baseOffset + padding }
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
        sectionApplyTimer.stop()
        sectionApplyQueue = []
        sectionApplyRequest = null
        sectionBusyCount = 0
        sectionFinalizeTotal = 0
        clearMeasurementHighlights()
        for (let id in modelByNode) modelByNode[id].destroy()
        for (let id in geometryByNode) geometryByNode[id].destroy()
        for (let id in edgeModelByNode) edgeModelByNode[id].destroy()
        for (let id in edgeGeometryByNode) edgeGeometryByNode[id].destroy()
        for (let id in sectionOverlayModelByNode) sectionOverlayModelByNode[id].destroy()
        for (let id in sectionOverlayGeometryByNode) sectionOverlayGeometryByNode[id].destroy()
        for (let id in xrayModelBySegment) xrayModelBySegment[id].destroy()
        modelByNode = ({})
        geometryByNode = ({})
        edgeModelByNode = ({})
        edgeGeometryByNode = ({})
        sectionOverlayModelByNode = ({})
        sectionOverlayGeometryByNode = ({})
        xrayModelBySegment = ({})
        boundsByNode = ({})
        hasSceneBounds = false
        sectionWasEnabled = false
        sectionWasInteracting = false
        lastSectionPlaneKey = ""
        sceneMinimum = Qt.vector3d(0, 0, 0)
        sceneMaximum = Qt.vector3d(0, 0, 0)
        fitCamera()
    }

    function applyPresentation() {
        for (let id in modelByNode) {
            modelByNode[id].visible = nodeVisibleInPreview(modelByNode[id].nodeId)
                    && (!controller || controller.isNodeVisible(modelByNode[id].nodeId))
        }
        for (let id in xrayModelBySegment)
            xrayModelBySegment[id].visible = externalHighlightNodeId.length > 0
                    && nodeMatchesSelection(xrayModelBySegment[id].nodeId, externalHighlightNodeId)
        applyRenderMode()
    }

    function applyRenderMode() {
        const section = !presentationOnly && controller ? controller.section : null
        const sliceOnlyFinal = section && section.enabled && section.sliceOnly && !section.interacting
                && !sectionFinalizing
        for (let id in modelByNode)
            modelByNode[id].visible = renderMode !== 2 && !sliceOnlyFinal
                    && nodeVisibleInPreview(modelByNode[id].nodeId)
                    && (!controller || controller.isNodeVisible(modelByNode[id].nodeId))
        for (let id in edgeModelByNode) {
            const edgeModel = edgeModelByNode[id]
            const selected = controller && controller.isNodeSelected(edgeModel.nodeId)
            const sectionPreview = section && section.enabled && section.interacting
            edgeModel.visible = !sliceOnlyFinal && !sectionPreview && (renderMode !== 0 || selected)
                    && nodeVisibleInPreview(edgeModel.nodeId)
                    && (!controller || controller.isNodeVisible(edgeModel.nodeId))
        }
        for (let id in sectionOverlayModelByNode) {
            const overlay = sectionOverlayModelByNode[id]
            const hasExactDisplay = section && section.enabled && !section.interacting
                    && (section.sliceOnly || section.capEnabled)
            overlay.visible = hasExactDisplay && (section.sliceOnly || renderMode !== 2)
                    && (!controller || controller.isNodeVisible(overlay.nodeId))
        }
    }

    function applyAppearance() {
        if (!controller) return
        for (let id in modelByNode) {
            const model = modelByNode[id]
            model.sourceColor = controller.resolvedAppearanceColor(model.nodeId, model.importColor)
            const overlay = sectionOverlayModelByNode[id]
            if (overlay) overlay.sourceColor = model.sourceColor
        }
    }

    function applySection() {
        if (!root.controller || root.presentationOnly) return
        const section = root.controller.section
        const interactionChanged = sectionWasInteracting !== section.interacting
        sectionWasInteracting = section.interacting
        const planeKey = section.usingSelectedPlane ? "selected" : section.axisName
        const shouldCenterAxis = section.enabled && hasSceneBounds && !section.usingSelectedPlane
                && (!sectionWasEnabled || lastSectionPlaneKey !== planeKey)
        sectionWasEnabled = section.enabled
        lastSectionPlaneKey = planeKey
        if (shouldCenterAxis) {
            const normal = section.effectiveNormal
            const baseOffset = section.effectiveOffset - section.position
            const centeredPosition = normal.x * sceneCenter.x + normal.y * sceneCenter.y
                    + normal.z * sceneCenter.z - baseOffset
            if (Math.abs(section.position - centeredPosition) > 0.000001) {
                section.position = centeredPosition
                return
            }
        }
        if (section.interacting) {
            if (!interactionChanged) return
            sectionApplyTimer.stop()
            sectionApplyQueue = []
            sectionApplyRequest = null
            sectionFinalizeTotal = 0
            applyRenderMode()
            return
        }
        sectionApplyTimer.stop()
        for (let id in edgeGeometryByNode) {
            edgeGeometryByNode[id].configureSection(section.enabled,
                    section.effectiveNormal.x, section.effectiveNormal.y, section.effectiveNormal.z,
                    section.effectiveOffset, section.flipped, section.sliceOnly, false)
        }
        const queue = []
        if (section.enabled && (section.sliceOnly || section.capEnabled)) {
            for (let id in geometryByNode) {
                const overlay = ensureSectionOverlay(id)
                if (overlay) queue.push(overlay)
            }
        }
        sectionApplyRequest = queue.length > 0 ? {
            enabled: section.enabled,
            normal: section.effectiveNormal,
            offset: section.effectiveOffset,
            flipped: section.flipped,
            capEnabled: section.capEnabled,
            sliceOnly: section.sliceOnly,
            // Retain a fill subset in outline-only mode so the border keeps a
            // stable second material slot without rebuilding the section twice.
            sliceFill: section.sliceOnly || section.sliceDisplay !== "outline",
            sliceOutline: section.sliceDisplay !== "filled",
            outlineWidth: Math.max(sceneDiagonal * 0.00001,
                                   sectionWorldPerPixel * section.sliceBorderWidth),
            preview: false
        } : null
        sectionApplyQueue = queue
        sectionApplyIndex = 0
        sectionFinalizeTotal = queue.length
        applyRenderMode()
        if (queue.length > 0) sectionApplyTimer.restart()
    }

    function updateSectionBusyCount() {
        let count = 0
        for (let id in sectionOverlayGeometryByNode) {
            if (sectionOverlayGeometryByNode[id].sectionBusy) count += 1
        }
        sectionBusyCount = count
        applyRenderMode()
        if (sectionApplyRequest && sectionApplyIndex < sectionApplyQueue.length
                && sectionBusyCount < maximumConcurrentSectionBuilds
                && !sectionApplyTimer.running) {
            sectionApplyTimer.restart()
        }
    }

    function applyNextSectionItem() {
        if (!sectionApplyRequest || sectionApplyIndex >= sectionApplyQueue.length) return
        if (sectionBusyCount >= maximumConcurrentSectionBuilds) return
        const geometry = sectionApplyQueue[sectionApplyIndex]
        const request = sectionApplyRequest
        sectionApplyIndex += 1
        geometry.configureSection(request.enabled, request.normal.x, request.normal.y, request.normal.z,
                                  request.offset, request.flipped, request.capEnabled, request.sliceOnly,
                                  request.sliceFill, request.sliceOutline, request.preview,
                                  request.outlineWidth)
        updateSectionBusyCount()
    }

    function ensureSectionOverlay(segmentKey) {
        if (root.presentationOnly) return null
        const existing = sectionOverlayGeometryByNode[segmentKey]
        if (existing) return existing
        const sourceGeometry = geometryByNode[segmentKey]
        const sourceModel = modelByNode[segmentKey]
        if (!sourceGeometry || !sourceModel) return null

        const overlayGeometry = geometryComponent.createObject(sceneRoot)
        if (!overlayGeometry.copySectionOverlayFrom(sourceGeometry)) {
            overlayGeometry.destroy()
            return null
        }
        overlayGeometry.sectionBusyChanged.connect(root.updateSectionBusyCount)
        const overlayModel = sectionOverlayModelComponent.createObject(sceneRoot, {
            "geometry": overlayGeometry,
            "nodeId": sourceModel.nodeId,
            "sourceColor": sourceModel.sourceColor
        })
        sectionOverlayGeometryByNode[segmentKey] = overlayGeometry
        sectionOverlayModelByNode[segmentKey] = overlayModel
        return overlayGeometry
    }

    function appendMesh(nodeId, segmentKey, sourceColor, meshJson) {
        if (geometryByNode[segmentKey]) {
            const geometry = geometryByNode[segmentKey]
            if (!geometry.replaceWorkerMesh(meshJson)) return
            const overlayGeometry = sectionOverlayGeometryByNode[segmentKey]
            if (overlayGeometry) overlayGeometry.copySectionOverlayFrom(geometry)
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
        const importedColor = sourceColor || root.theme.neutralBody
        const model = modelComponent.createObject(sceneRoot, { "geometry": geometry, "nodeId": nodeId, "importColor": importedColor,
                                                    "sourceColor": controller ? controller.resolvedAppearanceColor(nodeId, importedColor) : importedColor })
        const xrayModel = componentXrayModel.createObject(componentXrayRoot, {
            "geometry": geometry,
            "nodeId": nodeId,
            "visible": root.externalHighlightNodeId.length > 0
                    && root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId)
        })
        geometryByNode[segmentKey] = geometry
        modelByNode[segmentKey] = model
        xrayModelBySegment[segmentKey] = xrayModel
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

    function pickAt(x, y, additive) {
        const hit = view.pick(x, y)
        if (!hit.objectHit || !hit.objectHit.nodeId) {
            root.controller.selectNode("", false)
            return false
        }
        if (root.measurementActive) {
            acceptMeasurementHit(hit)
        } else {
            root.controller.acceptViewSelection(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                                hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z, additive)
        }
        return true
    }

    function topologyPickFromHit(hit) {
        if (!root.controller || !hit.objectHit || !hit.objectHit.nodeId) return null
        const mode = root.controller.measurement.mode
        const nodeId = hit.objectHit.nodeId
        let sourceGeometry = null
        let info = null
        if (mode === 1 || mode === 3) {
            sourceGeometry = edgeGeometryByNode[nodeId]
            if (sourceGeometry) {
                const tolerance = Math.max(root.sceneDiagonal * 0.001, root.sectionWorldPerPixel * 10)
                info = sourceGeometry.topologyAtPoint(hit.scenePosition.x, hit.scenePosition.y,
                                                      hit.scenePosition.z, tolerance)
                if (info && mode === 3 && info.radiusMm <= 0) info = null
            }
            if (!info && mode === 3 && hit.objectHit.geometry) {
                sourceGeometry = hit.objectHit.geometry
                info = sourceGeometry.topologyAtPoint(hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z)
                if (info && info.radiusMm <= 0) info = null
            }
        } else if (mode === 2 || mode === 4 || mode === 5) {
            sourceGeometry = hit.objectHit.geometry
            if (sourceGeometry)
                info = sourceGeometry.topologyAtPoint(hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z)
        }
        if (!info || !info.topologyId) return null
        return {
            nodeId: nodeId,
            entityKind: info.entityKind,
            topologyId: info.topologyId,
            measureMm: info.measureMm,
            radiusMm: info.radiusMm,
            sourceGeometry: sourceGeometry,
            point: hit.scenePosition,
            normal: hit.sceneNormal
        }
    }

    function showHoverTopology(pick) {
        root.hoverTopology = pick
        hoverFaceHighlight.clearTopology()
        hoverEdgeGeometry.clearEdges()
        if (!pick) return
        if (pick.entityKind === "face")
            hoverFaceHighlight.setTopology(pick.sourceGeometry, pick.topologyId)
        else if (pick.entityKind === "edge")
            hoverEdgeGeometry.copyTopologyFrom(pick.sourceGeometry, pick.topologyId)
    }

    function clearMeasurementHighlights() {
        root.hoverTopology = null
        root.acceptedTopologies = []
        hoverFaceHighlight.clearTopology()
        hoverEdgeGeometry.clearEdges()
    }

    function acceptMeasurementHit(hit) {
        const mode = root.controller.measurement.mode
        if (mode === 0) {
            root.controller.acceptViewPick(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                           hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z)
            return
        }
        if (mode === 6 || mode === 7) {
            root.controller.acceptViewSelection(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                                hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z)
            return
        }
        const pick = topologyPickFromHit(hit)
        if (!pick) return
        if (!root.controller.acceptTopologyPick(pick.nodeId, pick.entityKind, pick.topologyId,
                                                pick.point.x, pick.point.y, pick.point.z,
                                                pick.normal.x, pick.normal.y, pick.normal.z,
                                                pick.measureMm, pick.radiusMm)) return
        const maximum = mode === 2 || mode === 4 ? 2 : 1
        let next = root.acceptedTopologies.slice()
        if (next.length >= maximum) next = []
        next.push(pick)
        root.acceptedTopologies = next
        showHoverTopology(null)
    }

    function prepareOrbitPivotAt(x, y) {
        const hit = view.pick(x, y)
        if (hit.objectHit && hit.scenePosition)
            navigation.setPendingOrbitPivot(hit.scenePosition)
        else
            navigation.clearPendingOrbitPivot()
    }

    function updateHover(x, y) {
        const hit = view.pick(x, y)
        const nodeId = hit.objectHit && hit.objectHit.nodeId ? hit.objectHit.nodeId : ""
        if (hoveredNodeId !== nodeId) hoveredNodeId = nodeId
        if (root.measurementActive && hit.objectHit && hit.objectHit.nodeId)
            showHoverTopology(topologyPickFromHit(hit))
        else if (root.hoverTopology)
            showHoverTopology(null)
    }

    function captureToFile(fileUrl) {
        if (!root.controller || !fileUrl) return
        const capture = root.controller.capture
        if (capture.inProgress) return
        capture.beginCapture()
        root.captureInProgress = true
        root.captureUiHidden = true
        viewportVisualTheme.transparentCapture = capture.transparentBackground
        if (!capture.includeSectionCaps)
            for (let id in sectionOverlayGeometryByNode)
                sectionOverlayGeometryByNode[id].setSectionOptions(false, false, true, true)

        const targetPixels = Qt.size(capture.resolvedWidth, capture.resolvedHeight)
        const pixelRatio = captureDevicePixelRatio()
        const grabSize = Qt.size(Math.max(1, Math.round(targetPixels.width / pixelRatio)),
                                 Math.max(1, Math.round(targetPixels.height / pixelRatio)))
        capture.setCaptureProgress(0.22, qsTr("Preparing %1 × %2 capture…").arg(targetPixels.width).arg(targetPixels.height))
        view.explicitTextureWidth = targetPixels.width
        view.explicitTextureHeight = targetPixels.height
        orthographicCamera.horizontalMagnification = navigation.orthographicMagnification * capture.scale
        orthographicCamera.verticalMagnification = navigation.orthographicMagnification * capture.scale

        Qt.callLater(function() {
            Qt.callLater(function() {
                capture.setCaptureProgress(0.48, qsTr("Rendering at %1×…").arg(capture.scale.toFixed(2)))
                const started = view.grabToImage(function(result) {
                    capture.setCaptureProgress(0.82, qsTr("Encoding PNG…"))
                    capture.saveImage(result.image, fileUrl)
                    view.explicitTextureWidth = 0
                    view.explicitTextureHeight = 0
                    orthographicCamera.horizontalMagnification = navigation.orthographicMagnification
                    orthographicCamera.verticalMagnification = navigation.orthographicMagnification
                    viewportVisualTheme.transparentCapture = false
                    root.captureInProgress = false
                    root.captureUiHidden = false
                    root.applySection()
                }, grabSize)
                if (!started) {
                    capture.failCapture(qsTr("Capture failed: the render could not start."))
                    view.explicitTextureWidth = 0
                    view.explicitTextureHeight = 0
                    orthographicCamera.horizontalMagnification = navigation.orthographicMagnification
                    orthographicCamera.verticalMagnification = navigation.orthographicMagnification
                    viewportVisualTheme.transparentCapture = false
                    root.captureInProgress = false
                    root.captureUiHidden = false
                    root.applySection()
                }
            })
        })
    }

    Component { id: geometryComponent; MeshGeometry {} }
    Component { id: edgeGeometryComponent; CadEdgeGeometry {} }
    Timer { id: fitTimer; interval: 120; onTriggered: root.fitCamera() }
    Timer { id: sectionApplyTimer; interval: 0; repeat: false; onTriggered: root.applyNextSectionItem() }
    Component {
        id: modelComponent
        Model {
            id: modelItem
            property string nodeId: ""
            property color importColor: root.theme.neutralBody
            property color sourceColor: root.theme.neutralBody
            property bool selected: root.showComponentSelection && root.controller
                                    && ((root.selectionEnabled && root.controller.selectedNodeCount >= 0
                                         && root.controller.isNodeSelected(nodeId))
                                        || root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId))
            property bool hovered: !selected && root.hoveredNodeId === nodeId
            property bool inspectionHovered: hovered && !root.measurementActive && root.componentHoverEnabled
            pickable: true
            materials: [sectionMaterial]
            SectionClipMaterial {
                id: sectionMaterial
                baseColor: modelItem.selected ? viewportVisualTheme.selectionColor
                                              : modelItem.inspectionHovered ? viewportVisualTheme.hoverColor
                                                                          : modelItem.sourceColor
                emissiveFactor: modelItem.selected ? viewportVisualTheme.selectionEmissive
                                                   : modelItem.inspectionHovered ? viewportVisualTheme.hoverEmissive
                                                                               : Qt.vector3d(0, 0, 0)
                roughnessValue: 0.34
                clipEnabled: !root.presentationOnly && root.controller && root.controller.section.enabled
                sectionNormal: root.effectiveSectionNormal
                sectionOffset: root.controller ? root.controller.section.effectiveOffset : 0
                retainedSign: root.controller && root.controller.section.flipped ? -1 : 1
            }
        }
    }
    Component {
        id: componentXrayModel
        Model {
            property string nodeId: ""
            pickable: false
            materials: PrincipledMaterial {
                lighting: PrincipledMaterial.NoLighting
                baseColor: root.theme.accent
                emissiveFactor: root.theme.accent
                opacity: 0.46
                cullMode: Material.NoCulling
            }
        }
    }
    Component {
        id: edgeModelComponent
        Model {
            id: edgeModelItem
            property string nodeId: ""
            property bool selected: root.showComponentSelection && root.controller
                                    && ((root.selectionEnabled && root.controller.selectedNodeCount >= 0
                                         && root.controller.isNodeSelected(nodeId))
                                        || root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId))
            pickable: false
            position: root.edgeOverlayOffset
            materials: [standardEdgeMaterial]
            PrincipledMaterial {
                id: standardEdgeMaterial
                lighting: PrincipledMaterial.NoLighting
                baseColor: edgeModelItem.selected ? viewportVisualTheme.selectionEdgeColor
                                                   : viewportVisualTheme.edgeColor
                // The camera-facing model offset removes coplanar depth
                // fighting. Depth testing still keeps hidden edges hidden.
                lineWidth: edgeModelItem.selected ? 2.5 : 1.5
                roughness: 1.0
                cullMode: Material.NoCulling
            }
        }
    }
    Component {
        id: sectionOverlayModelComponent
        Model {
            id: overlayModelItem
            property string nodeId: ""
            property color sourceColor: root.theme.neutralBody
            pickable: false
            materials: [sectionFillMaterial, sectionOutlineMaterial]
            PrincipledMaterial {
                id: sectionFillMaterial
                lighting: PrincipledMaterial.NoLighting
                baseColor: overlayModelItem.sourceColor
                emissiveFactor: Qt.vector3d(0.08, 0.08, 0.08)
                opacity: root.controller && root.controller.section.sliceOnly
                         && root.controller.section.sliceDisplay === "outline" ? 0 : 1
                alphaMode: opacity < 1 ? PrincipledMaterial.Blend : PrincipledMaterial.Opaque
                roughness: 0.6
                cullMode: Material.NoCulling
            }
            PrincipledMaterial {
                id: sectionOutlineMaterial
                lighting: PrincipledMaterial.NoLighting
                baseColor: root.controller && root.controller.section.sliceBorderColor.length > 0
                           ? root.controller.section.sliceBorderColor : viewportVisualTheme.edgeColor
                roughness: 1.0
                cullMode: Material.NoCulling
            }
        }
    }

    ViewportNavigation {
        id: navigation
        cameraRig: cameraRig
        perspectiveCamera: perspectiveCamera
        orthographicCamera: orthographicCamera
        viewportWidth: view.width
        viewportHeight: view.height
    }

    View3D {
        id: view
        anchors.fill: parent
        renderMode: View3D.Offscreen
        environment: SceneEnvironment {
            id: sceneEnvironment
            backgroundMode: viewportVisualTheme.backgroundMode
            clearColor: viewportVisualTheme.canvasBackground
        }
        camera: navigation.projectionMode === "orthographic" ? orthographicCamera : perspectiveCamera
        Node {
            id: cameraRig
            PerspectiveCamera {
                id: perspectiveCamera
                clipNear: 0.01
                clipFar: 1000000
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: orthographicCamera
                clipNear: -1000000
                clipFar: 1000000
                horizontalMagnification: navigation.orthographicMagnification
                verticalMagnification: navigation.orthographicMagnification
            }
            DirectionalLight { eulerRotation: Qt.vector3d(0, 0, 0); brightness: viewportVisualTheme.keyLightBrightness }
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -35, 0); brightness: viewportVisualTheme.fillLightBrightness }
        Node {
            id: sceneRoot

            Model {
                visible: root.hoverTopology && root.hoverTopology.entityKind === "edge"
                geometry: CadEdgeGeometry { id: hoverEdgeGeometry }
                materials: PrincipledMaterial {
                    lighting: PrincipledMaterial.NoLighting
                    baseColor: root.theme.accent
                    emissiveFactor: root.theme.accent
                    lineWidth: 3
                }
            }
            Repeater3D {
                model: root.acceptedTopologies
                delegate: Model {
                    required property var modelData
                    visible: modelData.entityKind === "edge"
                    geometry: CadEdgeGeometry {
                        Component.onCompleted: copyTopologyFrom(modelData.sourceGeometry, modelData.topologyId)
                    }
                    materials: PrincipledMaterial {
                        lighting: PrincipledMaterial.NoLighting
                        baseColor: viewportVisualTheme.selectionColor
                        emissiveFactor: viewportVisualTheme.selectionEmissive
                        lineWidth: 4
                    }
                }
            }

            SectionManipulator {
                id: sceneSectionManipulator
                visible: !root.presentationOnly && !root.captureUiHidden && root.controller && root.controller.section.enabled
                         && root.hasSceneBounds && !root.useSectionFallback
                position: root.sectionAnchor
                normal: root.removedSectionNormal
                arrowLength: root.sectionArrowLength
                color: root.theme.accentVivid
                outlineColor: viewportVisualTheme.sectionBorder
            }
        }
        onWidthChanged: root.updateCaptureViewportSize()
        onHeightChanged: root.updateCaptureViewportSize()
        Component.onCompleted: root.updateCaptureViewportSize()
    }

    View3D {
        id: componentXrayView
        anchors.fill: parent
        renderMode: View3D.Offscreen
        visible: root.externalHighlightNodeId.length > 0
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            depthTestEnabled: false
            depthPrePassEnabled: false
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }
        camera: navigation.projectionMode === "orthographic"
                ? componentXrayOrthographicCamera : componentXrayPerspectiveCamera
        Node {
            position: navigation.cameraPosition
            rotation: navigation.orientation
            PerspectiveCamera {
                id: componentXrayPerspectiveCamera
                clipNear: 0.01
                clipFar: 1000000
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: componentXrayOrthographicCamera
                clipNear: -1000000
                clipFar: 1000000
                horizontalMagnification: navigation.orthographicMagnification
                verticalMagnification: navigation.orthographicMagnification
            }
        }
        Node { id: componentXrayRoot }
    }

    View3D {
        id: measurementXrayView
        objectName: "measurementXrayView"
        anchors.fill: parent
        renderMode: View3D.Offscreen
        visible: !root.presentationOnly && ((root.hoverTopology && root.hoverTopology.entityKind === "face")
                 || root.hasAcceptedFaceHighlight
                 )
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            depthTestEnabled: false
            depthPrePassEnabled: false
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }
        camera: navigation.projectionMode === "orthographic"
                ? xrayOrthographicCamera : xrayPerspectiveCamera

        Node {
            position: navigation.cameraPosition
            rotation: navigation.orientation

            PerspectiveCamera {
                id: xrayPerspectiveCamera
                clipNear: 0.01
                clipFar: 1000000
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: xrayOrthographicCamera
                clipNear: -1000000
                clipFar: 1000000
                horizontalMagnification: navigation.orthographicMagnification
                verticalMagnification: navigation.orthographicMagnification
            }
        }

        MeasurementFaceHighlight {
            id: hoverFaceHighlight
            objectName: "hoverFaceHighlight"
            active: root.hoverTopology && root.hoverTopology.entityKind === "face"
            fillColor: root.theme.accent
            boundaryColor: viewportVisualTheme.sectionBorder
            fillAlpha: 0.44
            boundaryWidth: 3
        }

        Repeater3D {
            model: root.acceptedTopologies
            delegate: MeasurementFaceHighlight {
                required property var modelData
                active: modelData.entityKind === "face"
                fillColor: viewportVisualTheme.selectionColor
                boundaryColor: viewportVisualTheme.selectionEdgeColor
                fillAlpha: 0.58
                boundaryWidth: 4
                Component.onCompleted: {
                    if (active) setTopology(modelData.sourceGeometry, modelData.topologyId)
                }
            }
        }
    }

    Rectangle {
        id: sectionFinalizeStatus
        visible: !root.captureUiHidden && root.sectionFinalizing
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        width: 224
        height: 42
        radius: root.theme.radius1
        color: root.theme.surfaceRaised
        border.color: root.theme.border
        z: 7

        Column {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Label {
                width: parent.width
                text: qsTr("Finalizing section %1/%2").arg(root.sectionFinalizeCompleted)
                        .arg(root.sectionFinalizeTotal)
                color: root.theme.onSurface
                font.pixelSize: 12
            }

            Rectangle {
                width: parent.width
                height: 4
                radius: 2
                color: root.theme.surfaceSubtle

                Rectangle {
                    width: parent.width * root.sectionFinalizeCompleted
                            / Math.max(1, root.sectionFinalizeTotal)
                    height: parent.height
                    radius: parent.radius
                    color: root.theme.accent
                }
            }
        }
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
        hoverEnabled: true

        onPressed: function(mouse) {
            root.forceActiveFocus()
            pressX = mouse.x
            pressY = mouse.y
            pressButton = mouse.button
            dragging = false
            if (mouse.button === Qt.LeftButton && !(mouse.modifiers & Qt.ShiftModifier))
                root.prepareOrbitPivotAt(mouse.x, mouse.y)
            else
                navigation.clearPendingOrbitPivot()
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
            else if (pressButton === Qt.LeftButton)
                navigation.orbit(deltaX, deltaY)
            pressX = mouse.x
            pressY = mouse.y
        }
        onReleased: function(mouse) {
            if (!dragging && pressButton === Qt.LeftButton && root.selectionEnabled) {
                const additive = (mouse.modifiers & Qt.ShiftModifier)
                        || (mouse.modifiers & Qt.ControlModifier)
                        || (mouse.modifiers & Qt.MetaModifier)
                root.pickAt(mouse.x, mouse.y, additive)
            }
            if (!dragging && pressButton === Qt.RightButton && root.contextActionsEnabled) {
                if (root.pickAt(mouse.x, mouse.y, false)) {
                    if (root.presentationOnly) exportComponentContextMenu.popup(mouse.x, mouse.y)
                    else componentContextMenu.popup(mouse.x, mouse.y)
                }
                else viewportContextMenu.popup(mouse.x, mouse.y)
            }
            pressButton = Qt.NoButton
            navigation.clearPendingOrbitPivot()
        }
        onExited: {
            root.hoveredNodeId = ""
            root.showHoverTopology(null)
        }
        onWheel: function(wheel) {
            const deviceType = wheel.device ? wheel.device.type : PointerDevice.Unknown
            const mode = pointerInputRouter.wheelMode(deviceType,
                                                       wheel.pixelDelta.x,
                                                       wheel.pixelDelta.y,
                                                       wheel.angleDelta.x,
                                                       wheel.angleDelta.y)
            if (mode === "pan") {
                navigation.pan(wheel.pixelDelta.x, wheel.pixelDelta.y)
            } else {
                navigation.zoom(wheel.angleDelta.y !== 0
                                ? wheel.angleDelta.y : wheel.pixelDelta.y)
            }
            wheel.accepted = true
        }
    }

    Item {
        id: measurementPickOverlay
        visible: !root.presentationOnly && root.controller && root.controller.measurement.pickedPoints.length > 0
        anchors.fill: parent
        z: 4
        enabled: false

        Repeater {
            model: root.controller ? root.controller.measurement.pickedPoints : []
            delegate: Rectangle {
                required property var modelData
                readonly property point screenPosition: {
                    // mapFrom3DScene has no declared camera dependency, so keep
                    // the overlay projection synchronized with navigation.
                    const navigationRevision = navigation.revision
                    if (navigationRevision < 0) return Qt.point(-10000, -10000)
                    return view.mapFrom3DScene(modelData)
                }
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
        id: sectionArrowHitTarget
        visible: !root.presentationOnly && !root.captureUiHidden && root.controller && root.controller.section.enabled
                 && root.hasSceneBounds && !root.useSectionFallback
        x: root.sectionAnchorScreen.x
        y: root.sectionAnchorScreen.y - height / 2
        width: Math.max(28, root.sectionProjectedLength)
        height: 34
        rotation: Math.atan2(root.sectionArrowTipScreen.y - root.sectionAnchorScreen.y,
                             root.sectionArrowTipScreen.x - root.sectionAnchorScreen.x) * 180 / Math.PI
        transformOrigin: Item.Left
        z: 5

        MouseArea {
            id: sectionArrowInput
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            preventStealing: true
            property real startPosition: 0
            property real startScreenX: 0
            property real startScreenY: 0
            property real axisX: 1
            property real axisY: 0
            property real worldPerPixel: 1
            property bool dragging: false

            function cancelDrag() {
                if (dragging) root.controller.section.cancelInteraction()
                dragging = false
            }

            onPressed: function(mouse) {
                root.forceActiveFocus()
                const point = mapToItem(root, mouse.x, mouse.y)
                startScreenX = point.x
                startScreenY = point.y
                axisX = (root.sectionArrowTipScreen.x - root.sectionAnchorScreen.x) / root.sectionProjectedLength
                axisY = (root.sectionArrowTipScreen.y - root.sectionAnchorScreen.y) / root.sectionProjectedLength
                worldPerPixel = root.sectionArrowLength / root.sectionProjectedLength
                startPosition = root.controller.section.position
                root.controller.section.beginInteraction()
                dragging = true
            }
            onPositionChanged: function(mouse) {
                if (!dragging) return
                const point = mapToItem(root, mouse.x, mouse.y)
                const pixelOffset = (point.x - startScreenX) * axisX + (point.y - startScreenY) * axisY
                const multiplier = (mouse.modifiers & Qt.ShiftModifier) ? 0.1 : 1.0
                const direction = root.removedSectionNormal.x * root.effectiveSectionNormal.x
                        + root.removedSectionNormal.y * root.effectiveSectionNormal.y
                        + root.removedSectionNormal.z * root.effectiveSectionNormal.z
                root.controller.section.previewPosition(startPosition + pixelOffset * worldPerPixel * direction * multiplier)
            }
            onCanceled: cancelDrag()
            onReleased: {
                root.controller.section.commitInteraction()
                dragging = false
            }
            onDoubleClicked: root.controller.section.position = 0
        }
    }

    SectionOffsetSlider {
        id: sectionFallbackSlider
        theme: root.theme
        visible: !root.presentationOnly && !root.captureUiHidden && root.controller && root.controller.section.enabled
                 && root.hasSceneBounds && root.useSectionFallback
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 14
        z: 5
        from: root.sectionOffsetRange.from
        to: root.sectionOffsetRange.to
        value: root.controller ? root.controller.section.position : 0
        resetValue: 0
        onInteractionStarted: root.controller.section.beginInteraction()
        onValueEdited: function(value) {
            if (root.controller.section.interacting) root.controller.section.previewPosition(value)
            else root.controller.section.position = value
        }
        onInteractionCommitted: root.controller.section.commitInteraction()
        onInteractionCanceled: root.controller.section.cancelInteraction()
    }

    ThemedMenu {
        id: componentContextMenu
        theme: root.theme
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Fit selection"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.fitSelection() }
        ThemedMenuSeparator { theme: componentContextMenu.theme }
        ThemedMenuItem {
            theme: componentContextMenu.theme
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Restore") : qsTr("Isolate")
            enabled: root.controller && root.controller.activeNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Hide"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Hide others"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideOtherNodes() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Restore"); enabled: root.controller && root.controller.canRestoreVisibility; onTriggered: root.controller.restoreVisibility() }
        ThemedMenuSeparator { theme: componentContextMenu.theme }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Properties"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.toolRequested("properties") }
    }

    ThemedMenu {
        id: exportComponentContextMenu
        theme: root.theme
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Fit selection"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.fitSelection() }
        ThemedMenuSeparator { theme: exportComponentContextMenu.theme }
        ThemedMenuItem {
            theme: exportComponentContextMenu.theme
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Restore") : qsTr("Isolate")
            enabled: root.controller && root.controller.activeNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Hide"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
    }

    ThemedMenu {
        id: viewportContextMenu
        theme: root.theme
        ThemedMenuItem { theme: viewportContextMenu.theme; text: qsTr("Fit assembly"); onTriggered: root.fitCamera() }
        ThemedMenuItem { theme: viewportContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
    }

    ThemedComboBox {
        id: displayModeControl
        theme: root.theme
        visible: !root.presentationOnly && !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 12
        implicitWidth: 132
        model: [qsTr("Solid"), qsTr("Solid + Edges"), qsTr("Edges Only")]
        currentIndex: root.renderMode
        Accessible.name: qsTr("Display style")
        onActivated: index => root.renderMode = index
        ThemedToolTip {
            theme: root.theme
            text: qsTr("Display style")
            visible: parent.hovered
        }
    }

    ProjectionControl {
        id: projectionControl
        theme: root.theme
        projectionMode: navigation.projectionMode
        visible: !root.presentationOnly && !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: displayModeControl.left
        anchors.topMargin: 12
        anchors.rightMargin: 8
        onProjectionRequested: function(mode) { navigation.setProjection(mode) }
    }

    ViewCube {
        id: viewCube
        theme: root.theme
        visible: !root.presentationOnly && !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: displayModeControl.height + 22
        anchors.rightMargin: 14
        onViewRequested: function(normal) { root.setStandardView(normal) }
    }

    Shortcut { sequence: "F"; enabled: !root.presentationOnly; onActivated: root.fitCamera() }
    Shortcut { sequence: "Shift+F"; enabled: !root.presentationOnly; onActivated: root.fitSelection() }
    Shortcut { sequence: "I"; enabled: !root.presentationOnly; onActivated: if (root.controller) root.controller.toggleIsolateActiveNode() }
    Shortcut { sequence: "H"; enabled: !root.presentationOnly; onActivated: if (root.controller) root.controller.hideActiveNode() }
    Shortcut { sequence: "Shift+H"; enabled: !root.presentationOnly; onActivated: if (root.controller) root.controller.hideOtherNodes() }
    Shortcut { sequence: "Ctrl+Shift+H"; enabled: !root.presentationOnly; onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "Meta+Shift+H"; enabled: !root.presentationOnly; onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "M"; enabled: !root.presentationOnly; onActivated: root.toolRequested("measure") }
    Shortcut { sequence: "S"; enabled: !root.presentationOnly; onActivated: root.toolRequested("section") }
    Shortcut { sequences: ["1"]; enabled: !root.presentationOnly; onActivated: root.renderMode = 0 }
    Shortcut { sequences: ["2"]; enabled: !root.presentationOnly; onActivated: root.renderMode = 1 }
    Shortcut { sequences: ["3"]; enabled: !root.presentationOnly; onActivated: root.renderMode = 2 }
    // Ctrl/Cmd+O is owned by the File > Open STEP… menu item (Main.qml) so
    // there is exactly one shortcut registration for it at window scope.
    Shortcut {
        sequence: "Esc"
        enabled: !root.presentationOnly && sectionArrowInput.dragging
        onActivated: sectionArrowInput.cancelDrag()
    }
    Shortcut {
        sequence: "Esc"
        enabled: !root.presentationOnly && !sectionArrowInput.dragging
        onActivated: {
            if (root.controller && root.controller.canRestoreVisibility) root.controller.restoreVisibility()
            else root.toolRequested("close")
        }
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.clearMeshes() }
        function onMeshReady(nodeId, segmentKey, sourceColor, meshJson) { root.appendMesh(nodeId, segmentKey, sourceColor, meshJson) }
        function onEdgeReady(nodeId, edgeJson) { root.appendEdges(nodeId, edgeJson) }
        function onFitRequested() { root.fitSelection() }
        function onVisibilityChanged() { root.applyPresentation() }
        function onComponentPropertiesChanged() { root.applyAppearance() }
        function onActiveNodeIdChanged() { root.applyRenderMode() }
    }
    onRenderModeChanged: applyRenderMode()
    onDisplayOnlyNodeIdChanged: {
        applyRenderMode()
        Qt.callLater(fitDisplayFilter)
    }
    onExternalHighlightNodeIdChanged: applyPresentation()
    Connections {
        target: root.controller ? root.controller.section : null
        function onChanged() { root.applySection() }
    }
    Connections {
        target: root.controller ? root.controller.measurement : null
        function onModeChanged() { root.clearMeasurementHighlights() }
        function onResultChanged() {
            if (root.controller && root.controller.measurement.pickedEntities.length === 0)
                root.clearMeasurementHighlights()
        }
    }
}
