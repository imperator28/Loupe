import QtQuick

QtObject {
    function wheelMode(deviceType, pixelX, pixelY, angleX, angleY) {
        if (deviceType === PointerDevice.Mouse) return "zoom"
        if (deviceType === PointerDevice.TouchPad) return "pan"

        const hasPixelDelta = pixelX !== 0 || pixelY !== 0
        const isDiscreteWheel = angleX === 0 && angleY !== 0
                && Math.abs(angleY) % 120 === 0
        return !hasPixelDelta || isDiscreteWheel ? "zoom" : "pan"
    }
}
