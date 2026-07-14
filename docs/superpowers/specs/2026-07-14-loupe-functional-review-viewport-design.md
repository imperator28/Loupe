# Loupe Functional Review Viewport Design

**Date:** 2026-07-14  
**Status:** Approved design, pending written-spec review  
**Scope:** Phase 1 functional UX revision

## Purpose

Make Loupe's Inspect workspace functionally credible for CAD review before investing in full visual redesign or broad validation. This pass fixes five review blockers: static viewport navigation, missing STEP colors, collapsed tools, absent import progress, and illegible dark-theme controls. It also adds persistent user-defined materials and per-body appearance overrides for mechanism captures.

## Goals

- Provide familiar mouse and keyboard navigation for a rotatable 3D viewport.
- Preserve STEP face, occurrence, and definition colors.
- Make every inspection tool individually identifiable and operable.
- Report honest staged import progress, elapsed time, and a qualified ETA.
- Make all property and task controls legible in the dark theme.
- Support built-in and persistent user-defined materials.
- Support occurrence- and definition-scoped material and color overrides.
- Keep the validation gate narrow until functional UX is approved.

## Non-Goals

- Full visual redesign, branding, final typography, final iconography, or responsive-layout polish.
- Geometry certification, performance tuning, Windows regression, release packaging, signing, or notarization.
- Writing material or review-color overrides back into exported STEP files.
- A generalized rendering engine or material graph.

## Architecture

### Viewport Navigation

A dedicated QML viewport-navigation component owns the orbit pivot, yaw, pitch, distance, and screen-plane pan offset. It updates the existing Qt Quick 3D camera rig but remains separate from picking and rendering.

Input handling distinguishes a click from a drag with the platform drag threshold. Mouse gestures are:

| Input | Action |
|---|---|
| Left click | Select component or face |
| Left drag | Orbit |
| Middle drag | Orbit |
| Shift + left drag | Pan |
| Wheel or trackpad pinch | Zoom |
| Right click on component | Open component context menu at cursor |
| Right click on empty viewport | Open viewport context menu at cursor |

Orbit uses the current fit or selected-component center as its pivot. Pitch is clamped to prevent camera inversion. Zoom is multiplicative and bounded by model extent. Pan moves in the camera's local right/up plane. Opening a document resets navigation to a fitted assembly view.

Keyboard shortcuts are inactive while an editable text control has focus:

| Shortcut | Action |
|---|---|
| `F` | Fit assembly |
| `Shift+F` | Fit selected component |
| `I` | Isolate selected or restore |
| `H` | Hide selected |
| `Shift+H` | Hide others |
| `Cmd/Ctrl+Shift+H` | Show all |
| `M` | Open measurement tool |
| `S` | Open section tool |
| `Esc` | Cancel active tool or restore the previous visibility state |
| `Cmd/Ctrl+O` | Open STEP file |

### Context Menus And Visibility

The component context menu contains **Fit selection**, **Isolate/Restore**, **Hide**, **Hide others**, **Show all**, **Restore**, and **Properties**. The empty-viewport menu contains **Fit assembly** and **Show all**.

A C++ visibility model replaces the current single presentation enum. It stores visible/hidden state per stable node ID and one previous snapshot for restore. All instances of a definition remain independently addressable. Tree actions, viewport actions, toolbar actions, context menus, and shortcuts use this same model.

The user-facing term **Ghost** is removed. Primary CAD visibility language is **Isolate**, **Hide**, **Hide others**, **Show all**, and **Restore**.

### STEP Colors And Mesh Segments

The importer uses the XCAF color tool and resolves source appearance with this precedence:

1. Face color
2. Occurrence color
3. Definition color
4. Loupe neutral fallback

Meshing groups triangles into color-homogeneous segments while preserving the owning occurrence ID for selection and visibility. A mesh event carries request ID, component ID, segment key, geometry, and RGBA color. The existing `MeshReady` protocol shape is extended and its currently unused segment key becomes meaningful. A protocol minor-version increment documents the compatible extension.

Malformed or unsupported color attributes affect only their segment. Geometry remains usable and the import records a non-blocking warning. Selection uses an outline or highlight overlay and does not replace the resolved body color.

### Appearance Resolution

Loupe preserves imported appearance and layers review metadata above it:

1. Manual body-color override
2. Assigned material color
3. Original STEP color
4. Loupe neutral fallback

Material assignment and manual color override both support **This occurrence** and **All instances** scopes. Occurrence scope is the default. Definition scope affects every occurrence that references that definition. **Reset appearance** removes the override at the chosen scope and reveals the next layer.

Appearance overrides are stored by source fingerprint, stable node or definition ID, and scope. They survive reopening the same unchanged source. They are local inspection metadata and never mutate the imported XCAF document. Captures use the resolved on-screen appearance.

### Material Library

Each material contains:

- Stable ID
- Unique display name
- Positive finite density in `g/cm3`
- Display color
- Built-in/custom marker

Built-in materials remain read-only. A SQLite-backed local material store provides custom-material create, update, delete, list, and lookup operations. Storage lives under Loupe's application-local data root and is independent of the STEP cache.

The property panel material selector includes the built-in and custom library plus **Manage materials...**. The management dialog supports creating, editing, and deleting custom materials. Saving validates name, density, and color. Deleting an assigned custom material requires confirmation and reverts affected assignments to the next appearance layer.

Assigning a material updates mass calculation and viewport color. A separate appearance control allows body recoloring without material assignment.

### Import Progress And Cancellation

The existing progress protocol becomes active end to end. Import work runs in a dedicated worker thread so the worker's local-socket event loop remains responsive to cancellation and replacement requests.

The worker reports monotonic weighted progress across these stages:

| Stage | Overall range |
|---|---:|
| Reading STEP | 0-10% |
| Translating XCAF | 10-55% |
| Building assembly tree and resolving colors | 55-65% |
| Meshing and streaming components | 65-98% |
| Finalizing viewport | 98-100% |

OCCT `Message_ProgressIndicator` data drives translation progress. Tree/color work reports completed labels over total labels. Meshing reports completed shapes over total shapes. Stage ranges prevent progress from moving backward when phases use different units.

The application controller exposes stage label, fraction, elapsed time, cancellability, and optional ETA. ETA appears only after enough timed measurable work exists; otherwise the UI says **Estimating**. A cache hit may reveal the tree immediately while progress continues as **Refining geometry**.

Cancel sets the job's interruption flag, stops additional mesh work, discards partial state, and returns Inspect to its empty state. Opening another file cancels and supersedes the active request. Events from superseded request IDs are ignored.

### Functional UI Corrections

The bottom inspection dock derives a stable implicit width from its content, preventing all tools from occupying the same zero-width area. It exposes individually recognizable controls for **Fit**, **Isolate**, **Hide**, **Section**, **Measure**, and **Capture**. Controls have explicit checked, disabled, keyboard-focus, tooltip, and accessible-name states. Visibility actions that are more contextual remain in the component menu.

Import progress appears as a non-blocking viewport status panel with stage, determinate progress bar, percentage, elapsed time, qualified ETA, and Cancel.

Loupe selects Qt Fusion controls before constructing the application and applies one application-level dark palette. This avoids native macOS controls that ignore local QML palette customization. Property and task panels use semantic palette roles instead of feature-local text colors. Component properties separate identity, appearance, geometry, and mass.

These corrections make the functional build testable but do not approve the current layout as the final Loupe design.

## State And Error Handling

- Opening a document resets camera, selection, visibility, active tool, progress, and partial geometry.
- A replacement request cancels the previous job and ignores its late events.
- Import failure preserves the last reached stage and percentage while presenting the worker's actionable error.
- Missing STEP color data falls back per segment and does not fail import.
- Material-store failure leaves built-in materials available and reports a non-blocking warning.
- Invalid custom materials cannot be saved.
- Appearance writes are transactional; failed persistence leaves the current viewport unchanged.
- Context-menu invocation never rotates or changes selection unless the user invokes an action.
- Click/drag disambiguation prevents orbit from producing accidental picks.

## Focused Validation

Automated tests cover:

- Orbit, pan, zoom, fit, and camera bounds math
- Click-versus-drag dispatch and keyboard focus suppression
- Approved shortcut map and context-menu dispatch
- Per-node visibility, hide others, show all, and restore
- XCAF face/occurrence/definition color precedence
- Colored mesh-segment protocol encoding and rendering
- Occurrence- and definition-scoped appearance resolution
- Material CRUD, validation, persistence, deletion, and mass calculation
- Appearance precedence and reset behavior
- Monotonic staged progress, elapsed time, ETA qualification, cancellation, and request replacement
- Tool-dock dimensions and distinct control hit targets
- Dark-palette text/control contrast

Computer Use review uses one small and one large private-corpus file and verifies:

1. Left and middle drag rotation, Shift-left pan, zoom, fit, and selection.
2. Original STEP colors and selection without source-color replacement.
3. Tool identification, checked state, context actions, and shortcuts.
4. Import stage, percentage, elapsed time, ETA behavior, and cancellation.
5. Property-panel legibility in dark mode.
6. Custom material creation and reuse.
7. Occurrence/all-instance material and body-color changes.
8. Capture output matching the viewport's resolved appearance.

The review stops after these workflows. Exhaustive geometry validation, performance profiling, full test matrices, Windows verification, and release packaging remain deferred until functional UX approval.

## Acceptance Criteria

- The viewport is no longer a static rendering and supports every approved gesture and shortcut.
- Source STEP colors render with documented precedence and survive selection.
- Inspection controls no longer overlap and each action is identifiable and operable.
- Import always exposes a current stage and monotonic percentage; elapsed time is always shown and ETA is never fabricated.
- Property and task controls remain legible in the dark theme on macOS.
- Users can persist custom materials and assign material/color appearance at occurrence or definition scope.
- Captures reflect the current visibility and resolved appearance.
- The focused automated and Computer Use review gates pass without expanding into deferred validation.

## Follow-Up Gate

After this functional UX is approved, Loupe receives a separate visual-design pass covering information architecture, workspace proportions, typography, iconography, spacing, visual hierarchy, responsive behavior, motion, and final cross-platform appearance. The present shell is not treated as the ideal or final design.
