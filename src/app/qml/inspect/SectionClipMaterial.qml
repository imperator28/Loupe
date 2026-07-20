import QtQuick
import QtQuick3D

CustomMaterial {
    property color baseColor: "#67d5c0"
    property vector3d emissiveFactor: Qt.vector3d(0, 0, 0)
    property real roughnessValue: 0.34
    property bool clipEnabled: false
    property vector3d sectionNormal: Qt.vector3d(0, 0, 1)
    property real sectionOffset: 0
    property real retainedSign: 1

    shadingMode: CustomMaterial.Shaded
    fragmentShader: "shaders/section-clip.frag"
}
