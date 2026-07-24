import QtQuick
import QtQuick3D

CustomMaterial {
    property color edgeColor: "transparent"
    // Clip-space depth bias (fraction of the view-space W) pulling the edge
    // toward the camera relative to its coincident face. Unlike a world-space
    // position offset, this yields the same depth-buffer separation at every
    // view angle and distance, so edges cannot drop out on rotation.
    //
    // The camera's clip planes (StepViewport.qml) span clipNear=0.01 to
    // clipFar=1000000 — a 1e8 ratio. With a standard (non-reversed) depth
    // buffer that makes the ndc-to-world mapping extremely non-linear: a
    // fixed clip-space bias corresponds to a real-world thickness that grows
    // with the square of the viewing distance. 1e-4 (the first value tried
    // here) worked out to on the order of half a meter of real-world depth at
    // a typical fitted-part viewing distance — enough to punch through an
    // enclosure wall and expose the internal assembly's edges, which is worse
    // than the coplanar z-fighting this property exists to fix. 2e-7 is
    // roughly 500x smaller, keeping the real-world thickness sub-millimeter
    // at that same distance.
    property real depthBiasAmount: 0.0000002

    shadingMode: CustomMaterial.Unshaded
    cullMode: Material.NoCulling
    vertexShader: "shaders/cad-edge.vert"
    fragmentShader: "shaders/cad-edge.frag"
}
