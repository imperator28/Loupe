import QtQuick
import QtQuick3D
import Loupe.App

Item {
    id: root
    property QtObject controller
    readonly property real fitDistance: Math.max(120, (root.controller ? root.controller.modelExtentMm : 100) * 3.2)

    function fitCamera() {
        camera.position = Qt.vector3d(fitDistance * 0.7, fitDistance * 0.55, fitDistance)
    }

    MeshGeometry {
        id: importedGeometry
    }

    View3D {
        anchors.fill: parent
        environment: SceneEnvironment {
            backgroundMode: SceneEnvironment.Color
            clearColor: "#101418"
        }
        camera: camera

        PerspectiveCamera {
            id: camera
            position: Qt.vector3d(root.fitDistance * 0.7, root.fitDistance * 0.55, root.fitDistance)
            eulerRotation: Qt.vector3d(-22, 32, 0)
        }
        DirectionalLight {
            eulerRotation: Qt.vector3d(-35, -35, 0)
            brightness: 1.2
        }
        Model {
            geometry: importedGeometry
            materials: DefaultMaterial {
                diffuseColor: "#67d5c0"
                roughness: 0.34
            }
        }
    }

    Connections {
        target: root.controller
        function onSnapshotChanged() {
            importedGeometry.clearMesh()
        }
        function onMeshReady(meshJson) {
            importedGeometry.appendWorkerMesh(meshJson)
        }
        function onFitRequested() {
            root.fitCamera()
        }
    }
}
