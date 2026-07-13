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
    function appendMesh(nodeId, meshJson) {
        if (geometryByNode[nodeId]) return
        const geometry = geometryComponent.createObject(sceneRoot)
        if (!geometry.appendWorkerMesh(meshJson)) { geometry.destroy(); return }
        const model = modelComponent.createObject(sceneRoot, { "geometry": geometry })
        geometryByNode[nodeId] = geometry
        modelByNode[nodeId] = model
        applyPresentation()
    }

    Component { id: geometryComponent; MeshGeometry {} }
    Component {
        id: modelComponent
        Model {
            materials: DefaultMaterial { diffuseColor: "#67d5c0"; roughness: 0.34 }
        }
    }

    View3D {
        anchors.fill: parent
        environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "#101418" }
        camera: camera
        PerspectiveCamera { id: camera; position: Qt.vector3d(root.fitDistance * 0.7, root.fitDistance * 0.55, root.fitDistance); eulerRotation: Qt.vector3d(-22, 32, 0) }
        DirectionalLight { eulerRotation: Qt.vector3d(-35, -35, 0); brightness: 1.2 }
        Node { id: sceneRoot }
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() { root.clearMeshes() }
        function onMeshReady(nodeId, meshJson) { root.appendMesh(nodeId, meshJson) }
        function onFitRequested() { root.fitCamera() }
        function onActiveNodeIdChanged() { root.applyPresentation() }
        function onViewerPresentationChanged() { root.applyPresentation() }
    }
}
