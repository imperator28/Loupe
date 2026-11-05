import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick3D
import Loupe.App

Item {
    id: root
    objectName: presentationOnly ? "previewViewport" : "inspectViewport"
    focus: true

    property QtObject controller
    property QtObject theme
    property bool presentationOnly: false
    // presentationOnly hides the full interactive toolbar (section, measure,
    // shortcuts, ...), but a presentation host (e.g. Export's preview panes)
    // may still want the user to be able to switch Solid/Solid+Edges/Edges
    // Only, so that one control is independently switchable.
    property bool renderModeControlVisible: !presentationOnly
    // The view cube belongs in every viewport the user can orbit, including the
    // presentation-only embedded previews, so it is opt-in the same way the
    // display-mode control above is rather than being tied to presentationOnly.
    property bool viewCubeVisible: !presentationOnly
    property bool selectionEnabled: true
    // Drawing workspace: a left click reports the whole face's frame instead of selecting.
    // Off everywhere else, so no existing viewport changes behaviour.
    property bool faceFrameSelectionEnabled: false
    property bool componentHoverEnabled: true
    property bool contextActionsEnabled: !presentationOnly
    // Component actions are export-specific. A presentation viewport can still want the
    // viewport menu for Fit without offering them.
    property bool componentContextActionsEnabled: true
    property bool requireDisplayFilter: false
    property string displayOnlyNodeId: ""
    property string externalHighlightNodeId: ""
    property var modelByNode: ({})
    property var geometryByNode: ({})
    property var edgeModelByNode: ({})
    property var edgeGeometryByNode: ({})
    // One section cap per part, keyed by node: the worker sends a part as one mesh per colour,
    // and a cut through any one of them alone is open, so it could not be filled.
    property var sectionOverlayModelByNode: ({})
    property var sectionOverlayGeometryByNode: ({})
    // nodeId -> the segment keys of its meshes, and the parts whose cap must be rebuilt from
    // them because one changed.
    property var segmentKeysByNode: ({})
    property var sectionSourceDirtyByNode: ({})
    property var xrayModelBySegment: ({})
    property var boundsByNode: ({})
    // The exploded view (#17). Each part's models live under one node per part, which is
    // what the exploded layout moves. Only the Inspect viewport shows that layout; Export
    // and Drawing keep showing the assembly as the file has it.
    property bool explodedLayoutEnabled: false
    // The explode tool is open: its handles show on the selection.
    property bool explodeToolActive: false
    readonly property QtObject explode: controller && controller.explode ? controller.explode : null
    readonly property bool explodedLayout: explodedLayoutEnabled && explode !== null && explode.exploded
    property var partNodeByNode: ({})
    property var xrayPartNodeByNode: ({})
    // Parts are moving: section caps, built for where the parts were, wait until they stop.
    property bool explodeSettling: false
    // The controller revision each segment (or node's edges) was last built from. A replay
    // hands every viewport everything again; one that already holds a revision skips it
    // rather than decoding the same payload a second time.
    property var meshRevisionBySegment: ({})
    property var edgeRevisionByNode: ({})
    // A viewport that only ever shows one part at a time (requireDisplayFilter) keeps the
    // payloads of every other part here, unbuilt, and builds a part only when it is shown.
    // Building all of them is what made entering Export or Drawing on a large assembly
    // slow: every segment became a geometry and two models to show perhaps one of them.
    // Keyed nodeId -> segmentKey -> entry, so a display change walks parts, not segments.
    property var pendingMeshesByNode: ({})
    property var pendingEdgesByNode: ({})
    // Copies of a part draw one shared mesh, kept here by its key (as a payload until a copy
    // first needs it); each copy is a model of its own, placed by its part node. Edges are clipped
    // per part, so each copy takes its own copy of the shared edges.
    property var sharedGeometryByKey: ({})
    property var sharedPayloadByKey: ({})
    property var sharedEdgeGeometryByKey: ({})
    property var sharedEdgePayloadByKey: ({})
    property var sharedMeshRevisionByKey: ({})
    property var sharedEdgesRevisionByKey: ({})
    // Segment keys whose geometry is shared, and the copies' segment keys by shared key.
    property var sharedSegment: ({})
    property var instanceSegmentsByKey: ({})
    // Where each copy of a shared mesh goes: { position, rotation } in assembly coordinates.
    property var placementByNode: ({})
    readonly property bool lazyMaterialization: requireDisplayFilter
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
    // Section builds run on worker threads: several at once, so a large assembly is not
    // finalized one part after another.
    readonly property int maximumConcurrentSectionBuilds: 6
    // Parts handed to their geometry per pass of the event loop. Parts the plane misses
    // finish on the spot, so a pass gets through many of them; the cap keeps the UI live.
    readonly property int sectionItemsPerPass: 150
    property var hoverTopology: null
    // Where the pointer is, for the measurement hover chip.
    property point hoverPointer: Qt.point(0, 0)
    // How close, in pixels, the pointer must be to take an edge, and an edge's end.
    readonly property real measureEdgePixels: 7
    readonly property real measureVertexPixels: 10
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
    // The screen's up direction in the scene: an exploded drawing is oriented by it.
    readonly property vector3d viewUpDirection: navigation.revision >= 0
        ? navigation.orientation.times(Qt.vector3d(0, 1, 0)) : Qt.vector3d(0, 1, 0)
    // The direction from the scene toward the camera.
    readonly property vector3d viewDirection: navigation.revision >= 0
        ? navigation.orientation.times(Qt.vector3d(0, 0, 1)) : Qt.vector3d(0, 0, 1)
    readonly property vector3d sceneCenter: Qt.vector3d((sceneMinimum.x + sceneMaximum.x) * 0.5,
                                                        (sceneMinimum.y + sceneMaximum.y) * 0.5,
                                                        (sceneMinimum.z + sceneMaximum.z) * 0.5)
    // The perspective camera's depth range, fitted around what is drawn. A fixed 0.01 to
    // 1,000,000 left the depth buffer far too coarse at any real distance, so faces a
    // fraction of a millimetre apart (a tape on a panel, two coincident shells) fought and
    // the model came out in stripes and shades. Orthographic depth is linear and was fine.
    readonly property var depthBounds: {
        const revision = explode ? explode.revision : -1
        const exploded = revision >= 0 ? explodedBounds([]) : null
        const low = exploded ? Qt.vector3d(Math.min(sceneMinimum.x, exploded.minimum.x), Math.min(sceneMinimum.y, exploded.minimum.y),
                                           Math.min(sceneMinimum.z, exploded.minimum.z)) : sceneMinimum
        const high = exploded ? Qt.vector3d(Math.max(sceneMaximum.x, exploded.maximum.x), Math.max(sceneMaximum.y, exploded.maximum.y),
                                            Math.max(sceneMaximum.z, exploded.maximum.z)) : sceneMaximum
        const centre = Qt.vector3d((low.x + high.x) * 0.5, (low.y + high.y) * 0.5, (low.z + high.z) * 0.5)
        const radius = Math.max(0.5, high.minus(low).length() * 0.5)
        return { centre: centre, radius: radius }
    }
    // How far in front of the camera the scene's centre is, along the view: a panned camera
    // is further from the centre than that, and fitting to the straight-line distance would
    // clip the near side.
    readonly property real perspectiveDepthDistance: {
        const revision = navigation.revision
        if (revision < 0) return 0
        const forward = navigation.orientation.times(Qt.vector3d(0, 0, -1))
        return depthBounds.centre.minus(navigation.cameraPosition).dotProduct(forward)
    }
    // Planes that are not finite (bounds or camera not settled yet) fall back to the old
    // fixed range rather than clip everything away.
    readonly property bool clipPlanesValid: hasSceneBounds && isFinite(perspectiveDepthDistance)
                                            && isFinite(depthBounds.radius) && depthBounds.radius > 0
    readonly property real perspectiveClipNear: clipPlanesValid
        ? Math.max(depthBounds.radius * 0.0005, perspectiveDepthDistance - depthBounds.radius * 1.25) : 0.01
    readonly property real perspectiveClipFar: clipPlanesValid
        ? Math.max(perspectiveClipNear * 4, perspectiveDepthDistance + depthBounds.radius * 1.5) : 1000000
    // Orthographic depth is linear, but 2,000,000 mm of it still left ~0.1 mm steps. An
    // orthographic picture does not depend on how far back the camera is, so its cameras
    // sit back far enough that the near plane is always in front of the model: a camera
    // inside the model meant a near plane behind it, which not every graphics backend
    // draws the same way.
    readonly property real orthographicBackoff: clipPlanesValid
        ? Math.max(0, depthBounds.radius * 2.5 - perspectiveDepthDistance) : 0
    readonly property real orthographicClipNear: clipPlanesValid
        ? perspectiveDepthDistance + orthographicBackoff - depthBounds.radius * 1.5 : -1000000
    readonly property real orthographicClipFar: clipPlanesValid
        ? perspectiveDepthDistance + orthographicBackoff + depthBounds.radius * 1.5 : 1000000
    // Edges are drawn a little in front of the faces they outline. The shader takes that
    // as a share of the depth range, so it is worked out here from the planes to come to
    // the same distance in the model whatever the range: as a fixed share it shrank to
    // thousandths of a millimetre once the planes fitted the model, and edges broke into
    // dashes where they met the tessellated faces.
    readonly property real edgeDepthBiasDistance: Math.min(5, Math.max(0.02, depthBounds.radius * 0.0004))
    readonly property real edgeDepthBias: {
        if (!clipPlanesValid) return 0.0000002
        if (navigation.projectionMode === "orthographic")
            return 2 * edgeDepthBiasDistance / Math.max(0.001, orthographicClipFar - orthographicClipNear)
        const n = perspectiveClipNear
        const f = perspectiveClipFar
        const d = Math.max(n, perspectiveDepthDistance)
        return edgeDepthBiasDistance * 2 * f * n / Math.max(0.000001, (f - n) * d * d)
    }
    readonly property real sceneDiagonal: {
        const x = sceneMaximum.x - sceneMinimum.x
        const y = sceneMaximum.y - sceneMinimum.y
        const z = sceneMaximum.z - sceneMinimum.z
        return Math.max(0.001, Math.sqrt(x * x + y * y + z * z))
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
    // Carries MeshGeometry::faceFrameFor's result: normal, centroid, deviation, area.
    // Empty-map results are not emitted, so a listener never sees a default frame.
    signal faceFrameSelected(var frame)
    signal openFileRequested()
    // The empty-space menu's Save picture…: the workspace owns the file dialog.
    signal savePictureRequested()

    // ---- Exploded-view handles (#17) ----
    // The selection the handles move, where it is drawn, and how big the handles are: a
    // constant size on screen, wherever the selection is and however far the camera is.
    readonly property var handleNodeIds: controller && controller.selectedNodeIds ? controller.selectedNodeIds : []
    readonly property var handleBounds: {
        const revision = explode ? explode.revision : -1
        if (!explode || handleNodeIds.length === 0 || revision < 0) return null
        const bounds = explode.displayedBounds(handleNodeIds)
        return bounds.valid ? bounds : null
    }
    // Where the handle sits and turns about: the selection's centre, or a point the user
    // picked on a part (#17), which goes along with that part as it moves.
    property var handlePivot: null
    // Every pivot picked this session, by the parts it was picked for: selecting those
    // parts again brings their pivot back, until Use centre clears it.
    property var savedPivots: ({})
    function pivotKey() {
        return controller ? controller.selectedNodeIds.slice().sort().join("|") : ""
    }
    // Turn the handle to the face the pivot was picked on, so its arrows run along and off
    // that face rather than along the world's axes.
    property bool handleAlignToFace: false
    // The next click on a part picks the pivot.
    property bool pickingPivot: false
    readonly property vector3d handlePivotPoint: {
        const revision = explode ? explode.revision : -1
        const node = handlePivot && revision >= 0 ? partNodeByNode[handlePivot.nodeId] : null
        return node ? node.mapPositionToScene(handlePivot.local) : Qt.vector3d(0, 0, 0)
    }
    // Without a picked pivot, the selection's own centre as it has been moved: a point fixed
    // to the parts. Their bounding box's centre shifts as they turn, so turning about it made
    // every turn after the first carry the parts off a little further.
    readonly property vector3d handleCentre: {
        const revision = explode ? explode.revision : -1
        if (handlePivot && partNodeByNode[handlePivot.nodeId]) return handlePivotPoint
        if (!handleBounds || revision < 0) return Qt.vector3d(0, 0, 0)
        return explode.movedCentre(handleNodeIds)
    }
    // The handle's orientation: the world's, or the picked face's.
    readonly property quaternion handleFrame: {
        const revision = explode ? explode.revision : -1
        const node = handlePivot && handleAlignToFace && handlePivot.normal && revision >= 0
                     ? partNodeByNode[handlePivot.nodeId] : null
        if (!node || !explode) return Qt.quaternion(1, 0, 0, 0)
        return explode.frameForNormal(node.mapDirectionToScene(handlePivot.normal))
    }
    readonly property real handlePixels: 96
    readonly property real handleLength: worldPerPixelAt(handleCentre) * handlePixels
    // Shown on the layout and on the assembly alike, hidden only while the view eases between
    // the two. On the assembly a drag starts a new layout from where the parts are assembled,
    // and the one kept aside is saved first, so nothing is lost by starting over.
    readonly property bool handlesShown: explodedLayoutEnabled && explodeToolActive && explode !== null
            && !presentationOnly && !captureUiHidden && handleBounds !== null
            && (explode.blend > 0.999 || explode.blend < 0.001)
            && !pickingPivot
    // The handle under the pointer, and the drag in progress: {kind, axis, ...}.
    property var handleHover: null
    property var handleDrag: null
    // What the drag has moved so far, for the readout by the pointer.
    property string handleReadout: ""
    property point handleReadoutPosition: Qt.point(0, 0)
    // Auto-explode and reset run as short animations over the controller's state.
    property real autoExplodeAnimationFactor: 0

    ExplodeHandleMath { id: handleMath }

    function worldPerPixelAt(point) {
        if (navigation.projectionMode === "orthographic")
            return 1 / Math.max(0.000001, navigation.orthographicMagnification)
        const x = navigation.cameraPosition.x - point.x
        const y = navigation.cameraPosition.y - point.y
        const z = navigation.cameraPosition.z - point.z
        const distance = Math.max(0.001, Math.sqrt(x * x + y * y + z * z))
        return 2 * distance * Math.tan(navigation.perspectiveFieldOfView * Math.PI / 360)
                / Math.max(1, view.height)
    }

    function handleAxis(axis) {
        const local = axis === 0 ? Qt.vector3d(1, 0, 0) : axis === 1 ? Qt.vector3d(0, 1, 0) : Qt.vector3d(0, 0, 1)
        return handleFrame.times(local)
    }

    // The two directions spanning the plane a ring lies in.
    function handleRingBasis(axis) {
        return [handleAxis((axis + 1) % 3), handleAxis((axis + 2) % 3)]
    }

    // Start picking the pivot: the next click on a part puts the handle there.
    function beginPivotPick() {
        if (!explodedLayoutEnabled) return
        pickingPivot = true
    }

    function cancelPivotPick() {
        pickingPivot = false
    }

    function clearPivot() {
        const saved = savedPivots
        delete saved[pivotKey()]
        savedPivots = saved
        handlePivot = null
        pickingPivot = false
    }

    // The point (and face) under (x, y) as the pivot, in the part's own frame so it stays on
    // the part wherever the part is moved. False when the click missed every part.
    function pickPivotAt(x, y) {
        const hit = view.pick(x, y)
        if (!hit.objectHit || !hit.objectHit.nodeId || !hit.scenePosition) return false
        const model = hit.objectHit
        const normal = hit.sceneNormal ? model.mapDirectionFromScene(hit.sceneNormal) : null
        handlePivot = { nodeId: model.nodeId, local: model.mapPositionFromScene(hit.scenePosition),
                        normal: normal && normal.length() > 0 ? normal.normalized() : null }
        const saved = savedPivots
        saved[pivotKey()] = handlePivot
        savedPivots = saved
        pickingPivot = false
        return true
    }

    // Saved views (Views tool window). The one last gone to stays marked as current until
    // the camera, or what is shown, changes.
    property string currentCameraViewId: ""
    property int currentCameraRevision: -1
    property string currentDisplayStateId: ""
    property int currentDisplayRevision: -1
    readonly property string activeCameraViewId: currentCameraViewId.length > 0
        && (navigation.tween.running || navigation.revision === currentCameraRevision) ? currentCameraViewId : ""
    readonly property string activeDisplayStateId: currentDisplayStateId.length > 0 && controller
        && controller.visibilityRevision === currentDisplayRevision ? currentDisplayStateId : ""
    Connections {
        target: navigation.tween
        function onStopped() { root.currentCameraRevision = navigation.revision }
    }

    // What a camera view looks along, in a word: a standard view when it is one.
    function cameraViewSummary() {
        const projection = navigation.projectionMode === "perspective" ? qsTr("Perspective") : qsTr("Orthographic")
        const d = viewDirection
        const named = [[Qt.vector3d(0, 0, 1), qsTr("Front")], [Qt.vector3d(0, 0, -1), qsTr("Back")],
                       [Qt.vector3d(0, 1, 0), qsTr("Top")], [Qt.vector3d(0, -1, 0), qsTr("Bottom")],
                       [Qt.vector3d(1, 0, 0), qsTr("Right")], [Qt.vector3d(-1, 0, 0), qsTr("Left")]]
        for (const entry of named) {
            if (d.dotProduct(entry[0]) > 0.996) return projection + " · " + entry[1]
        }
        return projection
    }

    // A small picture of the view as it is now, saved as an entry's thumbnail.
    function saveViewThumbnail(id) {
        if (!controller || id.length === 0 || view.width <= 0 || view.height <= 0) return
        const scale = Math.min(1, 240 / view.width)
        view.grabToImage(function(result) {
            if (root.controller && result.saveToFile(root.controller.savedViews.thumbnailPath(id)))
                root.controller.savedViews.thumbnailSaved(id)
        }, Qt.size(Math.round(view.width * scale), Math.round(view.height * scale)))
    }

    function saveCameraView(name) {
        if (!controller) return ""
        const id = controller.saveCameraView(name, navigation.cameraState(), cameraViewSummary())
        if (id.length > 0) {
            currentCameraViewId = id
            currentCameraRevision = navigation.revision
            saveViewThumbnail(id)
        }
        return id
    }

    function goToCameraView(id) {
        if (!controller) return false
        const state = controller.cameraView(id)
        const duration = root.theme && root.theme.durSpatial !== undefined ? Math.round(root.theme.durSpatial * 1.5) : 0
        if (!navigation.restoreCameraState(state, duration)) return false
        currentCameraViewId = id
        if (duration <= 0) currentCameraRevision = navigation.revision
        return true
    }

    function saveDisplayState(name) {
        if (!controller) return ""
        const id = controller.saveDisplayState(name)
        if (id.length > 0) {
            currentDisplayStateId = id
            currentDisplayRevision = controller.visibilityRevision
            saveViewThumbnail(id)
        }
        return id
    }

    // Saved exploded layouts. Going to one eases the parts out of the assembly to it.
    readonly property string activeExplodeLayoutId: {
        const revision = explode ? explode.revision : -1
        const showing = explode ? explode.blend > 0.5 : false
        const saved = controller ? controller.savedViews.explodeLayouts : []
        return controller && revision >= 0 && showing && saved.length > 0 ? controller.currentExplodeLayoutId() : ""
    }

    function saveExplodeLayout(name) {
        if (!controller) return ""
        const id = controller.saveExplodeLayout(name)
        if (id.length > 0) saveViewThumbnail(id)
        return id
    }

    function updateExplodeLayout(id) {
        if (!controller || !controller.updateExplodeLayout(id)) return false
        saveViewThumbnail(id)
        return true
    }

    function goToExplodeLayout(id) {
        if (!controller || !explode) return false
        explodeResetAnimation.stop()
        layoutViewAnimation.stop()
        autoExplodeAnimation.stop()
        if (!controller.applyExplodeLayout(id)) return false
        showExploded()
        return true
    }

    function goToDisplayState(id) {
        if (!controller || !controller.applyDisplayState(id)) return false
        currentDisplayStateId = id
        currentDisplayRevision = controller.visibilityRevision
        return true
    }

    // The Assembled / Exploded switch: ease the view to the assembly, or back to the layout,
    // keeping the layout either way.
    function showAssembled() {
        if (!explode || !explode.exploded) return
        explodeResetAnimation.stop()
        layoutViewAnimation.to = 0
        layoutViewAnimation.restart()
    }

    function showExploded() {
        if (!explode) return
        explodeResetAnimation.stop()
        layoutViewAnimation.to = 1
        layoutViewAnimation.restart()
    }

    NumberAnimation {
        id: layoutViewAnimation
        target: root.explode
        property: "blend"
        duration: root.theme ? root.theme.durSpatial * 2 : 0
        easing.type: Easing.InOutCubic
    }

    Connections {
        target: root.controller
        // The pivot belongs to the parts it was picked for: theirs, or the centre.
        function onSelectionChanged() {
            const saved = root.savedPivots[root.pivotKey()]
            root.handlePivot = saved !== undefined ? saved : null
        }
        // A new document starts without pivots.
        function onDocumentNameChanged() { root.savedPivots = ({}); root.handlePivot = null }
    }

    function screenPoint(scenePoint) {
        const point = view.mapFrom3DScene(scenePoint)
        return { x: point.x, y: point.y }
    }

    // The handles as drawn on screen, for hit-testing and dragging.
    function handleLayout() {
        if (!handlesShown) return null
        const centre = handleCentre
        const length = handleLength
        const radius = length * 0.72
        const layout = { centre: screenPoint(centre), tips: [], rings: [], ringPoints: [] }
        for (let axis = 0; axis < 3; ++axis) {
            layout.tips.push(screenPoint(centre.plus(handleAxis(axis).times(length * 0.98))))
            const basis = handleRingBasis(axis)
            const points = []
            const scenePoints = []
            for (let step = 0; step < 48; ++step) {
                const angle = step / 48 * 2 * Math.PI
                const scenePoint = centre.plus(basis[0].times(radius * Math.cos(angle)))
                                         .plus(basis[1].times(radius * Math.sin(angle)))
                scenePoints.push(scenePoint)
                points.push(screenPoint(scenePoint))
            }
            layout.rings.push(points)
            layout.ringPoints.push(scenePoints)
        }
        return layout
    }

    function handleAt(x, y) {
        return handleMath.handleAt(x, y, handleLayout())
    }

    function updateHandleHover(x, y) {
        const hit = handlesShown ? handleAt(x, y) : null
        const same = hit && handleHover && hit.kind === handleHover.kind && hit.axis === handleHover.axis
        if (!same && (hit || handleHover)) handleHover = hit
        return hit !== null
    }

    function beginHandleDrag(hit, x, y) {
        const layout = handleLayout()
        if (!layout || !explode) return false
        // Copied, not referenced: a vector3d read from a property stays tied to it, and the
        // handle's centre and frame move with the parts as they turn. Turning about a pivot
        // that followed each step flung the parts off, further the further the drag went.
        const copy = function(vector) { return Qt.vector3d(vector.x, vector.y, vector.z) }
        const axis = copy(handleAxis(hit.axis))
        const centre = copy(handleCentre)
        const drag = { kind: hit.kind, axis: hit.axis, axisVector: axis, startX: x, startY: y, centre: centre,
                       centreScreen: layout.centre, value: 0 }
        if (hit.kind === "free") {
            // In the plane of the screen through the handle, at the scale it is drawn at.
            drag.right = copy(navigation.orientation.times(Qt.vector3d(1, 0, 0)))
            drag.up = copy(navigation.orientation.times(Qt.vector3d(0, 1, 0)))
            drag.worldPerPixel = worldPerPixelAt(centre)
        } else if (hit.kind === "move") {
            drag.axisX = layout.tips[hit.axis].x - layout.centre.x
            drag.axisY = layout.tips[hit.axis].y - layout.centre.y
            drag.millimetres = handleLength * 0.98
        } else {
            // A ring seen face-on turns with the pointer's angle about the centre. Seen
            // edge-on that angle means nothing, so the drag follows the ring's tangent where
            // it was grabbed.
            const facing = axis.dotProduct(viewDirection)
            if (Math.abs(facing) > 0.25) {
                drag.mode = "angle"
                drag.sign = facing > 0 ? -1 : 1
                drag.lastAngle = handleMath.screenAngle(x, y, layout.centre)
                drag.turned = 0
            } else {
                drag.mode = "tangent"
                const points = layout.rings[hit.axis]
                let nearest = 0
                let best = Number.POSITIVE_INFINITY
                for (let index = 0; index < points.length; ++index) {
                    const dx = points[index].x - x
                    const dy = points[index].y - y
                    if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; nearest = index }
                }
                const grabbed = layout.ringPoints[hit.axis][nearest]
                const radius = handleLength * 0.72
                const tangent = axis.crossProduct(grabbed.minus(centre)).normalized()
                const ahead = screenPoint(grabbed.plus(tangent.times(radius * 0.2)))
                const here = screenPoint(grabbed)
                drag.tangentX = ahead.x - here.x
                drag.tangentY = ahead.y - here.y
                // Grabbed where the ring runs straight at the eye, its tangent is a few pixels
                // long, and a pixel of drag was many degrees of turn. Keep a fifth of a radian
                // at least a sixth of the ring's on-screen radius away.
                const pixels = Math.hypot(drag.tangentX, drag.tangentY)
                const minimum = Math.max(6, Math.hypot(here.x - layout.centre.x, here.y - layout.centre.y) / 6)
                if (pixels < 1e-3) {
                    drag.tangentX = -(here.y - layout.centre.y)
                    drag.tangentY = here.x - layout.centre.x
                }
                const length = Math.max(1e-3, Math.hypot(drag.tangentX, drag.tangentY))
                if (length < minimum) {
                    drag.tangentX *= minimum / length
                    drag.tangentY *= minimum / length
                }
            }
        }
        explode.beginInteraction(handleNodeIds)
        if (!explode.interacting) return false
        handleDrag = drag
        handleReadoutPosition = Qt.point(x, y)
        handleReadout = ""
        return true
    }

    function updateHandleDrag(x, y, free) {
        const drag = handleDrag
        if (!drag || !explode) return
        if (drag.kind === "free") {
            const travel = handleMath.screenPlaneTravel(x - drag.startX, y - drag.startY, drag.right, drag.up, drag.worldPerPixel)
            // Snapping steps each axis on its own, so a free move still lands on round numbers.
            const snapped = Qt.vector3d(explode.snapTranslation(travel.x, free), explode.snapTranslation(travel.y, free),
                                        explode.snapTranslation(travel.z, free))
            explode.previewTranslation(snapped)
            const digits = function(value) { return value.toFixed(Math.abs(value) < 10 ? 2 : 1) }
            handleReadout = qsTr("X %1  Y %2  Z %3 mm").arg(digits(snapped.x)).arg(digits(snapped.y)).arg(digits(snapped.z))
            handleReadoutPosition = Qt.point(x, y)
            return
        }
        const axis = drag.axisVector
        if (drag.kind === "move") {
            const travel = handleMath.axisTravel(x - drag.startX, y - drag.startY, drag.axisX, drag.axisY, drag.millimetres)
            const snapped = explode.snapTranslation(travel, free)
            explode.previewTranslation(axis.times(snapped))
            drag.value = snapped
            handleReadout = qsTr("%1 mm").arg(snapped.toFixed(Math.abs(snapped) < 10 ? 2 : 1))
        } else {
            let degrees = 0
            if (drag.mode === "angle") {
                // Over the centre the pointer's angle is noise, and crossing it flips by half
                // a turn: hold the angle there until the pointer is clear again.
                if (Math.hypot(x - drag.centreScreen.x, y - drag.centreScreen.y) > 8) {
                    const angle = handleMath.screenAngle(x, y, drag.centreScreen)
                    drag.turned += handleMath.angleStep(drag.lastAngle, angle)
                    drag.lastAngle = angle
                }
                degrees = drag.sign * drag.turned
            } else {
                degrees = handleMath.axisTravel(x - drag.startX, y - drag.startY, drag.tangentX, drag.tangentY, 0.2) * 180 / Math.PI
            }
            const snapped = explode.snapRotation(degrees, free)
            explode.previewRotation(axis, snapped, drag.centre)
            drag.value = snapped
            handleReadout = qsTr("%1°").arg(snapped.toFixed(1))
        }
        handleReadoutPosition = Qt.point(x, y)
    }

    function endHandleDrag(commit) {
        if (!handleDrag) return
        if (explode) {
            if (commit) explode.commitInteraction()
            else explode.cancelInteraction()
        }
        handleDrag = null
        handleReadout = ""
    }

    // Bring every part home with a short glide, then forget where they were moved.
    function animateExplodeReset() {
        if (!explode || !explode.exploded) return
        autoExplodeAnimation.stop()
        layoutViewAnimation.stop()
        explodeResetAnimation.restart()
    }

    // Auto-explode the scope ("" for the whole model) to `factor`, flying the parts apart.
    function animateAutoExplode(scopeId, factor) {
        if (!explode) return
        explodeResetAnimation.stop()
        layoutViewAnimation.stop()
        autoExplodeAnimation.stop()
        explode.beginAutoExplode(scopeId)
        if (!explode.interacting) return
        autoExplodeAnimationFactor = explode.autoFactor(scopeId)
        autoExplodeAnimation.to = Math.max(0, factor)
        autoExplodeAnimation.restart()
    }

    onAutoExplodeAnimationFactorChanged: if (autoExplodeAnimation.running && explode) explode.previewAutoExplode(autoExplodeAnimationFactor)

    NumberAnimation {
        id: autoExplodeAnimation
        target: root
        property: "autoExplodeAnimationFactor"
        duration: root.theme ? root.theme.durSpatial * 2 : 0
        easing.type: Easing.OutCubic
        onFinished: {
            if (root.explode) {
                root.explode.previewAutoExplode(autoExplodeAnimation.to)
                root.explode.commitInteraction()
            }
        }
    }

    NumberAnimation {
        id: explodeResetAnimation
        target: root.explode
        property: "blend"
        to: 0
        duration: root.theme ? root.theme.durSpatial * 2 : 0
        easing.type: Easing.InOutCubic
        onFinished: if (root.explode) root.explode.resetAll()
    }

    // Parts that move re-cut their section caps once they come to rest, not every frame.
    function explodeMoved() {
        if (presentationOnly || !controller || !controller.section.enabled) return
        if (!explodeSettling) {
            explodeSettling = true
            applyRenderMode()
        }
        explodeSettleTimer.restart()
    }

    Timer {
        id: explodeSettleTimer
        interval: 160
        onTriggered: {
            if (root.explode && root.explode.interacting) {
                restart()
                return
            }
            root.explodeSettling = false
            root.applySection()
        }
    }

    Connections {
        target: root.explode
        function onInteractionChanged() {
            if (root.explode.interacting) layoutViewAnimation.stop()
        }
        function onPartsMoved(partIds) {
            if (!root.explodedLayoutEnabled) return
            root.placeParts(partIds)
            root.explodeMoved()
        }
    }
    onExplodedLayoutEnabledChanged: placeAllParts()
    onHandlesShownChanged: if (!handlesShown) handleHover = null

    ViewportVisualTheme {
        id: viewportVisualTheme
        theme: root.theme
    }

    PointerInputRouter {
        id: pointerInputRouter
    }

    onControllerChanged: {
        updateCaptureViewportSize()
        // A viewport created later -- Export's or Drawing's on first entry -- starts in the
        // mode already chosen elsewhere rather than in Solid.
        if (controller) renderMode = controller.renderMode
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
        const exploded = explodedBounds([])
        if (exploded) navigation.fitBounds(exploded.minimum, exploded.maximum)
        else navigation.fitBounds(sceneMinimum, sceneMaximum)
        pendingFit = false
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

    // The node a part's models hang from, made on first use and placed where the exploded
    // layout has the part.
    function partNode(nodeId) {
        let node = partNodeByNode[nodeId]
        if (!node) {
            node = partNodeComponent.createObject(sceneRoot)
            partNodeByNode[nodeId] = node
            placePart(nodeId)
        }
        return node
    }

    function xrayPartNode(nodeId) {
        let node = xrayPartNodeByNode[nodeId]
        if (!node) {
            node = partNodeComponent.createObject(componentXrayRoot)
            xrayPartNodeByNode[nodeId] = node
            placePart(nodeId)
        }
        return node
    }

    function placePart(nodeId) {
        const moved = explodedLayoutEnabled && explode
        let position = moved ? explode.displayedPosition(nodeId) : Qt.vector3d(0, 0, 0)
        let rotation = moved ? explode.displayedRotation(nodeId) : Qt.quaternion(1, 0, 0, 0)
        // A copy of a shared mesh is drawn through its placement, and then any exploded move.
        const placement = placementByNode[nodeId]
        if (placement) {
            position = position.plus(rotation.times(placement.position))
            rotation = rotation.times(placement.rotation)
        }
        for (const node of [partNodeByNode[nodeId], xrayPartNodeByNode[nodeId]]) {
            if (!node) continue
            node.position = position
            node.rotation = rotation
        }
    }

    function placeParts(nodeIds) {
        for (const nodeId of nodeIds) placePart(nodeId)
    }

    function placeAllParts() {
        for (let nodeId in partNodeByNode) placePart(nodeId)
    }

    // A world section plane in the part's own coordinates, as (normal, offset): the cap and
    // edge clip of a moved part are built against it, so they land on the part as drawn.
    function sectionPlaneFor(nodeId, normal, offset) {
        // A copy's geometry is in its part's own frame: the plane is taken into it through the
        // node the copy is drawn by, which carries the placement and any exploded move.
        const node = placementByNode[nodeId] ? partNodeByNode[nodeId] : null
        if (node) {
            const local = node.mapDirectionFromNode(sceneRoot, normal).normalized()
            const point = node.mapPositionFromNode(sceneRoot, normal.times(offset))
            return Qt.vector4d(local.x, local.y, local.z, local.dotProduct(point))
        }
        if (explodedLayoutEnabled && explode && explode.isMoved(nodeId)) return explode.localPlane(nodeId, normal, offset)
        return Qt.vector4d(normal.x, normal.y, normal.z, offset)
    }

    // Where everything is drawn when the layout is exploded, or null to use the assembly's
    // own bounds. `nodeIds` narrows it to those nodes' parts.
    function explodedBounds(nodeIds) {
        if (!explodedLayout) return null
        const bounds = explode.displayedBounds(nodeIds)
        return bounds.valid ? bounds : null
    }

    // A fit was asked for but the geometry it needs had not arrived yet. Fitting is
    // driven by an asynchronous replay, so whether the request or the geometry lands
    // first is a race -- one that Windows happened to win and macOS to lose, leaving
    // the part off-screen until refitted by hand. Holding the request until it can
    // actually be satisfied removes the timing dependency instead of tuning it.
    property bool pendingFit: false

    function requestFit() {
        pendingFit = true
        Qt.callLater(fitCamera)
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
        if (found) {
            navigation.fitBounds(minimum, maximum)
            pendingFit = false
        }
    }

    function fitSelection() {
        if (!controller || controller.selectedNodeCount === 0) {
            fitCamera()
            return
        }
        const exploded = explodedBounds(controller.selectedNodeIds)
        if (exploded) {
            navigation.fitBounds(exploded.minimum, exploded.maximum)
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

    // Look straight at the cut: from the side the section removes, so the cut face is what
    // faces the camera. Opposite: keep the other half instead and look at its cut. Looking
    // from the kept side, as this used to, showed the part's untouched outside.
    function alignToSection(oppositeSide) {
        if (!controller) return
        if (oppositeSide) controller.section.flipped = !controller.section.flipped
        const removed = sceneSectionManipulator.removedSideNormal(effectiveSectionNormal, controller.section.flipped)
        navigation.alignToNormal(removed)
        Qt.callLater(fitVisibleGeometry)
    }

    function setStandardView(normal) {
        // Reverted to the pre-session implementation after five failed attempts to preserve
        // the zoom without introducing a lag between the cube and the model.
        //
        // This costs the zoom on every cube click, which is a real complaint, but it is the
        // version that demonstrably turns the model in step with the cube. A known imperfect
        // behaviour beats an intermittent one.
        //
        // What is established, for whoever picks this up: the cube renders from
        // navigation.orientation through a binding, the main camera rig is written
        // imperatively by ViewportNavigation.apply(), and the overlay view's camera has its
        // own bindings. A flat-rendered cube alongside an oblique model proves those sources
        // disagree, so the next step is instrumentation -- drive this function and compare the
        // cube's orientation against cameraRig.rotation frame by frame -- not another guess.
        navigation.alignToNormal(normal)
        Qt.callLater(fitVisibleGeometry)
    }

    // A segment's bounds in assembly coordinates, or null when it has none: a copy of a shared
    // mesh is measured as placed, everything else as its geometry stands.
    function segmentBounds(segmentKey) {
        const geometry = geometryByNode[segmentKey]
        if (!geometry) return null
        if (sharedSegment[segmentKey]) {
            const model = modelByNode[segmentKey]
            const placement = model ? placementByNode[model.nodeId] : null
            if (!placement) return null
            const bounds = geometry.boundsUnder(placement.position, placement.rotation)
            return bounds.minimum !== undefined ? bounds : null
        }
        const minimum = Qt.vector3d(geometry.minimumCoordinate(0), geometry.minimumCoordinate(1), geometry.minimumCoordinate(2))
        const maximum = Qt.vector3d(geometry.maximumCoordinate(0), geometry.maximumCoordinate(1), geometry.maximumCoordinate(2))
        if (!isFinite(minimum.x) || !isFinite(minimum.y) || !isFinite(minimum.z)
                || !isFinite(maximum.x) || !isFinite(maximum.y) || !isFinite(maximum.z)) return null
        return { minimum: minimum, maximum: maximum }
    }

    // The bounds of everything currently drawn, or null when nothing is.
    function visibleGeometryBounds() {
        const exploded = explodedBounds([])
        if (exploded) return exploded
        let minimum = Qt.vector3d(0, 0, 0)
        let maximum = Qt.vector3d(0, 0, 0)
        let found = false
        for (let id in geometryByNode) {
            const bounds = segmentBounds(id)
            if (!bounds) continue
            const low = bounds.minimum
            const high = bounds.maximum
            if (!found) {
                minimum = low
                maximum = high
                found = true
            } else {
                minimum = Qt.vector3d(Math.min(minimum.x, low.x), Math.min(minimum.y, low.y),
                                      Math.min(minimum.z, low.z))
                maximum = Qt.vector3d(Math.max(maximum.x, high.x), Math.max(maximum.y, high.y),
                                      Math.max(maximum.z, high.z))
            }
        }
        return found ? { minimum: minimum, maximum: maximum } : null
    }

    function centreVisibleGeometry() {
        const bounds = root.visibleGeometryBounds()
        if (bounds) navigation.centreOn(bounds.minimum, bounds.maximum)
    }

    function fitVisibleGeometry() {
        const exploded = explodedBounds([])
        if (exploded) {
            navigation.fitBounds(exploded.minimum, exploded.maximum)
            return
        }
        let minimum = Qt.vector3d(0, 0, 0)
        let maximum = Qt.vector3d(0, 0, 0)
        let hasVisibleGeometry = false
        for (let id in geometryByNode) {
            const bounds = segmentBounds(id)
            if (!bounds) continue
            const currentMinimum = bounds.minimum
            const currentMaximum = bounds.maximum
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

    function includeGeometryBounds(nodeId, segmentKey) {
        const bounds = segmentBounds(segmentKey)
        if (!bounds) return
        const minimum = bounds.minimum
        const maximum = bounds.maximum
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
        for (let id in geometryByNode) if (!sharedSegment[id]) geometryByNode[id].destroy()
        for (let key in sharedGeometryByKey) sharedGeometryByKey[key].destroy()
        for (let key in sharedEdgeGeometryByKey) sharedEdgeGeometryByKey[key].destroy()
        for (let id in edgeModelByNode) edgeModelByNode[id].destroy()
        for (let id in edgeGeometryByNode) edgeGeometryByNode[id].destroy()
        for (let id in sectionOverlayModelByNode) sectionOverlayModelByNode[id].destroy()
        for (let id in sectionOverlayGeometryByNode) sectionOverlayGeometryByNode[id].destroy()
        for (let id in xrayModelBySegment) xrayModelBySegment[id].destroy()
        for (let id in partNodeByNode) partNodeByNode[id].destroy()
        for (let id in xrayPartNodeByNode) xrayPartNodeByNode[id].destroy()
        partNodeByNode = ({})
        xrayPartNodeByNode = ({})
        modelByNode = ({})
        geometryByNode = ({})
        edgeModelByNode = ({})
        edgeGeometryByNode = ({})
        sectionOverlayModelByNode = ({})
        sectionOverlayGeometryByNode = ({})
        segmentKeysByNode = ({})
        sectionSourceDirtyByNode = ({})
        xrayModelBySegment = ({})
        boundsByNode = ({})
        meshRevisionBySegment = ({})
        edgeRevisionByNode = ({})
        pendingMeshesByNode = ({})
        pendingEdgesByNode = ({})
        sharedGeometryByKey = ({})
        sharedPayloadByKey = ({})
        sharedEdgeGeometryByKey = ({})
        sharedEdgePayloadByKey = ({})
        sharedMeshRevisionByKey = ({})
        sharedEdgesRevisionByKey = ({})
        sharedSegment = ({})
        instanceSegmentsByKey = ({})
        placementByNode = ({})
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

    // Whether the user has hidden this component, as distinct from whether the current
    // display mode happens to draw its solid.
    function nodeShown(nodeId) {
        return nodeVisibleInPreview(nodeId) && (!controller || controller.isNodeVisible(nodeId))
    }

    function sliceOnlyFinalState(section) {
        return section && section.enabled && section.sliceOnly && !section.interacting
                && !sectionFinalizing
    }

    // Transparent parts are context in the full-assembly views. A single-part preview
    // (Export's standalone file, the Drawing view) shows its part as it is.
    function nodeTransparent(nodeId) {
        return !requireDisplayFilter && controller && controller.isNodeTransparent(nodeId)
    }

    function applySolidState(model, sliceOnlyFinal) {
        const shown = nodeShown(model.nodeId)
        model.transparent = nodeTransparent(model.nodeId)
        if (model.transparent) model.transparentOpacity = controller.nodeOpacity(model.nodeId)
        model.visible = renderMode !== 2 && !sliceOnlyFinal && shown
        // Picking follows the user's own hide, not the display mode. A ray
        // pick still reports a model that is merely not drawn, so leaving
        // this true let a hidden part keep swallowing clicks and hovers in
        // front of the parts behind it -- the reason a hidden component was
        // hard to see past or select around. It deliberately does not use
        // `visible`: in Edges Only, and while a 2D slice is showing, the
        // solid is not drawn but its edges are, and those must stay
        // selectable.
        // A transparent part lets clicks through to what is behind it -- seeing past it to
        // the part in focus is the point.
        model.pickable = shown && !model.transparent
    }

    function applyEdgeState(edgeModel, section, sliceOnlyFinal) {
        // The "show edges for a selected part even in Solid mode" override
        // is an interactive-Inspect aid (so a multi-select stays legible
        // without switching modes). A presentation-only host — e.g.
        // Export's preview panes — sets its own displayed node as the
        // shared controller selection too, which would otherwise always
        // trip this override and make its render-mode control a no-op.
        const selected = !root.presentationOnly && controller && controller.isNodeSelected(edgeModel.nodeId)
        const sectionPreview = section && section.enabled && (section.interacting || root.explodeSettling)
        edgeModel.visible = !sliceOnlyFinal && !sectionPreview && (renderMode !== 0 || selected)
                && nodeShown(edgeModel.nodeId)
                // A transparent part keeps its edges unless they were turned off for it.
                && (!nodeTransparent(edgeModel.nodeId) || controller.nodeShowsTransparentEdges(edgeModel.nodeId))
    }

    function applyRenderMode() {
        const section = !presentationOnly && controller ? controller.section : null
        const sliceOnlyFinal = sliceOnlyFinalState(section)
        for (let id in modelByNode) applySolidState(modelByNode[id], sliceOnlyFinal)
        for (let id in edgeModelByNode) applyEdgeState(edgeModelByNode[id], section, sliceOnlyFinal)
        for (let id in sectionOverlayModelByNode) {
            const overlay = sectionOverlayModelByNode[id]
            const hasExactDisplay = section && section.enabled && !section.interacting && !root.explodeSettling
                    && (section.sliceOnly || section.capEnabled)
            overlay.visible = hasExactDisplay && (section.sliceOnly || renderMode !== 2)
                    && (!controller || controller.isNodeVisible(overlay.nodeId))
        }
    }

    // New geometry only needs the section re-applied when a section is actually showing;
    // with it off, a fresh geometry is already in the unsectioned state. Coalesced, so a
    // burst of arriving segments costs one pass rather than one per segment.
    function scheduleSectionForNewGeometry() {
        if (!presentationOnly && controller && controller.section.enabled) Qt.callLater(root.applySection)
    }

    // The colour a part is drawn in. The inspector swatch resolves the same way, so the two
    // cannot disagree; when nothing colours the part, both use the theme's neutral body.
    function displayColor(nodeId, importColor) {
        const resolved = controller ? controller.resolvedAppearanceColor(nodeId, importColor) : importColor
        return resolved && resolved.length > 0 ? resolved : root.theme.neutralBody
    }

    function applyAppearance() {
        if (!controller) return
        for (let id in modelByNode) {
            const model = modelByNode[id]
            model.sourceColor = root.displayColor(model.nodeId, model.importColor)
        }
        for (let nodeId in sectionOverlayModelByNode)
            sectionOverlayModelByNode[nodeId].sourceColor = sectionCapColor(nodeId)
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
            const plane = sectionPlaneFor(id, section.effectiveNormal, section.effectiveOffset)
            edgeGeometryByNode[id].configureSection(section.enabled, plane.x, plane.y, plane.z, plane.w,
                    section.flipped, section.sliceOnly, false)
        }
        const queue = []
        if (section.enabled && (section.sliceOnly || section.capEnabled)) {
            for (let nodeId in segmentKeysByNode) {
                const overlay = ensureSectionOverlay(nodeId)
                if (overlay) queue.push({ geometry: overlay, nodeId: nodeId })
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

    // One part's build started or finished. Counted as it changes: counting every part
    // each time made finishing N parts cost N squared.
    function sectionBuildBusyChanged(busy) {
        sectionBusyCount = Math.max(0, sectionBusyCount + (busy ? 1 : -1))
        if (sectionApplyRequest && sectionApplyIndex < sectionApplyQueue.length
                && sectionBusyCount < maximumConcurrentSectionBuilds
                && !sectionApplyTimer.running) {
            sectionApplyTimer.restart()
        }
    }

    function applyNextSectionItem() {
        const request = sectionApplyRequest
        let handed = 0
        while (request && sectionApplyIndex < sectionApplyQueue.length
               && sectionBusyCount < maximumConcurrentSectionBuilds && handed < sectionItemsPerPass) {
            const item = sectionApplyQueue[sectionApplyIndex]
            sectionApplyIndex += 1
            handed += 1
            const plane = sectionPlaneFor(item.nodeId, request.normal, request.offset)
            item.geometry.configureSection(request.enabled, plane.x, plane.y, plane.z,
                                           plane.w, request.flipped, request.capEnabled, request.sliceOnly,
                                           request.sliceFill, request.sliceOutline, request.preview,
                                           request.outlineWidth)
        }
        // More to hand out and room for it: carry on next pass. Otherwise a finishing
        // build restarts this.
        if (request && sectionApplyIndex < sectionApplyQueue.length
                && sectionBusyCount < maximumConcurrentSectionBuilds)
            sectionApplyTimer.restart()
    }

    // The render state depends on whether the section is still being finalized, not on
    // each part's progress: set it when that changes.
    onSectionFinalizingChanged: applyRenderMode()

    // A part's segments, in the order they arrived.
    function noteSegment(nodeId, segmentKey) {
        let keys = segmentKeysByNode[nodeId]
        if (!keys) {
            keys = []
            segmentKeysByNode[nodeId] = keys
        }
        if (keys.indexOf(segmentKey) < 0) keys.push(segmentKey)
        sectionSourceDirtyByNode[nodeId] = true
    }

    // The cut face takes the colour most of the part is drawn in.
    function sectionCapColor(nodeId) {
        let best = null
        let bestCount = -1
        for (const key of segmentKeysByNode[nodeId] || []) {
            const model = modelByNode[key]
            const geometry = geometryByNode[key]
            if (!model || !geometry) continue
            const count = geometry.sourceTriangleCount()
            if (count > bestCount) {
                best = model.sourceColor
                bestCount = count
            }
        }
        return best !== null ? best : root.theme.neutralBody
    }

    // The part's cap, built from every one of its meshes, and rebuilt when one of them changed.
    function ensureSectionOverlay(nodeId) {
        if (root.presentationOnly) return null
        const keys = segmentKeysByNode[nodeId]
        if (!keys || keys.length === 0) return null
        const sources = []
        for (const key of keys) {
            if (geometryByNode[key]) sources.push(geometryByNode[key])
        }
        if (sources.length === 0) return null
        const existing = sectionOverlayGeometryByNode[nodeId]
        if (existing) {
            if (sectionSourceDirtyByNode[nodeId]) {
                existing.copySectionOverlayFromSources(sources)
                sectionOverlayModelByNode[nodeId].sourceColor = sectionCapColor(nodeId)
                delete sectionSourceDirtyByNode[nodeId]
            }
            return existing
        }

        const overlayGeometry = geometryComponent.createObject(sceneRoot)
        if (!overlayGeometry.copySectionOverlayFromSources(sources)) {
            overlayGeometry.destroy()
            return null
        }
        delete sectionSourceDirtyByNode[nodeId]
        overlayGeometry.sectionBusyChanged.connect(function() { root.sectionBuildBusyChanged(overlayGeometry.sectionBusy) })
        const overlayModel = sectionOverlayModelComponent.createObject(partNode(nodeId), {
            "geometry": overlayGeometry,
            "nodeId": nodeId,
            "sourceColor": sectionCapColor(nodeId)
        })
        sectionOverlayGeometryByNode[nodeId] = overlayGeometry
        sectionOverlayModelByNode[nodeId] = overlayModel
        return overlayGeometry
    }

    function appendMesh(nodeId, segmentKey, sourceColor, meshJson) {
        const revision = controller ? controller.meshRevision(segmentKey) : 0
        if (revision > 0 && meshRevisionBySegment[segmentKey] === revision) return
        meshRevisionBySegment[segmentKey] = revision
        if (lazyMaterialization && !geometryByNode[segmentKey] && !nodeVisibleInPreview(nodeId)) {
            let pending = pendingMeshesByNode[nodeId]
            if (!pending) {
                pending = {}
                pendingMeshesByNode[nodeId] = pending
            }
            pending[segmentKey] = { sourceColor: sourceColor, meshJson: meshJson }
            return
        }
        buildMesh(nodeId, segmentKey, sourceColor, meshJson)
    }

    function buildMesh(nodeId, segmentKey, sourceColor, meshJson) {
        if (geometryByNode[segmentKey]) {
            const geometry = geometryByNode[segmentKey]
            if (!geometry.replaceWorkerMesh(meshJson)) return
            sectionSourceDirtyByNode[nodeId] = true
            const model = modelByNode[segmentKey]
            if (model) {
                model.importColor = sourceColor
                model.sourceColor = root.displayColor(nodeId, model.importColor)
            }
            scheduleSectionForNewGeometry()
            return
        }
        const geometry = geometryComponent.createObject(sceneRoot)
        if (!geometry.appendWorkerMesh(meshJson)) {
            geometry.destroy()
            return
        }
        const model = modelComponent.createObject(partNode(nodeId), { "geometry": geometry, "nodeId": nodeId, "importColor": sourceColor,
                                                    "sourceColor": root.displayColor(nodeId, sourceColor) })
        const xrayModel = componentXrayModel.createObject(xrayPartNode(nodeId), {
            "geometry": geometry,
            "nodeId": nodeId,
            "visible": root.externalHighlightNodeId.length > 0
                    && root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId)
        })
        geometryByNode[segmentKey] = geometry
        modelByNode[segmentKey] = model
        xrayModelBySegment[segmentKey] = xrayModel
        noteSegment(nodeId, segmentKey)
        includeGeometryBounds(nodeId, segmentKey)
        // Only the new model's own state. Re-walking every model here made a replay of N
        // segments cost N^2, which is most of what entering a workspace used to spend.
        const section = !presentationOnly && controller ? controller.section : null
        applySolidState(model, sliceOnlyFinalState(section))
        scheduleSectionForNewGeometry()
    }

    function appendEdges(nodeId, edgeJson) {
        const revision = controller ? controller.edgeRevision(nodeId) : 0
        if (revision > 0 && edgeRevisionByNode[nodeId] === revision) return
        edgeRevisionByNode[nodeId] = revision
        if (lazyMaterialization && !edgeGeometryByNode[nodeId] && !nodeVisibleInPreview(nodeId)) {
            pendingEdgesByNode[nodeId] = edgeJson
            return
        }
        buildEdges(nodeId, edgeJson)
    }

    function buildEdges(nodeId, edgeJson) {
        if (edgeGeometryByNode[nodeId]) {
            edgeGeometryByNode[nodeId].replaceWorkerEdges(edgeJson)
            scheduleSectionForNewGeometry()
            return
        }
        const geometry = edgeGeometryComponent.createObject(sceneRoot)
        if (!geometry.replaceWorkerEdges(edgeJson)) {
            geometry.destroy()
            return
        }
        const model = edgeModelComponent.createObject(partNode(nodeId), { "geometry": geometry, "nodeId": nodeId })
        edgeGeometryByNode[nodeId] = geometry
        edgeModelByNode[nodeId] = model
        const section = !presentationOnly && controller ? controller.section : null
        applyEdgeState(model, section, sliceOnlyFinalState(section))
        scheduleSectionForNewGeometry()
    }

    // A part's shared mesh, made from its payload when a copy first needs it.
    function sharedGeometry(sourceKey) {
        let geometry = sharedGeometryByKey[sourceKey]
        if (geometry) return geometry
        const payload = sharedPayloadByKey[sourceKey]
        if (!payload) return null
        geometry = geometryComponent.createObject(sceneRoot)
        if (!geometry.appendWorkerMesh(payload)) {
            geometry.destroy()
            return null
        }
        sharedGeometryByKey[sourceKey] = geometry
        delete sharedPayloadByKey[sourceKey]
        return geometry
    }

    function appendSharedMesh(sourceKey, sourceColor, meshJson) {
        const revision = controller ? controller.sharedMeshRevision(sourceKey) : 0
        if (revision > 0 && sharedMeshRevisionByKey[sourceKey] === revision) return
        sharedMeshRevisionByKey[sourceKey] = revision
        const geometry = sharedGeometryByKey[sourceKey]
        if (!geometry) {
            sharedPayloadByKey[sourceKey] = meshJson
            return
        }
        // The refined tessellation: every copy draws it at once.
        if (!geometry.replaceWorkerMesh(meshJson)) return
        for (const segmentKey of instanceSegmentsByKey[sourceKey] || []) {
            const model = modelByNode[segmentKey]
            if (model) sectionSourceDirtyByNode[model.nodeId] = true
        }
        scheduleSectionForNewGeometry()
    }

    function notePlacement(nodeId, position, rotation) {
        placementByNode[nodeId] = { position: position, rotation: rotation }
        if (partNodeByNode[nodeId] || xrayPartNodeByNode[nodeId]) placePart(nodeId)
    }

    function appendMeshInstance(nodeId, segmentKey, sourceKey, sourceColor, position, rotation) {
        const revision = controller ? controller.meshRevision(segmentKey) : 0
        if (revision > 0 && meshRevisionBySegment[segmentKey] === revision) return
        meshRevisionBySegment[segmentKey] = revision
        notePlacement(nodeId, position, rotation)
        if (lazyMaterialization && !geometryByNode[segmentKey] && !nodeVisibleInPreview(nodeId)) {
            let pending = pendingMeshesByNode[nodeId]
            if (!pending) {
                pending = {}
                pendingMeshesByNode[nodeId] = pending
            }
            pending[segmentKey] = { sourceKey: sourceKey, sourceColor: sourceColor }
            return
        }
        buildMeshInstance(nodeId, segmentKey, sourceKey, sourceColor)
    }

    function buildMeshInstance(nodeId, segmentKey, sourceKey, sourceColor) {
        const existing = modelByNode[segmentKey]
        if (existing) {
            // The refined pass: the shared mesh it draws was already replaced.
            existing.importColor = sourceColor
            existing.sourceColor = root.displayColor(nodeId, existing.importColor)
            return
        }
        const geometry = sharedGeometry(sourceKey)
        if (!geometry) return
        const model = modelComponent.createObject(partNode(nodeId), { "geometry": geometry, "nodeId": nodeId, "importColor": sourceColor,
                                                    "sourceColor": root.displayColor(nodeId, sourceColor) })
        const xrayModel = componentXrayModel.createObject(xrayPartNode(nodeId), {
            "geometry": geometry,
            "nodeId": nodeId,
            "visible": root.externalHighlightNodeId.length > 0
                    && root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId)
        })
        geometryByNode[segmentKey] = geometry
        sharedSegment[segmentKey] = true
        let copies = instanceSegmentsByKey[sourceKey]
        if (!copies) {
            copies = []
            instanceSegmentsByKey[sourceKey] = copies
        }
        copies.push(segmentKey)
        modelByNode[segmentKey] = model
        xrayModelBySegment[segmentKey] = xrayModel
        noteSegment(nodeId, segmentKey)
        includeGeometryBounds(nodeId, segmentKey)
        const section = !presentationOnly && controller ? controller.section : null
        applySolidState(model, sliceOnlyFinalState(section))
        scheduleSectionForNewGeometry()
    }

    // A part's shared edges, made from their payload when a copy first needs them. Not drawn
    // themselves: each copy draws its own copy, clipped against its own section plane.
    function sharedEdgeGeometry(sourceKey) {
        let geometry = sharedEdgeGeometryByKey[sourceKey]
        if (geometry) return geometry
        const payload = sharedEdgePayloadByKey[sourceKey]
        if (!payload) return null
        geometry = edgeGeometryComponent.createObject(sceneRoot)
        if (!geometry.replaceWorkerEdges(payload)) {
            geometry.destroy()
            return null
        }
        sharedEdgeGeometryByKey[sourceKey] = geometry
        delete sharedEdgePayloadByKey[sourceKey]
        return geometry
    }

    function appendSharedEdges(sourceKey, edgeJson) {
        const revision = controller ? controller.sharedEdgesRevision(sourceKey) : 0
        if (revision > 0 && sharedEdgesRevisionByKey[sourceKey] === revision) return
        sharedEdgesRevisionByKey[sourceKey] = revision
        const geometry = sharedEdgeGeometryByKey[sourceKey]
        // The copies take the refined edges as their instances arrive.
        if (geometry) geometry.replaceWorkerEdges(edgeJson)
        else sharedEdgePayloadByKey[sourceKey] = edgeJson
    }

    function appendEdgeInstance(nodeId, sourceKey, position, rotation) {
        const revision = controller ? controller.edgeRevision(nodeId) : 0
        if (revision > 0 && edgeRevisionByNode[nodeId] === revision) return
        edgeRevisionByNode[nodeId] = revision
        notePlacement(nodeId, position, rotation)
        if (lazyMaterialization && !edgeGeometryByNode[nodeId] && !nodeVisibleInPreview(nodeId)) {
            pendingEdgesByNode[nodeId] = { sourceKey: sourceKey }
            return
        }
        buildEdgeInstance(nodeId, sourceKey)
    }

    function buildEdgeInstance(nodeId, sourceKey) {
        const shared = sharedEdgeGeometry(sourceKey)
        if (!shared) return
        if (edgeGeometryByNode[nodeId]) {
            edgeGeometryByNode[nodeId].copyEdgesFrom(shared)
            scheduleSectionForNewGeometry()
            return
        }
        const geometry = edgeGeometryComponent.createObject(sceneRoot)
        if (!geometry.copyEdgesFrom(shared)) {
            geometry.destroy()
            return
        }
        const model = edgeModelComponent.createObject(partNode(nodeId), { "geometry": geometry, "nodeId": nodeId })
        edgeGeometryByNode[nodeId] = geometry
        edgeModelByNode[nodeId] = model
        const section = !presentationOnly && controller ? controller.section : null
        applyEdgeState(model, section, sliceOnlyFinalState(section))
        scheduleSectionForNewGeometry()
    }

    // Builds the parked parts the display filter now shows. Called before the filter's
    // fit, so the fit finds the bounds it needs.
    function materializePending() {
        for (let nodeId in pendingMeshesByNode) {
            if (lazyMaterialization && !nodeVisibleInPreview(nodeId)) continue
            const pending = pendingMeshesByNode[nodeId]
            delete pendingMeshesByNode[nodeId]
            for (let segmentKey in pending) {
                const entry = pending[segmentKey]
                if (entry.sourceKey !== undefined) buildMeshInstance(nodeId, segmentKey, entry.sourceKey, entry.sourceColor)
                else buildMesh(nodeId, segmentKey, entry.sourceColor, entry.meshJson)
            }
        }
        for (let nodeId in pendingEdgesByNode) {
            if (lazyMaterialization && !nodeVisibleInPreview(nodeId)) continue
            const pending = pendingEdgesByNode[nodeId]
            delete pendingEdgesByNode[nodeId]
            if (pending.sourceKey !== undefined) buildEdgeInstance(nodeId, pending.sourceKey)
            else buildEdges(nodeId, pending)
        }
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

    // What a click at this hit would pick in the current mode, as the CAD tools snap: in
    // point to point a vertex, else a circle's centre, else a point on an edge, else the point
    // on the face; for the other modes the whole edge or face the mode is about. Null when
    // the mode measures whole parts (volume, bounds) or nothing is there.
    //   { nodeId, entity (a MeasureEntity), info (its description), entityKind ("face" or
    //     "edge", what to highlight), topologyId, sourceGeometry, placementNode }
    // `withShape` asks for a face's triangles too: only an accepted surface-to-surface pick
    // needs them, and copying them on every hover would cost for nothing.
    function measureCandidateAt(hit, withShape) {
        if (!root.controller || !hit.objectHit || !hit.objectHit.nodeId || !hit.scenePosition) return null
        const measurement = root.controller.measurement
        const mode = measurement.mode
        if (mode === 6 || mode === 7) return null
        const model = hit.objectHit
        const nodeId = model.nodeId
        // The part's geometry (faces and edges alike) is in its own frame -- a copy of a shared
        // mesh, an exploded part -- so the hit is taken into it, and the entity back out.
        const local = model.mapPositionFromScene(hit.scenePosition)
        const toScene = model.sceneTransform
        const pixel = root.worldPerPixelAt(hit.scenePosition)
        const edgeGeometry = edgeGeometryByNode[nodeId]
        const edgeModel = edgeModelByNode[nodeId]
        const edgeToScene = edgeModel ? edgeModel.sceneTransform : toScene
        let entity = undefined
        let entityKind = ""
        let sourceGeometry = null
        let topologyId = 0
        let placementNode = model.parent

        const takeFace = function() {
            const geometry = model.geometry
            const info = geometry ? geometry.topologyAtPoint(local.x, local.y, local.z) : null
            if (!info || !info.topologyId) return false
            entity = geometry.faceEntity(info.topologyId, toScene, hit.scenePosition, hit.sceneNormal, withShape === true)
            if (!entity) return false
            entityKind = "face"
            sourceGeometry = geometry
            topologyId = info.topologyId
            return true
        }
        const takeEdge = function(round) {
            if (!edgeGeometry) return false
            // Within a few pixels of the pointer, whatever the zoom.
            const info = edgeGeometry.topologyAtPoint(local.x, local.y, local.z, pixel * root.measureEdgePixels)
            if (!info || !info.topologyId || (round && info.radiusMm <= 0)) return false
            entity = edgeGeometry.edgeEntity(info.topologyId, edgeToScene, hit.scenePosition)
            if (!entity) return false
            entityKind = "edge"
            sourceGeometry = edgeGeometry
            topologyId = info.topologyId
            placementNode = edgeModel ? edgeModel.parent : model.parent
            return true
        }

        if (mode === 0) {
            const snap = edgeGeometry ? edgeGeometry.snapPointAt(local.x, local.y, local.z, pixel * root.measureEdgePixels,
                                                                 pixel * root.measureVertexPixels, edgeToScene) : undefined
            if (snap) {
                entity = snap
                entityKind = "edge"
                sourceGeometry = edgeGeometry
                topologyId = measurement.describe(snap).topologyId
                placementNode = edgeModel ? edgeModel.parent : model.parent
            } else {
                // A point on the face, with the face lit so it is clear which one.
                takeFace()
                entity = measurement.surfacePointEntity(hit.scenePosition, hit.sceneNormal)
            }
        } else if (mode === 1) {
            takeEdge(false)
        } else if (mode === 2 || mode === 5) {
            takeFace()
        } else if (mode === 3) {
            // Round first; failing that, whatever is there, so the hover can say it is not round.
            if (!takeEdge(true) && !takeFace()) takeEdge(false)
        } else if (mode === 4) {
            if (!takeEdge(false)) takeFace()
        }
        if (!entity) return null
        const info = measurement.describe(entity)
        return {
            nodeId: nodeId,
            entity: entity,
            info: info,
            entityKind: entityKind,
            topologyId: topologyId,
            sourceGeometry: sourceGeometry,
            placementNode: placementNode,
            point: info.point
        }
    }

    // A highlight copies the picked face or edge out of its part's geometry, in that geometry's
    // frame, so it is drawn where the part is: placed as the part's node is.
    function placementPosition(pick) {
        return pick && pick.placementNode ? pick.placementNode.position : Qt.vector3d(0, 0, 0)
    }
    function placementRotation(pick) {
        return pick && pick.placementNode ? pick.placementNode.rotation : Qt.quaternion(1, 0, 0, 0)
    }

    function showHoverTopology(pick) {
        const same = pick && root.hoverTopology && pick.sourceGeometry === root.hoverTopology.sourceGeometry
                && pick.topologyId === root.hoverTopology.topologyId && pick.entityKind === root.hoverTopology.entityKind
        root.hoverTopology = pick
        // The pointer moved along the same face or edge: its highlight stands, only the snap
        // point and the chip follow.
        if (same) return
        hoverFaceHighlight.clearTopology()
        hoverEdgeGeometry.clearEdges()
        if (!pick || !pick.sourceGeometry || !pick.topologyId) return
        if (pick.entityKind === "face") {
            hoverFaceHighlight.position = placementPosition(pick)
            hoverFaceHighlight.rotation = placementRotation(pick)
            hoverFaceHighlight.setTopology(pick.sourceGeometry, pick.topologyId)
        } else if (pick.entityKind === "edge") {
            hoverEdgeModel.position = placementPosition(pick)
            hoverEdgeModel.rotation = placementRotation(pick)
            hoverEdgeGeometry.copyTopologyFrom(pick.sourceGeometry, pick.topologyId)
        }
    }

    function clearMeasurementHighlights() {
        root.hoverTopology = null
        root.acceptedTopologies = []
        hoverFaceHighlight.clearTopology()
        hoverEdgeGeometry.clearEdges()
    }

    function acceptMeasurementHit(hit) {
        const mode = root.controller.measurement.mode
        if (mode === 6 || mode === 7) {
            root.controller.acceptViewSelection(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                                hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z)
            return
        }
        // Surface to surface measures between whole faces, so it takes the face's triangles.
        const pick = measureCandidateAt(hit, mode === 2)
        if (!pick || !root.controller.acceptMeasureEntity(pick.nodeId, pick.entity)) return
        // The measurement was started over when this pick began a new one.
        let next = root.controller.measurement.pickedEntities.length === 1 ? [] : root.acceptedTopologies.slice()
        // A whole edge or face stays lit in the measurement colour; a point has its numbered marker.
        if (pick.sourceGeometry && pick.topologyId && (pick.info.kind === "edge" || pick.info.kind === "face")) next.push(pick)
        root.acceptedTopologies = next
        showHoverTopology(null)
        // The hover is the next candidate, at once: no move needed to see it.
        updateHover(input.mouseX, input.mouseY)
    }

    // A face pick independent of the measurement mode, so the drawing workspace can pick and
    // highlight faces without pretending to be a measurement tool.
    function facePickFromHit(hit) {
        if (!hit.objectHit || !hit.scenePosition) return null
        const geometry = hit.objectHit.geometry
        if (!geometry) return null
        // The geometry is in the part's assembled coordinates; an exploded part is drawn
        // elsewhere, so the hit is taken back into the part's own frame to find its face.
        const local = hit.objectHit.mapPositionFromScene(hit.scenePosition)
        const info = geometry.topologyAtPoint(local.x, local.y, local.z)
        if (!info || !info.topologyId || info.entityKind !== "face") return null
        return {
            nodeId: hit.objectHit.nodeId,
            entityKind: info.entityKind,
            topologyId: info.topologyId,
            measureMm: info.measureMm,
            radiusMm: info.radiusMm,
            sourceGeometry: geometry,
            sourceModel: hit.objectHit,
            placementNode: hit.objectHit.parent,
            point: hit.scenePosition,
            normal: hit.sceneNormal
        }
    }

    // The face a right-click landed on, for the menu's "View normal to face", and the last
    // face clicked at all, for the shortcut when the pointer is not over a face.
    property var contextFacePick: null
    property var lastFacePick: null
    property var lastNormalView: null

    // Right-click: a part that is already selected keeps the selection, so the menu acts on
    // every selected part, as the tree's right-click does. Only an unselected part replaces
    // the selection, and empty space leaves it alone.
    function contextPickAt(x, y) {
        const hit = view.pick(x, y)
        root.contextFacePick = null
        if (!hit.objectHit || !hit.objectHit.nodeId) return false
        root.contextFacePick = root.facePickFromHit(hit)
        if (root.contextFacePick) root.lastFacePick = root.contextFacePick
        if (root.controller.isNodeSelected(hit.objectHit.nodeId)) return true
        return root.pickAt(x, y, false)
    }

    function rememberFaceAt(x, y) {
        const pick = root.facePickFromHit(view.pick(x, y))
        if (pick) root.lastFacePick = pick
    }

    // Menu text for an action on the selection: the plain wording for one part, and the
    // count for several, so it is clear the action is not just for the part clicked.
    function selectionActionText(single, several) {
        const count = root.controller ? root.controller.selectedNodeIds.length : 0
        return count > 1 ? several.arg(count) : single
    }

    // Turn the view to look straight at a picked face, keeping the zoom. A flat face is seen
    // along its exact normal, a cylinder along its axis, anything else along the normal at
    // the picked point. Asked again for the same face while looking at it, it shows the
    // face's other side.
    function viewNormalToFacePick(pick) {
        if (!pick || !pick.sourceGeometry) return false
        const face = pick.sourceGeometry.faceViewFor(pick.topologyId)
        const resolved = face && face.direction !== undefined
        // The face is described in its part's own frame; a moved or turned part is seen where
        // it is drawn, so its direction and centre go back into the scene.
        const model = pick.sourceModel
        const toScene = function(vector, isDirection) {
            if (!model || !vector) return vector
            return isDirection ? model.mapDirectionToScene(vector) : model.mapPositionToScene(vector)
        }
        let direction = resolved ? toScene(face.direction, true) : pick.normal
        const target = face && face.centroid !== undefined ? toScene(face.centroid, false) : pick.point
        const back = navigation.orientation.times(Qt.vector3d(0, 0, 1))
        // An axis has no outside; take the end that turns the view least.
        if (resolved && face.kind === "axis" && direction.dotProduct(back) < 0) direction = direction.times(-1)
        if (root.lastNormalView && root.lastNormalView.nodeId === pick.nodeId
                && root.lastNormalView.topologyId === pick.topologyId && direction.dotProduct(back) > 0.9999)
            direction = direction.times(-1)
        navigation.viewAlong(direction, target, face && face.edgeDirection !== undefined ? toScene(face.edgeDirection, true) : null)
        root.lastNormalView = { nodeId: pick.nodeId, topologyId: pick.topologyId }
        return true
    }

    function viewNormalToFaceAt(x, y) {
        return root.viewNormalToFacePick(root.facePickFromHit(view.pick(x, y)))
    }

    // The shortcut and menu command: the face under the pointer, else the last one clicked.
    function viewNormalToFace() {
        if (input.containsMouse && root.viewNormalToFaceAt(input.mouseX, input.mouseY)) return true
        return root.viewNormalToFacePick(root.lastFacePick)
    }

    function reportFaceFrameAt(x, y) {
        const pick = root.facePickFromHit(view.pick(x, y))
        if (!pick) return
        const frame = pick.sourceGeometry.faceFrameFor(pick.topologyId)
        // An empty map means the face could not be resolved; emitting it would hand the
        // listener a default frame that reads as a valid view.
        if (!frame || frame.topologyId === undefined) return
        // A copy of a shared mesh describes its face in the part's own frame; the listener
        // works in assembly coordinates, so the frame is placed (and not exploded).
        const placement = placementByNode[pick.nodeId]
        if (placement) {
            frame.normal = placement.rotation.times(frame.normal)
            frame.centroid = placement.rotation.times(frame.centroid).plus(placement.position)
        }
        root.faceFrameSelected(frame)
    }

    function prepareOrbitPivotAt(x, y) {
        const hit = view.pick(x, y)
        if (hit.objectHit && hit.scenePosition)
            navigation.setPendingOrbitPivot(hit.scenePosition)
        else
            navigation.clearPendingOrbitPivot()
    }

    function updateHover(x, y) {
        root.hoverPointer = Qt.point(x, y)
        const hit = view.pick(x, y)
        const nodeId = hit.objectHit && hit.objectHit.nodeId ? hit.objectHit.nodeId : ""
        if (hoveredNodeId !== nodeId) hoveredNodeId = nodeId
        if (root.faceFrameSelectionEnabled)
            showHoverTopology(root.facePickFromHit(hit))
        else if (root.measurementActive && hit.objectHit && hit.objectHit.nodeId)
            showHoverTopology(measureCandidateAt(hit, false))
        else if (root.hoverTopology)
            showHoverTopology(null)
    }

    function captureToFile(fileUrl) {
        if (!fileUrl) return
        performCapture(qsTr("Encoding PNG…"), function(capture, image) { capture.saveImage(image, fileUrl) })
    }

    function captureToClipboard() {
        performCapture(qsTr("Copying to clipboard…"), function(capture, image) { capture.copyImageToClipboard(image) })
    }

    // Quick-access path for the viewport's right-click menu and Ctrl/Cmd+C:
    // always transparent, regardless of whatever the Capture panel's
    // Transparent/Viewport solid toggle is currently set to, since that is
    // exactly what this specific action promises by name.
    function copyAsTransparentPicture() {
        if (!root.controller) return
        root.controller.capture.transparentBackground = true
        captureToClipboard()
    }

    // Ctrl/Cmd+C is a window-wide shortcut, so without this guard it would
    // also fire (and steal the key) while a text field elsewhere (the hex
    // color field, a search box, ...) has focus and the user just wants to
    // copy selected text out of it.
    function isTextEditFocused() {
        const item = root.Window.window ? root.Window.window.activeFocusItem : null
        return !!(item && typeof item.selectAll === "function" && "text" in item)
    }

    // Shared render pipeline behind captureToFile/captureToClipboard: both grab
    // the viewport at the requested scale identically and differ only in what
    // happens to the resulting image, via onResult(capture, image).
    function performCapture(encodingMessage, onResult) {
        if (!root.controller) return
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
                    capture.setCaptureProgress(0.82, encodingMessage)
                    onResult(capture, result.image)
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
    Component { id: partNodeComponent; Node {} }
    Component { id: edgeGeometryComponent; CadEdgeGeometry {} }
    // Coalesces the burst of arriving meshes into one fit, and -- because
    // fitCamera() only clears pendingFit once it actually fits -- is also what
    // retries a fit that was requested before its geometry existed.
    Timer { id: fitTimer; interval: 120; onTriggered: root.fitCamera() }
    Timer { id: sectionApplyTimer; interval: 0; repeat: false; onTriggered: root.applyNextSectionItem() }
    Component {
        id: modelComponent
        Model {
            id: modelItem
            property string nodeId: ""
            // The file's own colour, or empty when the file gives none.
            property string importColor: ""
            property color sourceColor: root.theme.neutralBody
            property bool selected: root.showComponentSelection && root.controller
                                    && ((root.selectionEnabled && root.controller.selectedNodeCount >= 0
                                         && root.controller.isNodeSelected(nodeId))
                                        || root.nodeMatchesSelection(nodeId, root.externalHighlightNodeId))
            property bool hovered: !selected && root.hoveredNodeId === nodeId
            // Volume and bounds measure the whole part, so the part is what the hover shows.
            property bool inspectionHovered: hovered && root.componentHoverEnabled
                                             && (!root.measurementActive || root.showComponentSelection)
            property bool transparent: false
            // The part's own opacity while transparent (Properties > Opacity).
            property real transparentOpacity: 0.45
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
                roughnessValue: 0.6
                clipEnabled: !root.presentationOnly && root.controller && root.controller.section.enabled
                sectionNormal: root.effectiveSectionNormal
                sectionOffset: root.controller ? root.controller.section.effectiveOffset : 0
                retainedSign: root.controller && root.controller.section.flipped ? -1 : 1
                ghostOpacity: modelItem.transparent ? modelItem.transparentOpacity : 1.0
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
            materials: [standardEdgeMaterial]
            CadEdgeMaterial {
                id: standardEdgeMaterial
                depthBiasAmount: root.edgeDepthBias
                edgeColor: edgeModelItem.selected ? viewportVisualTheme.selectionEdgeColor
                                                   : viewportVisualTheme.edgeColor
                lineWidth: edgeModelItem.selected ? 2.5 : 1.5
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
        objectName: "viewportNavigation"
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
                clipNear: root.perspectiveClipNear
                clipFar: root.perspectiveClipFar
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: orthographicCamera
                z: root.orthographicBackoff
                clipNear: root.orthographicClipNear
                clipFar: root.orthographicClipFar
                horizontalMagnification: navigation.orthographicMagnification
                verticalMagnification: navigation.orthographicMagnification
            }
            DirectionalLight {
                eulerRotation: Qt.vector3d(0, 0, 0)
                brightness: viewportVisualTheme.keyLightBrightness
                ambientColor: viewportVisualTheme.ambientLight
            }
        }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -35, 0); brightness: viewportVisualTheme.fillLightBrightness }
        Node {
            id: sceneRoot

            Model {
                id: hoverEdgeModel
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
                    position: root.placementPosition(modelData)
                    rotation: root.placementRotation(modelData)
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
                clipNear: root.perspectiveClipNear
                clipFar: root.perspectiveClipFar
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: componentXrayOrthographicCamera
                z: root.orthographicBackoff
                clipNear: root.orthographicClipNear
                clipFar: root.orthographicClipFar
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
        // presentationOnly suppresses measurement chrome in preview viewports, but picking a
        // face to project normal to is a legitimate reason to show the face highlight in one:
        // without it there is no way to tell which surface a click is about to take.
        visible: (!root.presentationOnly || root.faceFrameSelectionEnabled)
                 && ((root.hoverTopology && root.hoverTopology.entityKind === "face")
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
                clipNear: root.perspectiveClipNear
                clipFar: root.perspectiveClipFar
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: xrayOrthographicCamera
                z: root.orthographicBackoff
                clipNear: root.orthographicClipNear
                clipFar: root.orthographicClipFar
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
                position: root.placementPosition(modelData)
                rotation: root.placementRotation(modelData)
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

    View3D {
        id: handleView
        objectName: "explodeHandleView"
        anchors.fill: parent
        renderMode: View3D.Offscreen
        visible: root.handlesShown
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Transparent
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }
        camera: navigation.projectionMode === "orthographic" ? handleOrthographicCamera : handlePerspectiveCamera
        Node {
            position: navigation.cameraPosition
            rotation: navigation.orientation
            PerspectiveCamera {
                id: handlePerspectiveCamera
                clipNear: root.perspectiveClipNear
                clipFar: root.perspectiveClipFar
                fieldOfView: navigation.perspectiveFieldOfView
            }
            OrthographicCamera {
                id: handleOrthographicCamera
                z: root.orthographicBackoff
                clipNear: root.orthographicClipNear
                clipFar: root.orthographicClipFar
                horizontalMagnification: navigation.orthographicMagnification
                verticalMagnification: navigation.orthographicMagnification
            }
        }
        ExplodeHandle {
            id: explodeHandle
            objectName: "explodeHandle"
            position: root.handleCentre
            rotation: root.handleFrame
            length: root.handleLength
            xColor: root.theme && root.theme.axisX !== undefined ? root.theme.axisX : "transparent"
            yColor: root.theme && root.theme.axisY !== undefined ? root.theme.axisY : "transparent"
            zColor: root.theme && root.theme.axisZ !== undefined ? root.theme.axisZ : "transparent"
            activeColor: root.theme && root.theme.handleActive !== undefined ? root.theme.handleActive : "transparent"
            centreColor: root.theme && root.theme.foreground !== undefined ? root.theme.foreground : "transparent"
            activeKind: root.handleDrag ? root.handleDrag.kind : root.handleHover ? root.handleHover.kind : ""
            activeAxis: root.handleDrag ? root.handleDrag.axis : root.handleHover ? root.handleHover.axis : -1
        }
    }

    Rectangle {
        id: pivotPickHint
        objectName: "pivotPickHint"
        visible: root.pickingPivot && !root.captureUiHidden
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 16
        z: 6
        width: pivotPickLabel.implicitWidth + 24
        height: pivotPickLabel.implicitHeight + 14
        radius: root.theme && root.theme.radius2 !== undefined ? root.theme.radius2 : 6
        color: root.theme ? root.theme.surfaceRaised : "transparent"
        border.color: root.theme ? root.theme.accent : "transparent"
        Label {
            id: pivotPickLabel
            anchors.centerIn: parent
            text: qsTr("Click a point on a part to move the handle there  ·  Esc to cancel")
            color: root.theme ? root.theme.foreground : "transparent"
        }
    }

    Rectangle {
        id: handleReadoutTag
        visible: root.handleDrag !== null && root.handleReadout.length > 0
        x: root.handleReadoutPosition.x + 16
        y: root.handleReadoutPosition.y + 12
        z: 6
        width: handleReadoutLabel.implicitWidth + 12
        height: handleReadoutLabel.implicitHeight + 6
        radius: root.theme && root.theme.radius1 !== undefined ? root.theme.radius1 : 4
        color: root.theme ? root.theme.surfaceRaised : "transparent"
        border.color: root.theme ? root.theme.border : "transparent"
        Label {
            id: handleReadoutLabel
            anchors.centerIn: parent
            text: root.handleReadout
            color: root.theme ? root.theme.foreground : "transparent"
            font.pixelSize: 12
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
        property bool rolling: false
        property real lastRollAngle: 0
        // Continuous roll the gesture has accumulated, and how much of it has been
        // applied. Snapping needs both: the pointer moves continuously while the model
        // only moves in steps.
        property real rollAccumulator: 0
        property real rollApplied: 0
        hoverEnabled: true

        cursorShape: root.pickingPivot ? Qt.CrossCursor
                   : root.handleDrag || root.handleHover ? Qt.PointingHandCursor : Qt.ArrowCursor

        onPressed: function(mouse) {
            root.forceActiveFocus()
            pressX = mouse.x
            pressY = mouse.y
            pressButton = mouse.button
            dragging = false
            // A press on a handle drags the selection instead of the view.
            if (mouse.button === Qt.LeftButton && root.handlesShown) {
                const handle = root.handleAt(mouse.x, mouse.y)
                if (handle && root.beginHandleDrag(handle, mouse.x, mouse.y)) {
                    navigation.clearPendingOrbitPivot()
                    return
                }
            }
            if (mouse.button === Qt.LeftButton && !(mouse.modifiers & Qt.ShiftModifier))
                root.prepareOrbitPivotAt(mouse.x, mouse.y)
            else
                navigation.clearPendingOrbitPivot()
        }
        onPositionChanged: function(mouse) {
            if (root.handleDrag) {
                root.updateHandleDrag(mouse.x, mouse.y, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                return
            }
            if (pressButton === Qt.NoButton) {
                if (root.handlesShown && root.updateHandleHover(mouse.x, mouse.y)) {
                    if (root.hoveredNodeId !== "") root.hoveredNodeId = ""
                    return
                }
                root.updateHover(mouse.x, mouse.y)
                return
            }
            const deltaX = mouse.x - pressX
            const deltaY = mouse.y - pressY
            if (Math.abs(deltaX) > 3 || Math.abs(deltaY) > 3) dragging = true
            if (!dragging) return
            // Alt + left drag spins the view in its own plane. Measured as the
            // cursor's angle about the viewport centre rather than a pixels-per-degree
            // factor, so the model tracks the pointer directly.
            if (pressButton === Qt.LeftButton && (mouse.modifiers & Qt.AltModifier)) {
                const angle = Math.atan2(mouse.y - root.height / 2, mouse.x - root.width / 2)
                        * 180 / Math.PI
                if (!rolling) {
                    // Latch the starting angle so engaging the gesture cannot jump.
                    rolling = true
                    rollAccumulator = 0
                    rollApplied = 0
                } else {
                    let step = angle - lastRollAngle
                    // Crossing the +/-180 seam would otherwise read as a full turn.
                    while (step > 180) step -= 360
                    while (step < -180) step += 360
                    if (mouse.modifiers & Qt.ShiftModifier) {
                        // Alt+Shift steps the roll, matching the 2D canvas so the gesture means
                        // the same thing in both. The accumulator stays continuous and only the
                        // applied value is quantised -- rounding the accumulator makes small
                        // drags round to zero, which reads as a stuck gesture.
                        rollAccumulator += step
                        const snapped = Math.round(rollAccumulator / 15) * 15
                        if (snapped !== rollApplied) {
                            navigation.roll(snapped - rollApplied)
                            rollApplied = snapped
                        }
                    } else {
                        navigation.roll(step)
                        rollAccumulator += step
                        rollApplied = rollAccumulator
                    }
                }
                lastRollAngle = angle
                pressX = mouse.x
                pressY = mouse.y
                return
            }
            rolling = false
            if (pressButton === Qt.MiddleButton || (pressButton === Qt.LeftButton && (mouse.modifiers & Qt.ShiftModifier)))
                navigation.pan(deltaX, deltaY)
            else if (pressButton === Qt.LeftButton)
                navigation.orbit(deltaX, deltaY)
            pressX = mouse.x
            pressY = mouse.y
        }
        onReleased: function(mouse) {
            if (root.handleDrag) {
                root.endHandleDrag(true)
                pressButton = Qt.NoButton
                return
            }
            // Picking the pivot: a click puts the handle on the part there, selecting nothing.
            if (root.pickingPivot && !dragging && pressButton === Qt.LeftButton) {
                root.pickPivotAt(mouse.x, mouse.y)
                pressButton = Qt.NoButton
                navigation.clearPendingOrbitPivot()
                return
            }
            if (!dragging && pressButton === Qt.LeftButton && root.faceFrameSelectionEnabled) {
                root.reportFaceFrameAt(mouse.x, mouse.y)
            }
            if (!dragging && pressButton === Qt.LeftButton && root.selectionEnabled) {
                const additive = (mouse.modifiers & Qt.ShiftModifier)
                        || (mouse.modifiers & Qt.ControlModifier)
                        || (mouse.modifiers & Qt.MetaModifier)
                root.pickAt(mouse.x, mouse.y, additive)
                root.rememberFaceAt(mouse.x, mouse.y)
            }
            if (!dragging && pressButton === Qt.RightButton && root.contextActionsEnabled) {
                if (root.componentContextActionsEnabled && root.contextPickAt(mouse.x, mouse.y)) {
                    if (root.presentationOnly) exportComponentContextMenu.popup(mouse.x, mouse.y)
                    else componentContextMenu.popup(mouse.x, mouse.y)
                }
                else viewportContextMenu.popup(mouse.x, mouse.y)
            }
            pressButton = Qt.NoButton
            rolling = false
            navigation.clearPendingOrbitPivot()
        }
        onCanceled: {
            root.endHandleDrag(false)
            pressButton = Qt.NoButton
        }
        onExited: {
            root.hoveredNodeId = ""
            root.handleHover = null
            root.showHoverTopology(null)
        }
        onWheel: function(wheel) {
            const deviceType = wheel.device ? wheel.device.type : PointerDevice.Unknown
            // Ctrl/Command forces zoom. Windows delivers a trackpad pinch as
            // Ctrl+wheel, so this is what keeps pinch-to-zoom working there now
            // that a plain two-finger drag pans instead.
            const zoomModifier = (wheel.modifiers & Qt.ControlModifier)
                    || (wheel.modifiers & Qt.MetaModifier)
            const mode = pointerInputRouter.wheelMode(deviceType,
                                                       wheel.pixelDelta.x,
                                                       wheel.pixelDelta.y,
                                                       wheel.angleDelta.x,
                                                       wheel.angleDelta.y,
                                                       zoomModifier)
            if (mode === "pan") {
                // macOS can report the fingers' drift as scrolling while it also reports
                // them pinching; panning from that during a pinch is a view shift the user
                // never asked for.
                if (touchGestures.active) {
                    wheel.accepted = true
                    return
                }
                // Never pixelDelta directly: it is always (0,0) on Windows, so
                // panning with it would move nothing at all there.
                const delta = pointerInputRouter.wheelPanDelta(wheel.pixelDelta.x,
                                                               wheel.pixelDelta.y,
                                                               wheel.angleDelta.x,
                                                               wheel.angleDelta.y)
                navigation.pan(delta.x, delta.y)
            } else {
                navigation.zoom(pointerInputRouter.wheelZoomDelta(wheel.pixelDelta.y,
                                                                  wheel.angleDelta.y),
                                wheel.x, wheel.y)
            }
            wheel.accepted = true
        }
    }

    MeasurementOverlay {
        id: measurementOverlay
        objectName: "measurementOverlay"
        anchors.fill: parent
        z: 4
        enabled: false
        theme: root.theme
        visualTheme: viewportVisualTheme
        view: view
        revision: navigation.revision
        measurement: root.controller ? root.controller.measurement : null
        visible: !root.presentationOnly && root.controller !== null
                 && (root.measurementActive || root.controller.measurement.pickedPoints.length > 0)
        // The hover is for aiming; a capture shows the result only, and only if asked to.
        hover: root.measurementActive ? root.hoverTopology : null
        pointer: root.hoverPointer
        showHover: root.measurementActive && !root.captureUiHidden && input.containsMouse && input.pressedButtons === Qt.NoButton
        showResult: !root.captureUiHidden || (root.controller !== null && root.controller.capture.includeMeasurements)
    }

    // Two-finger touch gestures: pinch to zoom, drag to pan, twist to roll.
    //
    // Deliberately two-point only, which leaves a one-finger drag to fall
    // through to the MouseArea above (Qt synthesizes mouse events from an
    // ungrabbed touch point) so a single finger still orbits and a tap still
    // selects. Zoom and pan run together because that is how direct
    // manipulation is expected to feel; roll is gated behind a threshold
    // because a little incidental rotation is unavoidable whenever two fingers
    // pinch or drag, and an unasked-for roll is disorienting.
    PinchHandler {
        id: touchGestures
        target: null
        minimumPointCount: 2
        maximumPointCount: 2
        // Without this the handler never activates. The MouseArea above covers the
        // whole viewport and, via Qt's mouse-from-touch synthesis, grabs the first
        // touch point; a pointer handler is not allowed to take a grab away from an
        // item unless it is given permission, so the second finger arrived with the
        // gesture already owned elsewhere.
        grabPermissions: PointerHandler.CanTakeOverFromAnything
                         | PointerHandler.ApprovesTakeOverByAnything

        readonly property real twistThresholdDegrees: 8
        property real lastScale: 1
        property point lastTranslation: Qt.point(0, 0)
        property real lastRotation: 0
        property bool twisting: false

        onActiveChanged: {
            lastScale = 1
            lastTranslation = Qt.point(0, 0)
            lastRotation = 0
            twisting = false
            // Hand the gesture over from the single-finger orbit path.
            //
            // Qt synthesizes a mouse event from the first touch point, which latches
            // input.pressButton. With a second finger down no matching release ever
            // arrives, so the MouseArea kept treating every subsequent move as an
            // orbit drag and the view pivoted continuously. Clearing that state on
            // both transitions makes a two-finger gesture take over cleanly and stop
            // cleanly.
            input.pressButton = Qt.NoButton
            input.dragging = false
            if (navigation.clearPendingOrbitPivot) navigation.clearPendingOrbitPivot()
        }
        onActiveScaleChanged: {
            if (!active || lastScale <= 0) return
            // Anchored where the pinch is: on a trackpad that is the pointer.
            navigation.zoomByFactor(activeScale / lastScale, centroid.position.x, centroid.position.y)
            lastScale = activeScale
        }
        // A trackpad has no translation gesture of its own to report here. macOS
        // delivers a pinch as a native zoom gesture with a synthetic centroid, and
        // that centroid drifts as the fingers close, so panning from it moved the
        // view every time the user zoomed. Panning on a trackpad is the two-finger
        // drag handled in onWheel above ("pan" mode), which loses nothing by this.
        // A real touchscreen still pans from the gesture, which is its only way to.
        readonly property bool translationPans:
                !centroid.device || centroid.device.type !== PointerDevice.TouchPad

        onActiveTranslationChanged: {
            if (!active) return
            if (!translationPans) {
                lastTranslation = activeTranslation
                return
            }
            navigation.pan(activeTranslation.x - lastTranslation.x,
                           activeTranslation.y - lastTranslation.y)
            lastTranslation = activeTranslation
        }
        onActiveRotationChanged: {
            if (!active) return
            if (!twisting) {
                if (Math.abs(activeRotation) < twistThresholdDegrees) return
                // Absorb the rotation accumulated up to the threshold so the
                // view does not jump by the threshold amount on engagement.
                twisting = true
                lastRotation = activeRotation
                return
            }
            navigation.roll(activeRotation - lastRotation)
            lastRotation = activeRotation
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
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("View normal to face"); enabled: root.contextFacePick !== null; onTriggered: root.viewNormalToFacePick(root.contextFacePick) }
        ThemedMenuSeparator { theme: componentContextMenu.theme }
        ThemedMenuItem {
            theme: componentContextMenu.theme
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Exit isolate")
                  : root.selectionActionText(qsTr("Isolate"), qsTr("Isolate %1 selected"))
            enabled: root.controller && root.controller.activeNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: componentContextMenu.theme; text: root.selectionActionText(qsTr("Hide"), qsTr("Hide %1 selected")); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: root.transparencyActionText(); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.toggleTransparentSelection() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Make others transparent"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.makeOthersTransparent() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Undo visibility change"); enabled: root.controller && root.controller.canRestoreVisibility; onTriggered: root.controller.restoreVisibility() }
        ThemedMenuSeparator { theme: componentContextMenu.theme; visible: root.explodedLayout }
        ThemedMenuItem {
            theme: componentContextMenu.theme
            visible: root.explodedLayout
            text: root.selectionActionText(qsTr("Reset position"), qsTr("Reset %1 positions"))
            enabled: root.selectionMoved()
            onTriggered: root.resetSelectedPositions()
        }
        ThemedMenuItem {
            theme: componentContextMenu.theme
            visible: root.explodedLayout
            text: qsTr("Reset exploded view")
            onTriggered: root.animateExplodeReset()
        }
        ThemedMenuSeparator { theme: componentContextMenu.theme }
        ThemedMenuItem { theme: componentContextMenu.theme; text: qsTr("Properties"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.toolRequested("properties") }
    }

    ThemedMenu {
        id: exportComponentContextMenu
        theme: root.theme
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Fit selection"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.fitSelection() }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("View normal to face"); enabled: root.contextFacePick !== null; onTriggered: root.viewNormalToFacePick(root.contextFacePick) }
        ThemedMenuSeparator { theme: exportComponentContextMenu.theme }
        ThemedMenuItem {
            theme: exportComponentContextMenu.theme
            text: root.controller && root.controller.viewerPresentation === AppState.Isolate ? qsTr("Exit isolate")
                  : root.selectionActionText(qsTr("Isolate"), qsTr("Isolate %1 selected"))
            enabled: root.controller && root.controller.activeNodeId.length > 0
            onTriggered: root.controller.toggleIsolateActiveNode()
        }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: root.selectionActionText(qsTr("Hide"), qsTr("Hide %1 selected")); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.hideActiveNode() }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: root.transparencyActionText(); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.toggleTransparentSelection() }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Make others transparent"); enabled: root.controller && root.controller.activeNodeId.length > 0; onTriggered: root.controller.makeOthersTransparent() }
        ThemedMenuItem { theme: exportComponentContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
    }

    function selectionMoved() {
        if (!explodedLayout || !controller || explode.revision < 0) return false
        for (const nodeId of controller.selectedNodeIds) {
            if (explode.canReset(nodeId)) return true
        }
        return false
    }

    // Make transparent, or Make opaque once the whole selection is.
    function transparencyActionText() {
        const opaqueAgain = controller && controller.visibilityRevision >= 0 && controller.selectedNodeCount >= 0
                            && controller.isSelectionTransparent()
        return opaqueAgain ? selectionActionText(qsTr("Make opaque"), qsTr("Make %1 selected opaque"))
                           : selectionActionText(qsTr("Make transparent"), qsTr("Make %1 selected transparent"))
    }

    function resetSelectedPositions() {
        if (!explode || !controller) return
        for (const nodeId of controller.selectedNodeIds) explode.resetNode(nodeId)
    }

    ThemedMenu {
        id: viewportContextMenu
        objectName: "viewportContextMenu"
        theme: root.theme
        ThemedMenuItem { theme: viewportContextMenu.theme; text: root.requireDisplayFilter ? qsTr("Fit part") : qsTr("Fit assembly")
                                     onTriggered: root.fitCamera() }
        ThemedMenuItem { theme: viewportContextMenu.theme; text: qsTr("Show all"); enabled: root.controller; onTriggered: root.controller.showAllNodes() }
        ThemedMenuItem {
            theme: viewportContextMenu.theme
            visible: root.explodedLayout
            text: qsTr("Reset exploded view")
            onTriggered: root.animateExplodeReset()
        }
        ThemedMenuSeparator { theme: viewportContextMenu.theme }
        ThemedMenuItem {
            theme: viewportContextMenu.theme
            objectName: "viewportSavePictureItem"
            text: qsTr("Save picture…")
            enabled: root.controller && !root.controller.capture.inProgress
            onTriggered: root.savePictureRequested()
        }
        ThemedMenuItem {
            theme: viewportContextMenu.theme
            text: qsTr("Copy as transparent picture")
            enabled: root.controller && !root.controller.capture.inProgress
            onTriggered: root.copyAsTransparentPicture()
        }
    }

    ThemedComboBox {
        id: displayModeControl
        theme: root.theme
        visible: root.renderModeControlVisible && !root.captureUiHidden
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
        controller: root.controller
        // Tracks the live camera so the cube always shows the part's orientation.
        cameraOrientation: navigation.orientation
        // captureUiHidden stays in the condition so the cube is never baked into
        // an exported image.
        visible: root.viewCubeVisible && !root.captureUiHidden
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: displayModeControl.height + 22
        anchors.rightMargin: 14
        onViewRequested: function(normal) { root.setStandardView(normal) }
        // Dragging the cube orbits the part, matching CAD convention.
        onOrbitRequested: function(deltaX, deltaY) { navigation.orbit(deltaX, deltaY) }
        // Clicking the face already square-on spins the drawing in its own plane,
        // reusing the same roll the two-finger twist gesture uses.
        onPlanarRotateRequested: function(degrees) { navigation.roll(degrees) }
    }

    Shortcut { sequence: "F"; enabled: root.visible && (!root.presentationOnly); onActivated: root.fitCamera() }
    Shortcut { sequence: "Shift+F"; enabled: root.visible && (!root.presentationOnly); onActivated: root.fitSelection() }
    Shortcut { sequence: "I"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.toggleIsolateActiveNode() }
    Shortcut { sequence: "H"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.hideActiveNode() }
    Shortcut { sequence: "T"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.toggleTransparentSelection() }
    Shortcut { sequence: "Shift+T"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.makeOthersTransparent() }
    Shortcut { sequence: "Ctrl+Shift+H"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "Meta+Shift+H"; enabled: root.visible && (!root.presentationOnly); onActivated: if (root.controller) root.controller.showAllNodes() }
    Shortcut { sequence: "M"; enabled: root.visible && (!root.presentationOnly); onActivated: root.toolRequested("measure") }
    Shortcut { sequence: "S"; enabled: root.visible && (!root.presentationOnly); onActivated: root.toolRequested("section") }
    Shortcut { sequence: "E"; enabled: root.visible && (!root.presentationOnly) && root.explodedLayoutEnabled; onActivated: root.toolRequested("explode") }
    Shortcut { sequence: "V"; enabled: root.visible && (!root.presentationOnly) && root.explodedLayoutEnabled; onActivated: root.toolRequested("views") }
    // root.visible matters as much as presentationOnly: every workspace in the stack is
    // instantiated, so Inspect's viewport kept these enabled while Export was on screen. Two
    // enabled "1" shortcuts is an ambiguous sequence, and Qt fires neither -- which is why
    // the Export and Drawing shortcuts appeared to do nothing.
    Shortcut { sequences: ["1"]; enabled: !root.presentationOnly && root.visible
               onActivated: root.renderMode = 0 }
    Shortcut { sequences: ["2"]; enabled: !root.presentationOnly && root.visible
               onActivated: root.renderMode = 1 }
    Shortcut { sequences: ["3"]; enabled: !root.presentationOnly && root.visible
               onActivated: root.renderMode = 2 }
    Shortcut {
        sequences: [StandardKey.Copy]
        enabled: root.visible && (!root.presentationOnly && root.controller && !root.controller.capture.inProgress)
                 && !root.isTextEditFocused()
        onActivated: root.copyAsTransparentPicture()
    }
    // Ctrl/Cmd+O is owned by the File > Open STEP… menu item (Main.qml) so
    // there is exactly one shortcut registration for it at window scope.
    Shortcut {
        sequence: "Esc"
        enabled: root.visible && (!root.presentationOnly && sectionArrowInput.dragging)
        onActivated: sectionArrowInput.cancelDrag()
    }
    Shortcut {
        sequence: "Esc"
        enabled: root.visible && !root.presentationOnly && root.handleDrag !== null
        onActivated: root.endHandleDrag(false)
    }
    Shortcut {
        sequence: "Esc"
        enabled: root.visible && !root.presentationOnly && root.pickingPivot && root.handleDrag === null
        onActivated: root.cancelPivotPick()
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: root.visible && root.explodeToolActive && root.explode !== null && root.explode.canUndo
                 && !root.isTextEditFocused()
        onActivated: root.explode.undo()
    }
    Shortcut {
        sequences: [StandardKey.Redo]
        enabled: root.visible && root.explodeToolActive && root.explode !== null && root.explode.canRedo
                 && !root.isTextEditFocused()
        onActivated: root.explode.redo()
    }
    // Esc clears the measurement first; pressed again it does what it otherwise does.
    readonly property bool measurementHasPicks: root.measurementActive && root.controller !== null
                                               && root.controller.measurement.pickedEntities.length > 0
    Shortcut {
        sequence: "Esc"
        enabled: root.visible && !root.presentationOnly && root.measurementHasPicks && !sectionArrowInput.dragging
                 && root.handleDrag === null && !root.pickingPivot
        onActivated: root.controller.measurement.clearPicks()
    }
    Shortcut {
        sequence: "Esc"
        enabled: root.visible && (!root.presentationOnly && !sectionArrowInput.dragging && root.handleDrag === null
                                  && !root.pickingPivot && !root.measurementHasPicks)
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
        function onSharedMeshReady(sourceKey, sourceColor, meshJson) { root.appendSharedMesh(sourceKey, sourceColor, meshJson) }
        function onSharedEdgesReady(sourceKey, edgeJson) { root.appendSharedEdges(sourceKey, edgeJson) }
        function onMeshInstanceReady(nodeId, segmentKey, sourceKey, sourceColor, position, rotation) {
            root.appendMeshInstance(nodeId, segmentKey, sourceKey, sourceColor, position, rotation)
        }
        function onEdgeInstanceReady(nodeId, sourceKey, position, rotation) {
            root.appendEdgeInstance(nodeId, sourceKey, position, rotation)
        }
        function onFitRequested() { root.fitSelection() }
        function onViewNormalToFaceRequested() {
            if (root.visible && root.contextActionsEnabled && root.componentContextActionsEnabled) root.viewNormalToFace()
        }
        function onVisibilityChanged() { root.applyPresentation() }
        function onComponentPropertiesChanged() { root.applyAppearance() }
        function onActiveNodeIdChanged() { root.applyRenderMode() }
    }
    onRenderModeChanged: {
        applyRenderMode()
        if (controller && controller.renderMode !== renderMode) controller.renderMode = renderMode
    }
    Connections {
        target: root.controller
        function onRenderModeChanged() {
            if (root.renderMode !== root.controller.renderMode) root.renderMode = root.controller.renderMode
        }
    }
    onDisplayOnlyNodeIdChanged: {
        materializePending()
        applyRenderMode()
        // Owed until it succeeds: the filtered part's geometry may still be in
        // flight, in which case fitDisplayFilter() below is a no-op and the
        // retry happens when its bounds arrive.
        if (displayOnlyNodeId.length > 0) pendingFit = true
        Qt.callLater(fitDisplayFilter)
    }
    onExternalHighlightNodeIdChanged: applyPresentation()
    onLazyMaterializationChanged: if (!lazyMaterialization) materializePending()
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
