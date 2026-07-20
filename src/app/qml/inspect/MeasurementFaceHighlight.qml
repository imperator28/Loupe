import QtQuick
import QtQuick3D
import Loupe.App

Node {
    id: root

    property color fillColor: "#67d5c0"
    property color boundaryColor: "#dffbf4"
    property real fillAlpha: 0.42
    property real boundaryWidth: 3
    property bool active: false
    property bool faceReady: false
    property bool boundaryReady: false
    property var sourceGeometry: null
    property int topologyId: 0

    function setTopology(source, id) {
        if (sourceGeometry === source && topologyId === id && faceReady) return
        sourceGeometry = source
        topologyId = id
        faceGeometry.clearMesh()
        boundaryGeometry.clearEdges()
        faceReady = source && id > 0 ? faceGeometry.copyTopologyFrom(source, id) : false
        boundaryReady = faceReady ? boundaryGeometry.copyFaceBoundaryFrom(source, id) : false
    }

    function clearTopology() {
        sourceGeometry = null
        topologyId = 0
        faceReady = false
        boundaryReady = false
        faceGeometry.clearMesh()
        boundaryGeometry.clearEdges()
    }

    Model {
        objectName: "measurementFaceFill"
        visible: root.active && root.faceReady
        pickable: false
        castsShadows: false
        receivesShadows: false
        geometry: MeshGeometry { id: faceGeometry }
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: Qt.rgba(root.fillColor.r, root.fillColor.g, root.fillColor.b, root.fillAlpha)
            alphaMode: PrincipledMaterial.Blend
            depthDrawMode: Material.NeverDepthDraw
            cullMode: Material.NoCulling
        }
    }

    Model {
        objectName: "measurementFaceBoundary"
        visible: root.active && root.boundaryReady
        pickable: false
        castsShadows: false
        receivesShadows: false
        geometry: CadEdgeGeometry { id: boundaryGeometry }
        materials: PrincipledMaterial {
            lighting: PrincipledMaterial.NoLighting
            baseColor: root.boundaryColor
            emissiveFactor: root.boundaryColor
            lineWidth: root.boundaryWidth
            depthDrawMode: Material.NeverDepthDraw
        }
    }
}
