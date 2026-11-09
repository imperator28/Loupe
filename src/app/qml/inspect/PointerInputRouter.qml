import QtQuick

// Decides whether a wheel event means "zoom" or "pan", and how far to pan.
//
// This is harder than it looks because the two platforms report a trackpad
// completely differently, verified against Qt's own Windows backend:
//
//   * macOS supplies a real device type (TouchPad) and a pixelDelta, so a
//     two-finger drag is directly identifiable.
//   * Windows routes trackpads through legacy mouse messages -- Qt's
//     qwindowspointerhandler.cpp comments "Let Mouse/TouchPad be handled using
//     legacy messages" -- so device.type reports Mouse. Worse, that same
//     function calls handleWheelEvent with an unconditional QPoint() for
//     pixelDelta, so pixelDelta is ALWAYS (0,0) on Windows.
//
// So on Windows the only usable signal is the shape of angleDelta itself: a real
// wheel notch is a whole multiple of 120 eighth-degrees, while a precision
// trackpad emits smaller arbitrary values and can also emit a horizontal
// component. That is what distinguishes the two here.
QtObject {
    id: router

    // One mouse-wheel notch, in the eighth-of-a-degree units Qt reports.
    readonly property int wheelNotch: 120
    // Windows maps one notch to three text lines. Treating that as ~60 px keeps
    // trackpad panning close to one-to-one with finger travel, and is only used
    // where no pixelDelta exists to be more precise with.
    readonly property real pixelsPerNotch: 60

    // `zoomModifier` must be true for Ctrl (or Command) held. This matters more
    // than it appears: Windows expresses a trackpad PINCH as Ctrl+wheel, so
    // without this branch, teaching the app that sub-notch deltas mean "pan"
    // would silently break pinch-to-zoom. It also gives every device a reliable
    // way to zoom regardless of how it is classified below.
    function wheelMode(deviceType, pixelX, pixelY, angleX, angleY, zoomModifier) {
        if (zoomModifier) return "zoom"

        // Trusted when the platform provides it, which is macOS.
        if (deviceType === PointerDevice.TouchPad) return "pan"

        // A clean vertical multiple of one notch is the strongest wheel signal
        // there is, and it outranks everything below: a wheel spun quickly
        // reports several whole notches at once, and some mice report a pixel
        // delta alongside. Checked first so those still zoom.
        if (angleX === 0 && angleY !== 0 && Math.abs(angleY) % router.wheelNotch === 0) return "zoom";

        // A pixel delta only ever comes from a high-resolution device.
        if (pixelX !== 0 || pixelY !== 0) return "pan"

        // Horizontal travel means two fingers moving sideways; a plain wheel has
        // no horizontal axis.
        if (angleX !== 0) return "pan"

        // A partial notch with no pixel delta. On Windows this is the precision
        // trackpad signature, and treating it as a wheel is what used to make a
        // two-finger drag zoom instead of pan.
        //
        // Caveat worth knowing: a high-resolution mouse wheel produces the same
        // event shape, and the two are genuinely indistinguishable here -- Qt
        // gives no device information and no pixel delta on Windows for either.
        // Panning is the better default because trackpads are far more common on
        // the laptops this runs on, and Ctrl+wheel above still zooms for anyone
        // affected.
        if (angleY !== 0) return "pan"

        return "zoom"
    }

    // Pan distance in pixels. Prefers a real pixel delta and falls back to
    // converting the angle delta, which is the only option on Windows.
    function wheelPanDelta(pixelX, pixelY, angleX, angleY) {
        if (pixelX !== 0 || pixelY !== 0) return Qt.point(pixelX, pixelY)
        const scale = router.pixelsPerNotch / router.wheelNotch
        return Qt.point(angleX * scale, angleY * scale)
    }

    // Zoom magnitude. angleDelta is the reliable channel for a wheel; pixelDelta
    // is the fallback for a device that only reports pixels.
    function wheelZoomDelta(pixelY, angleY) {
        return angleY !== 0 ? angleY : pixelY
    }

    // ---- Touch (#42) ----

    // Whether a two-finger gesture's translation pans the view. A touchscreen has no other way
    // to pan; a trackpad pans by scrolling (wheelMode above), and its pinch's synthetic centroid
    // drifts as the fingers close, so panning from it moved the view on every zoom. Decided by
    // the device the gesture comes from, not the platform: a touch Mac's screen is a
    // touchscreen, its trackpad a trackpad.
    function pinchTranslationPans(deviceType) {
        return deviceType !== PointerDevice.TouchPad
    }

    // How far a press may wander and still be a tap (a click), in pixels: a fingertip rolls and
    // jitters far more than a mouse.
    function tapSlop(touch) {
        return touch ? 10 : 3
    }

    // How long a finger is held still before it aims (in a pick mode) or opens the menu.
    readonly property int pressAndHoldMs: 450

    // What a pointer can grab, in pixels: a fingertip needs about 9 mm (44 pt), a mouse a few
    // pixels. `mouse` is the size for a mouse; on touch it grows to at least `touchMinimum`.
    function targetSize(mouse, touch, touchMinimum) {
        return touch ? Math.max(mouse, touchMinimum) : mouse
    }
}
