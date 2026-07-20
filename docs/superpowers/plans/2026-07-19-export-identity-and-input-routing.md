# Export Identity and Pointer Input Corrections Plan

## Goal

Show each bucket row's original component identity alongside its editable output filename, and restore physical mouse-wheel zoom without removing two-finger trackpad pan.

## Tasks

1. Update `ExportBucket.qml` so each row includes a read-only, elided source component label with a full-name tooltip above the generated filename.
2. Add a small, deterministic wheel-event classifier to `StepViewport.qml` and route discrete wheel input to zoom and high-resolution pixel input to pan.
3. Extend focused QML coverage for wheel classification and verify the export bucket loads with the additional identity label.
4. Build the app and focused tests, run export and QML smoke tests, copy the executable into the macOS review bundle, sign it, and relaunch it.

## Acceptance Checks

- Sequential rows visibly map an original component name to each generated filename.
- Reordering changes sequential filenames but not original component labels.
- A physical mouse wheel zooms.
- Two-finger trackpad drag pans.
- Pinch zoom, middle-button drag pan, and Shift plus left-button drag pan remain unchanged.
- The app loads in light and dark themes without QML errors.
