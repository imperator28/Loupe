import QtQuick
import QtQuick3D
import Loupe.App

Item {
    id: root
    property QtObject controller
    property var modelByNode: ({})
    property var geometryByNode: ({})
    readonly property real fitDistance: Math.max(120, (root.controller ? root.controller.modelExtentMm : 100) * 3.2)

    function fitCamera() {
        camera.position = Qt.vector3d(fitDistance * 0.7, fitDistance * 0.55, fitDistance)
    }
    function clearMeshes() {
        for (let id in modelByNode) modelByNode[id].destroy()
        for (let id in geometryByNode) geometryByNode[id].destroy()
        modelByNode = ({})
        geometryByNode = ({})
    }
    function applyPresentation() {
        const active = root.controller ? root.controller.activeNodeId : ""
        const mode = root.controller ? root.controller.viewerPresentation : AppState.Full
        for (let id in modelByNode) {
            const model = modelByNode[id]
            model.visible = mode !== AppState.Isolate || id === active
            model.opacity = mode === AppState.Ghost && id !== active ? 0.16 : 1.0
        }
    }
    function applySection() {
        if (!root.controller) return
        const section = root.controller.section
        const axis = section.axisName === "X" ? 0 : section.axisName === "Y" ? 1 : 2
        for (let id in geometryByNode)
            geometryByNode[id].setSection(section.enabled, axis, section.position, section.flipped)
    }
    function appendMesh(nodeId, meshJson) {
        if (geometryByNode[nodeId]) return
        const geometry = geometryComponent.createObject(sceneRoot)
        if (!geometry.appendWorkerMesh(meshJson)) { geometry.destroy(); return }
        const model = modelComponent.createObject(sceneRoot, { "geometry": geometry, "nodeId": nodeId })
        geometryByNode[nodeId] = geometry
        modelByNode[nodeId] = model
        applyPresentation()
        applySection()
    }
    function captureToFile(fileUrl) {
        if (!root.controller || !fileUrl) return
        const capture = root.controller.capture
        const originalBackgroundMode = view.environment.backgroundMode
        const originalColor = view.environment.clearColor
        if (capture.transparentBackground) {
            view.environment.backgroundMode = SceneEnvironment.Transparent
            view.environment.clearColor = "transparent"
        }
        view.grabToImage(function(result) {
            result.saveToFile(fileUrl.toLocalFile())
            view.environment.backgroundMode = originalBackgroundMode
            view.environment.clearColor = originalColor
        }, Qt.size(capture.resolvedWidth, capture.resolvedHeight))
    }

    Component { id: geometryComponent; MeshGeometry {} }
    Component {
        id: modelComponent
        Model {
            property string nodeId: ""
            pickable: true
            materials: DefaultMaterial { diffuseColor: "#67d5c0"; roughness: 0.34 }
        }
    }

    View3D {
        id: view
        anchors.fill: parent
        environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "#101418" }
        camera: camera
        PerspectiveCamera { id: camera; position: Qt.vector3d(root.fitDistance * 0.7, root.fitDistance * 0.55, root.fitDistance); eulerRotation: Qt.vector3d(-22, 32, 0) }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -35, 0); brightness: 1.2 }
        Node { id: sceneRoot }
        onWidthChanged: if (root.controller) root.controller.capture.setViewportSize(width, height)
        onHeightChanged: if (root.controller) root.controller.capture.setViewportSize(width, height)
        MouseArea {
            anchors.fill: parent
            onClicked: function(mouse) {
                const hit = view.pick(mouse.x, mouse.y)
                if (hit.objectHit && hit.objectHit.nodeId)
                    root.controller.acceptViewPick(hit.objectHit.nodeId, hit.scenePosition.x, hit.scenePosition.y, hit.scenePosition.z,
                                                    hit.sceneNormal.x, hit.sceneNormal.y, hit.sceneNormal.z)
            }
        }
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.clearMeshes() }
        function onMeshReady(nodeId, meshJson) { root.appendMesh(nodeId, meshJson) }
        function onFitRequested() { root.fitCamera() }
        function onActiveNodeIdChanged() { root.applyPresentation() }
        function onViewerPresentationChanged() { root.applyPresentation() }
    }
    Connections {
        target: root.controller ? root.controller.section : null
        function onChanged() { root.applySection() }
    }
}
