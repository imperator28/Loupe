import QtQuick
import QtTest
import "../../src/app/qml/inspect"

TestCase {
    name: "PointerInputRouterTests"

    PointerInputRouter {
        id: router
    }

    function test_mouseWheelZoomsEvenWithPixelDelta() {
        compare(router.wheelMode(PointerDevice.Mouse, 0, 8, 0, 120), "zoom")
    }

    function test_touchpadScrollPans() {
        compare(router.wheelMode(PointerDevice.TouchPad, 12, -9, 14, -11), "pan")
    }

    function test_unknownDiscreteWheelZooms() {
        compare(router.wheelMode(PointerDevice.Unknown, 0, 8, 0, -120), "zoom")
    }

    function test_unknownHighResolutionScrollPans() {
        compare(router.wheelMode(PointerDevice.Unknown, 3, -7, 4, -9), "pan")
    }

    function test_unknownAngleOnlyInputZooms() {
        compare(router.wheelMode(PointerDevice.Unknown, 0, 0, 0, 15), "zoom")
    }
}
