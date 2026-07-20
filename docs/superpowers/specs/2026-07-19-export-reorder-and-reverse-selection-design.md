# Export Reorder and Reverse Selection Design

## Purpose

Make sequential export order predictable, add direct drag reordering, and let users identify an assembly component by clicking it in the master preview without changing export inclusion.

## Sequential Naming

Sequential filenames derive from the current export bucket position. Reordering a non-overridden component immediately regenerates its number, so the first row uses `-001`, the second uses `-002`, and so on.

Focusing or clicking an output filename does not create an override. A filename becomes a per-component override only after the user edits its text. Manual overrides remain attached to their component when the bucket is reordered; generated names continue to follow position.

The bucket model exposes whether each filename is generated or overridden. QML uses an explicit edit-dirty state so model refreshes and focus changes cannot accidentally freeze a generated filename.

## Reorder Interaction

Each bucket row provides:

- a grip handle as the primary drag target;
- a high-contrast insertion line showing the pending drop position;
- existing move-earlier and move-later actions for precise and keyboard-accessible changes.

Drag and arrow actions identify the moved row by node ID and provide a destination index. The controller resolves the row's current index at action time, avoiding stale indexes from reused QML delegates.

Dropping outside a valid insertion target leaves the bucket unchanged. Reorder actions do not alter the assembly, export inclusion, manual filename overrides, or the focused preview component.

## Reverse Selection

The master assembly preview accepts a normal left-click selection. The standalone preview remains inspection-only.

When the clicked rendered node maps directly to a visible exportable component, that component becomes focused. When the rendered node is an internal or suppressed body, the controller walks its assembly ancestors and focuses the nearest visible exportable component.

Focusing from the master preview:

- highlights the component in the master preview;
- updates the standalone preview;
- scrolls the component list to the focused row;
- leaves all export checkboxes and bucket contents unchanged.

Clicking empty master-preview background clears the focus highlight without changing export inclusion.

## Component Boundaries

- `ExportWorkspaceController` owns node-ID reorder, generated-versus-overridden filename state, and scene-node-to-picker-component resolution.
- `ExportBucket.qml` owns grip dragging, insertion feedback, edit-dirty tracking, and arrow actions.
- `ExportPreview.qml` exposes whether its viewport accepts component selection.
- `ExportWorkspace.qml` enables selection only for the master preview and synchronizes controller focus.
- `ExportComponentPicker.qml` owns list focus and scrolling to a selected node.

## Testing

Focused controller coverage verifies:

- generated sequential names follow row order;
- manual overrides remain attached to their component;
- node-ID reorder resolves the current source index;
- invalid drops leave order unchanged;
- internal scene nodes resolve to the nearest visible exportable ancestor;
- reverse selection never changes checked nodes.

QML coverage verifies that filename focus alone does not create an override and that drag/drop controls load with explicit delegate indexes. Manual review verifies insertion feedback, arrow fallback, master-preview picking, tree scrolling, and unchanged checkbox state.
