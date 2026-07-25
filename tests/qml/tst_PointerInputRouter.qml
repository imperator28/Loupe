import QtQuick
import QtTest
import "../../src/app/qml/inspect"

TestCase {
    name: "PointerInputRouterTests"

    PointerInputRouter {
        id: router
    }

    // A whole notch is the strongest wheel signal and outranks a stray pixel
    // delta, so a mouse that reports both still zooms.
    function test_mouseWheelZoomsEvenWithPixelDelta() {
        compare(router.wheelMode(PointerDevice.Mouse, 0, 8, 0, 120, false), "zoom")
    }

    function test_touchpadScrollPans() {
        compare(router.wheelMode(PointerDevice.TouchPad, 12, -9, 14, -11, false), "pan")
    }

    function test_unknownDiscreteWheelZooms() {
        compare(router.wheelMode(PointerDevice.Unknown, 0, 8, 0, -120, false), "zoom")
    }

    // A wheel spun quickly reports several notches in one event.
    function test_multipleNotchesStillZoom() {
        compare(router.wheelMode(PointerDevice.Unknown, 0, 0, 0, 360, false), "zoom")
    }

    function test_unknownHighResolutionScrollPans() {
        compare(router.wheelMode(PointerDevice.Unknown, 3, -7, 4, -9, false), "pan")
    }

    // The Windows precision-trackpad signature: a partial notch, no pixel delta,
    // and a device that claims to be a mouse because Windows routes trackpads
    // through legacy mouse messages. This must pan -- it is a two-finger drag.
    // Previously asserted as "zoom", which was the reason two-finger drag zoomed
    // on Windows while working correctly on macOS.
    function test_windowsTrackpadPartialNotchPans() {
        compare(router.wheelMode(PointerDevice.Mouse, 0, 0, 0, 15, false), "pan")
        compare(router.wheelMode(PointerDevice.Unknown, 0, 0, 0, 15, false), "pan")
    }

    // Horizontal travel cannot come from a plain wheel.
    function test_horizontalOnlyScrollPans() {
        compare(router.wheelMode(PointerDevice.Mouse, 0, 0, 40, 0, false), "pan")
    }

    // Windows delivers a trackpad PINCH as Ctrl+wheel, so the modifier has to
    // force zoom regardless of how the event would otherwise be classified --
    // otherwise pinch-to-zoom would start panning.
    function test_zoomModifierAlwaysZooms() {
        compare(router.wheelMode(PointerDevice.Mouse, 0, 0, 0, 15, true), "zoom")
        compare(router.wheelMode(PointerDevice.TouchPad, 12, -9, 14, -11, true), "zoom")
    }

    function test_noDeltaFallsBackToZoom() {
        compare(router.wheelMode(PointerDevice.Unknown, 0, 0, 0, 0, false), "zoom")
    }

    // A real pixel delta is used directly.
    function test_panDeltaPrefersPixelDelta() {
        const delta = router.wheelPanDelta(12, -9, 14, -11)
        compare(delta.x, 12)
        compare(delta.y, -9)
    }

    // Windows never supplies a pixel delta, so the pan distance has to come from
    // the angle delta or panning would move nothing at all there.
    function test_panDeltaDerivedFromAngleWhenNoPixelDelta() {
        const delta = router.wheelPanDelta(0, 0, 0, router.wheelNotch)
        compare(delta.x, 0)
        compare(delta.y, router.pixelsPerNotch)
        verify(delta.y > 0)
    }

    function test_panDeltaDerivedFromHorizontalAngle() {
        const delta = router.wheelPanDelta(0, 0, -router.wheelNotch, 0)
        compare(delta.x, -router.pixelsPerNotch)
        compare(delta.y, 0)
    }

    function test_zoomDeltaPrefersAngleDelta() {
        compare(router.wheelZoomDelta(8, 120), 120)
        compare(router.wheelZoomDelta(8, 0), 8)
    }
}
