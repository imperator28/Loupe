# Export Reorder and Reverse Selection Plan

## Goal

Keep generated sequence names positional, provide grip-based drag reordering with arrow fallback, and synchronize master-preview picks to the component tree without changing export inclusion.

## Tasks

1. Extend `ExportWorkspaceController` with filename-override state, node-ID reorder, and scene-node-to-visible-component focus resolution.
2. Add focused controller tests for generated and overridden names, current-index resolution, invalid moves, ancestor resolution, and unchanged checked state.
3. Update `ExportBucket.qml` with explicit delegate indexes, edit-dirty tracking, grip dragging, insertion feedback, and node-ID reorder calls.
4. Make only the master `ExportPreview` selectable and synchronize viewport picks to export focus.
5. Add component-picker focus and scroll-to-node support for reverse selection.
6. Build and run export, QML interaction, and smoke tests; refresh, sign, relaunch, and manually check the review bundle.

## Acceptance Checks

- Untouched sequential names regenerate after arrow or drag reorder.
- A manually edited filename stays with its component.
- Dragging shows a clear insertion target and moves the row on drop.
- Clicking master geometry focuses and reveals the corresponding component row.
- Preview picking never checks or unchecks an export item.
