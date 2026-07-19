# Export Bucket and Dual Preview Design

## Purpose

Replace the current plan-oriented Export workspace with a direct pick-and-export workflow. Users inspect components visually, add the desired standalone files to an export bucket, choose naming and output settings, and export the bucket to a folder.

The workspace must not expose internal concepts such as plan construction or fingerprints. Internal output planning remains an implementation detail used for validation and execution.

## Workspace Layout

Use the approved master-first review desk:

1. A component picker occupies the left rail.
2. A large master preview occupies the center.
3. A smaller standalone preview occupies the upper-right rail.
4. The export bucket and settings occupy the lower-right rail.

The master preview retains most of the available canvas because its purpose is assembly orientation. The standalone preview remains large enough to rotate and inspect the exact geometry that will become one output file.

Both previews use the active light or dark viewport theme and CAD edge settings. Neither preview includes Inspect-only tools such as section, measurement, capture, or material assignment.

## Component Picker

The picker shows named assembly occurrences and named bodies that can be exported as standalone files. Raw topology labels such as `SOLID` and `COMPOUND` are hidden when a meaningful named occurrence or body represents the same exportable geometry. A raw body label may appear only when no meaningful component name exists.

Each exportable row has a checkbox. Checkbox state is the only action that adds or removes an item from the export bucket.

Search filters the visible component rows without changing checkbox, hover, or pinned-preview state.

## Preview Interaction

The workspace maintains three independent states:

- `hoveredExportNodeId` is temporary and follows the component row under the pointer.
- `pinnedExportNodeId` is set by clicking a component row and survives pointer exit.
- `checkedNodeIds` contains the persistent export bucket selection.

Hovering any exportable row updates both previews, including for unchecked rows. This lets the user inspect a candidate before deciding whether to check it.

The master preview always shows the complete assembly. The hovered or pinned component is rendered as an X-ray highlight that remains visible through surrounding bodies. Other components retain their normal appearance.

The standalone preview shows only the hovered component. When no row is hovered, it shows the pinned component. The preview automatically fits the component when its identity changes and uses the selected assembly or local coordinate mode. Users may rotate, pan, and zoom it without affecting the master camera.

Clicking a row pins the candidate so the user can move the pointer into the standalone preview without losing it. Clicking does not modify the export bucket.

## Export Bucket

Checking a component immediately adds a row to the export bucket. Unchecking it or using the row's remove action removes it from both places.

Each bucket row shows:

- component name;
- resulting filename;
- format;
- validation or export status;
- remove action;
- drag handle when sequential naming is active.

The bucket is reorderable by drag and drop. Its order controls sequential numbering and execution order. Reordering does not alter the source assembly.

The primary action reads `Export N files`, where `N` is the total number of bucket rows. It is enabled when the bucket is non-empty, every row passes validation, and a destination folder is selected. There is no separate Build Plan action.

## Naming Strategies

The workspace provides three naming strategies:

### Keep Component Name

Use the component name with the selected format extension, for example `Cover.step`.

### Sequential Name

The user enters a base name. Bucket order produces zero-padded filenames such as `Bracket-001.step`, `Bracket-002.step`, and `Bracket-003.step`. Reordering the bucket updates filenames immediately.

### Prefix Component Name

The user enters a prefix that is prepended to the original component name, for example `REV-A_Cover.step`.

Every bucket filename remains individually editable for exceptional cases. An individual edit overrides the active strategy only for that row and does not rename the assembly component.

## Settings and Validation

The right rail contains destination, format, coordinates, naming strategy, and existing-file behavior. Grouping is fixed to separate files for this workflow and is not shown as a user setting.

Internal output plans are rebuilt automatically whenever bucket contents, order, filenames, destination, format, coordinates, or units change. Validation results are projected directly onto bucket rows.

Validation covers:

- empty or invalid filenames;
- duplicate output paths;
- unsupported component or coordinate combinations;
- unresolved unit decisions;
- missing or unwritable destination folders;
- existing destination files.

Existing-file behavior provides `Ask before replacing` as the default, plus `Skip existing` and `Replace existing`. The confirmation dialog lists only conflicting files and does not block non-conflicting rows.

## Export Execution

Pressing `Export N files` submits the validated outputs to the worker that owns the imported native CAD document. The worker uses the existing STEP and STL exporters and processes bucket rows in visible order.

The workspace shows overall progress and per-row state: queued, exporting, complete, skipped, cancelled, or failed. Users may cancel remaining work. Outputs already completed remain valid.

Each file is written through the existing atomic export-file mechanism. A failed or cancelled output must not leave a partial file at the final destination.

One failed row does not discard successful rows. The final summary reports completed, skipped, failed, and cancelled counts and keeps failed rows available for retry.

## Component Boundaries

- `ExportWorkspaceController` owns component projection, hover, pin, bucket order, settings, automatic validation, filenames, and execution status.
- A reusable CAD preview component owns scene presentation, camera interaction, filtering, fitting, and X-ray highlighting.
- `ExportWorkspace.qml` owns the approved master-first layout and user interactions.
- The worker protocol owns export submission, progress, cancellation, and result reporting.
- Existing core export planning and STEP/STL exporters remain the source of output validation and file creation.

The reusable preview component must not depend on Inspect tool state. Inspect and Export may configure it differently while sharing geometry presentation and navigation behavior.

## Error Handling

Errors appear at the narrowest useful scope:

- filename and collision errors appear on the affected bucket row;
- destination and global settings errors appear beside the relevant setting;
- worker or document errors appear in the progress area;
- per-file export failures remain attached to their rows with a retry action.

Hover or preview failures never change bucket contents. If standalone preview geometry is unavailable, the row remains selectable only when the exporter can resolve it; otherwise it is disabled with a reason.

## Testing and Review

Automated coverage includes:

- hover, pin, and checkbox state remain independent;
- hovering unchecked rows updates both preview targets;
- pointer exit falls back to the pinned component;
- component projection suppresses redundant `SOLID` and `COMPOUND` rows;
- checking, unchecking, removing, and reordering keep picker and bucket synchronized;
- all three naming strategies generate deterministic filenames;
- per-row overrides, invalid characters, duplicate names, and existing files validate correctly;
- standalone preview filtering and automatic fitting use the intended node and coordinate mode;
- worker execution reports progress, cancellation, partial success, and retry correctly;
- atomic file creation leaves no final partial output after failure or cancellation.

Manual UX review uses assemblies from `corpus/` and confirms:

- the master preview provides enough context for large assemblies;
- X-ray highlighting remains visible through occluding components in light and dark themes;
- standalone preview geometry matches the exported file;
- checkbox-to-bucket behavior is understandable without instructions;
- naming changes and bucket reordering provide immediate filename feedback;
- export progress is clear without exposing internal planning terminology.

## Delivery Sequence

1. Extract the reusable read-only CAD preview from the Inspect viewport.
2. Add hover, pin, and filtered-preview state to the Export controller.
3. Replace Output Plan with the master preview, standalone preview, and export bucket.
4. Add naming strategies, bucket ordering, and automatic validation.
5. Extend the worker protocol for execution, progress, cancellation, and result reporting.
6. Verify the complete workflow with focused automated tests and a locally launchable review build.
