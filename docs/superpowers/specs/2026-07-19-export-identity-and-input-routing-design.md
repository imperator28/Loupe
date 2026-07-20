# Export Identity and Pointer Input Corrections

## Purpose

Correct two regressions in the current review build without changing the approved Export workspace structure:

1. Keep the source component identity visible when an export naming strategy generates a different filename.
2. Restore physical mouse-wheel zoom while retaining two-finger trackpad pan.

## Export Bucket Identity

Each export bucket row shows the original component name as a read-only label above the editable output filename. The original name comes from the imported assembly and remains unchanged when the naming strategy, bucket order, filename override, or output format changes.

The generated filename remains editable. Sequential naming continues to update filenames from bucket order, while the source label makes mappings such as `INNER-1L86-550` to `Bracket-001.step` explicit. Existing row status, reorder, and remove controls remain available.

The row height increases only enough to fit the source label without truncating the filename field or status. Long component names elide with a tooltip containing the complete name.

## Pointer Input Routing

The viewport supports both devices automatically, without a preference or mode switch:

- Discrete vertical mouse-wheel movement zooms the camera.
- High-resolution two-finger trackpad scrolling pans the camera in both axes.
- Angle-only wheel events fall back to zoom.
- Pinch gestures continue to zoom.
- Middle-button drag and Shift plus left-button drag continue to pan.

QML wheel handling classifies a discrete wheel from its angle delta and routes it to the existing zoom function. Pixel-based, high-resolution movement routes to the existing pan function. This keeps camera behavior centralized in `ViewportNavigation` and limits the correction to event routing.

## Ambiguous Input

Some macOS pointing devices report both angle and pixel deltas. Exact wheel-notch angle increments take priority and zoom. Non-discrete high-resolution deltas pan. This prioritizes restoring conventional physical mouse-wheel behavior while preserving trackpad navigation.

## Testing

Focused verification covers:

- source and generated names appearing together in keep, prefix, and sequential naming modes;
- source labels remaining stable when bucket rows are reordered;
- discrete wheel events routing to zoom;
- pixel-based high-resolution events routing to pan;
- angle-only wheel events routing to zoom;
- existing pinch zoom and drag pan behavior remaining intact;
- QML smoke loading and a locally relaunched review build.

No export execution behavior, file contents, or CAD geometry pipeline changes are included in this correction.
