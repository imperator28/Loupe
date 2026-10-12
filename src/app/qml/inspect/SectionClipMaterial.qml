import QtQuick
import QtQuick3D

CustomMaterial {
    // Overridden per-model by the caller; inert default for standalone use.
    property color baseColor: "transparent"
    property vector3d emissiveFactor: Qt.vector3d(0, 0, 0)
    property real roughnessValue: 0.6
    property bool clipEnabled: false
    property vector3d sectionNormal: Qt.vector3d(0, 0, 1)
    property real sectionOffset: 0
    property real retainedSign: 1
    // Below 1 the part is drawn see-through, for context. Blended only then, so an opaque
    // part keeps the opaque pass and its depth write.
    property real ghostOpacity: 1.0
    sourceBlend: ghostOpacity < 1.0 ? CustomMaterial.SrcAlpha : CustomMaterial.NoBlend
    destinationBlend: ghostOpacity < 1.0 ? CustomMaterial.OneMinusSrcAlpha : CustomMaterial.NoBlend

    shadingMode: CustomMaterial.Shaded
    fragmentShader: "shaders/section-clip.frag"
}
