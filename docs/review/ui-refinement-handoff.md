# Loupe UI Refinement Handoff

**Status:** Approved functional baseline; ready for UI/UX refinement
**Date:** 2026-07-19
**Audience:** Product design, UI engineering, CAD interaction engineering, and QA

## 1. Purpose and authority

Loupe has reached a functional review baseline. The next pass may substantially improve layout, hierarchy, styling, density, responsiveness, and discoverability, but it is not a workflow rewrite.

This document is the canonical UI handoff for that pass. Where an older prototype, screenshot, plan, or specification conflicts with this document, use this document and the current implementation. Later user decisions supersede earlier experiments.

The governing rule is:

> Presentation may change. CAD behavior, state ownership, document integrity, and export correctness may not change without a separate product and engineering review.

## 2. Product intent

Loupe is a quiet, work-focused desktop CAD review and selective export application. It is not a marketing surface and should open directly into the usable product.

The primary jobs are:

1. Open a STEP assembly and understand it quickly.
2. Navigate, select, isolate, inspect, section, measure, recolor, and capture it with familiar CAD interactions.
3. Pick components for standalone export while seeing exactly what each choice represents.
4. Export a validated bucket of files with predictable names, order, coordinates, and destination.

The design should favor scanability, direct manipulation, restrained visual styling, and conventional CAD language. It should feel dense but organized rather than decorative or card-heavy.

## 3. Change boundary

### 3.1 Green: freely refinable in the UI pass

- Overall layout, panel sizing, splitters, docking, responsive breakpoints, and information hierarchy.
- Typography, spacing, iconography, borders, elevation, and semantic color tokens.
- Toolbar composition and the visual form of controls, provided every required action remains accessible.
- Context-menu styling, provided menus remain cursor-positioned and content-sized.
- Panel collapse, pin, dock, and hide affordances, provided hiding a tool panel does not disable its active result.
- Empty, loading, progress, validation, error, and completion presentation.
- Replacing `T`, `F`, and `R` buttons with a standard view cube or axis triad, provided Top, Front, and Right remain one-click actions.
- Reorganizing the Export workspace responsively, provided both previews, the picker, bucket, settings, and export status remain available.
- Short functional transitions and micro-interactions that do not delay input or animate geometry/layout expensively.

### 3.2 Yellow: requires product and engineering review

- Changing any mouse, trackpad, keyboard, or right-click mapping.
- Combining hover, focus, selection, visibility, or export inclusion into fewer states.
- Changing what counts as an exportable component or how occurrences map to source geometry.
- Adding or removing measurement modes, projection modes, section modes, display modes, naming strategies, or coordinate policies.
- Changing when export becomes enabled or how partial success and cancellation behave.
- Changing the STEP import/refinement pipeline, tessellation policy, or section computation model.
- Replacing shared viewport/controller code with workspace-specific copies.

### 3.3 Red: cannot be changed in a UI-only upgrade

- Source STEP/BRep geometry, imported colors, or assembly placement may not be mutated by review actions.
- Visibility, sectioning, material assignment, appearance overrides, measurement, and capture may not alter exported geometry.
- UI list indexes may not replace stable node IDs as component identity.
- QML may not duplicate or become the source of truth for domain state owned by C++ controllers.
- Pointer movement may not trigger exact CAD recomputation or per-event mesh rebuilding.
- Workspace switching may not synchronously replay the complete geometry archive on the UI thread.
- Hover, focus, or preview actions may not add/remove items from the export bucket.
- Export may not write unvalidated or partial data to a final output filename.
- The internal immutable export plan and worker protocol may not be bypassed for UI convenience.
- Preview-ready geometry may not be presented as final-ready geometry for export.

## 4. Global application contracts

### 4.1 Workspaces

- The top-level product has two workspaces: **Inspect** and **Export**.
- Switching workspaces must feel immediate. Expensive geometry replay remains asynchronous and must not freeze navigation.
- Inspect and Export share the same viewport navigation and rendering behavior. Export configures that viewport for presentation-only use rather than forking its camera logic.

### 4.2 Theme

- The persisted theme choices are **System**, **Light**, and **Dark**.
- Theme applies to the complete application, including the 3D viewport.
- Light theme uses a white or near-white viewport and suitably dark defining edges.
- Dark theme uses a near-black viewport and suitably light defining edges.
- Menus, combo boxes, text fields, spin boxes, switches, dialogs, tooltips, and disabled states must remain readable in every theme.
- Theme must not change imported STEP colors, assigned material colors, body appearance overrides, or captured color meaning.
- Text contrast must meet WCAG expectations; state cannot be communicated by color alone.

### 4.3 Import and refinement feedback

- Import has staged, monotonic progress with percentage, current stage, elapsed time, and a qualified ETA when available.
- Users receive an interactive preview before final refinement completes.
- Refinement progress remains visible and may take time; it must never look like a frozen or completed operation.
- Refinement may be canceled while retaining the usable preview and bodies already refined.
- Opening a replacement file cancels the prior request, and late events from the old request are ignored.
- Export remains blocked until the document reaches final-ready state.

### 4.4 Shared component identity

- Stable node IDs are the identity contract across tree rows, viewports, selection, visibility, material assignment, and export.
- Human-readable assembly/component/body names are shown whenever available.
- Raw `SOLID` and `COMPOUND` implementation nodes are suppressed when a meaningful exportable ancestor exists.
- Repeated occurrences remain distinct even when they share a definition.

## 5. Inspect workspace decisions

### 5.1 Assembly tree

- Hovering a row pre-highlights that component in the viewport.
- Clicking a row selects the component and applies the persistent selection highlight.
- Selecting geometry in the viewport reveals and scrolls to the corresponding tree row.
- Right-clicking a tree row opens the same component actions available from the viewport.
- Clicking empty viewport background clears the active component selection.
- Tree hover is temporary and visually different from persistent selection.

Required component actions use conventional CAD wording:

- Fit selection
- Isolate / Restore
- Hide
- Hide others
- Show all
- Restore visibility

Do not use the term "ghost".

### 5.2 Viewport selection and context menus

- Hover provides a temporary pre-highlight.
- Click provides a persistent selected-component highlight.
- Right-click opens a context menu at the cursor.
- Right-clicking geometry selects that component before showing component actions.
- Right-clicking the background shows viewport-level actions such as Fit assembly and Show all.
- Context menus size to their actual actions. They must not stretch into tall, sparsely populated panels.

### 5.3 Navigation and input mapping

The following mappings are approved behavior and cannot be remapped during visual refinement:

| Input | Behavior |
| --- | --- |
| Left drag | Orbit around the active pivot |
| Middle-button drag | Pan |
| Shift + left drag | Pan |
| Right click | Context menu at cursor |
| Physical mouse wheel | Zoom |
| Trackpad two-finger drag | Pan |
| Trackpad pinch | Zoom |
| Background click | Clear component selection |

Additional camera rules:

- Orbit direction follows the approved Creo-like configuration.
- Orbit is unrestricted and can pass continuously through bottom views.
- Pressing over geometry establishes a pending cursor-based pivot. It becomes active only after the drag threshold, preventing the model from drifting before rotation begins.
- Pressing empty space retains the previous pivot.
- Pan translates camera and pivot together; zoom does not displace the pivot.
- Fit resets framing and pivot appropriately.
- A small drag threshold distinguishes click from drag.

### 5.4 Projection and standard views

- **Orthographic** projection with an isometric orientation is the default.
- **Perspective** is an explicit alternative.
- Switching projection preserves orientation, pivot, framing, and apparent scale as closely as possible.
- Top, Front, and Right standard views remain one-click operations.
- Capture uses the currently active projection.

### 5.5 Display modes and geometry presentation

The three display modes are:

1. Solid
2. Solid + Edges
3. Edges Only

Defining edges come from sampled BRep curves. Triangle-wireframe rendering is not an acceptable substitute. Qt Quick 3D surfaces remain tessellated display meshes, while source BRep geometry stays intact in the worker. The final renderer uses adaptive tessellation and must not claim to display analytic NURBS directly.

Appearance precedence is fixed:

1. Manual body appearance override
2. Assigned material color
3. Original STEP face, occurrence, or definition color
4. Neutral fallback

Selection and hover overlays may temporarily modify presentation but may not replace or lose source appearance.

### 5.6 Materials and appearance

- Built-in materials remain available.
- Users may create persistent custom materials with name, positive finite density in `g/cm3`, and color.
- Assignment scopes are **This occurrence** and **All instances**.
- Material assignment changes visible color and mass calculations.
- A separate body appearance override changes review/capture color without assigning engineering material.
- The appearance swatch always reflects the current effective color, not a placeholder.
- Clear restores the next color in the fixed precedence chain.

### 5.7 Tool presentation

- Section, Measure, and Capture are nonmodal tools.
- Tool panels may be moved, docked, collapsed, or hidden without blocking the viewport.
- Hiding a panel does not disable an active tool result.
- Disabling an active tool requires an explicit command such as **Turn off section** or **Clear**.
- Tool state survives ordinary panel visibility changes and workspace layout adjustments.

### 5.8 Section tool

- Sectioning is inspection-only and never changes export geometry.
- Plane sources are X, Y, Z, or a selected planar face.
- Offset is available as exact numeric input and as direct viewport manipulation.
- The scene-space arrow is attached to the section plane, follows orbit/pan/zoom, points into the removed half-space, and moves only along the plane normal.
- Shift provides fine adjustment; Escape rolls back an active drag.
- When the projected normal is edge-on or unstable, a large vertical slider appears at the left side of the viewport as a fallback.
- Both arrow and slider use relative drag with no acquisition jump and must be easy to grab.
- Dragging updates GPU/live clipping immediately. Exact section geometry is computed once after release, not on every pointer event.
- Flip changes the clipped half-space and arrow direction without changing the numeric offset.
- **View normal to section** and **View opposite side** change the camera side independently from section flip.
- Cap cut faces is independently controllable.
- **2D slice only** offers **Outline**, **Filled**, and **Filled + Outline**. Filled + Outline is the default; fills use body colors and outlines remain theme-aware.
- Exact overlays are asynchronous and lazy so the viewport stays navigable.

### 5.9 Measurement tool

Supported measurement modes are:

- Point to point
- Edge length
- Surface to surface
- Radius / diameter
- Angle
- Surface area
- Volume
- Bounds

Selection feedback is topology-specific:

- Points use screen-stable markers.
- Edges highlight the complete defining BRep edge.
- Faces highlight the complete CAD face with translucent fill and a crisp boundary.
- Bodies highlight for volume and bounds.

Hover/preselection uses the temporary highlight role. Accepted picks use a persistent amber role and numbered `1`, `2`, and subsequent markers. Face highlights use an X-ray pass so they remain legible through bodies and section caps. Highlights are non-pickable and follow orbit, pan, and zoom.

Measurement picks are accepted only while Measure is active. Ordinary viewport selection must never drop measurement points. Clicking the background clears component selection but does not erase accepted measurement picks. Clear removes picks; changing mode clears incompatible picks; loading a new document resets them.

The measurement backend must select and calculate from the requested topology. A face or edge measurement may not be represented internally as an arbitrary point or body-level approximation.

### 5.10 Capture

- Capture saves a PNG successfully through the native save dialog.
- The image reflects active projection, theme, visibility, appearances, section state, and current camera.
- Options include measurements, section caps, and transparent background where supported.
- Capture may temporarily configure rendering but must restore the live viewport and theme afterward.

### 5.11 Keyboard shortcuts

| Shortcut | Action |
| --- | --- |
| `F` | Fit assembly |
| `Shift+F` | Fit selection |
| `I` | Isolate / restore |
| `H` | Hide selection |
| `Shift+H` | Hide others |
| `Ctrl/Cmd+Shift+H` | Show all |
| `M` | Measure |
| `S` | Section |
| `1` | Solid |
| `2` | Solid + Edges |
| `3` | Edges Only |
| `Ctrl/Cmd+O` | Open STEP |
| `Escape` | Cancel active manipulation or close/restore the current task state |

Shortcuts must not fire while the user is editing a text, numeric, search, or filename field. Every shortcut action must also be reachable through visible UI.

## 6. Export workspace decisions

### 6.1 Direct selection model

The user-facing concept is an **Export bucket**, not a "build plan."

- Checking a component adds it to the bucket immediately.
- Unchecking removes it immediately.
- The checkbox is the only preview-side action that changes export inclusion.
- Search filters the component list without changing checked, hovered, or focused state.
- Checking a row preserves component-tree scroll position.

The internal immutable `ExportPlan` remains required for validation and execution, but it is not exposed as a workflow step.

### 6.2 Two-preview review desk

The Export workspace must provide two distinct previews:

1. **Master assembly preview:** always shows the overall assembly. The candidate component is highlighted through surrounding geometry with an X-ray treatment.
2. **Standalone preview:** shows only the hovered or focused component, automatically fit as the standalone file will be exported.

The component picker helps users decide whether to check an item:

- Hover updates both previews temporarily, even for unchecked components.
- Row click pins/focuses the candidate without checking it.
- Clicking a part in the master preview reverse-locates the nearest visible exportable ancestor and scrolls the component tree to it without changing checked state.
- Hover, focus, and checked inclusion are three independent states and must remain visually and logically distinct.

Both previews use the shared camera/input contract. Export previews omit Inspect-specific section, measurement, and capture tools.

Right-clicking a component in an Export preview provides:

- Fit selection
- Isolate
- Hide
- Show all

These visibility actions affect review presentation only and never alter checked state or export contents.

### 6.3 Export bucket

Each row shows both:

- Original component name
- Resulting output filename

This remains true when sequential naming is enabled. The row also exposes format/status/error, remove, and reorder controls.

Reordering contracts:

- A dedicated drag grip starts drag-and-drop; dragging the row content must not become bucket scrolling.
- A visible insertion indicator shows the drop target.
- Up/down controls remain as an accessible fallback.
- Reorder changes the current bucket order, sequential file numbering, and export execution order.
- Reorder operates by stable node ID and current model position, never by a stale QML delegate index.
- A manually edited filename remains attached to its component after reorder.

### 6.4 Naming

Supported strategies are:

1. **Keep component name**
2. **Sequential:** a user-entered base followed by zero-padded sequence numbers such as `part-001.step`
3. **Prefix component name:** a user-entered prefix plus the original component name

Users may override an individual output filename. Merely focusing the filename field must not create an override; an override begins only after an actual edit.

### 6.5 Settings and destination

- Destination uses the native folder chooser.
- The selected folder is converted from URL form to a valid local path and is immediately visible in the UI.
- Spaces and escaped characters are preserved correctly.
- Format supports STEP and STL according to backend capability.
- Coordinates expose only supported policies, including Assembly coordinates where implemented.
- Grouping remains separate files. Unsupported options stay disabled rather than implying unimplemented behavior.

### 6.6 Validation and export execution

- The primary command is labeled **Export N files**.
- It is enabled only when the bucket is nonempty, the destination and names are valid, and the document is final-ready.
- Any bucket, order, naming, format, coordinate, or destination change automatically rebuilds/revalidates the internal plan.
- Export executes in deterministic bucket order.
- Writes are atomic: incomplete content is never exposed under the intended final filename.
- Cancellation preserves already completed valid files and reports remaining/canceled rows accurately.
- Overall and per-row progress remain visible.
- Validation and failure are reported per row with actionable text.
- The worker-owned imported CAD document is the source for export. Preview meshes are never exported as substitutes for source geometry.

## 7. Architecture boundary for UI engineers

The UI consumes controller properties, models, invokables, and signals. It must not recreate their state machines in QML.

| Owner | Protected responsibility |
| --- | --- |
| `ApplicationController` | Document lifecycle, request generations, tree identity, selection, visibility, appearance/material state, geometry replay, and worker coordination |
| `ViewportNavigation` | Camera, projection, orbit, pivot, pan, zoom, fit, and projection-preserving framing |
| `PointerInputRouter` | Mouse-versus-trackpad wheel classification and input routing |
| `SectionController` | Section plane, offset, flip, interaction lifecycle, preview, and final section request |
| `MeasurementController` | Mode eligibility, topology picks, persistent pick identity, calculations, and results |
| `ExportWorkspaceController` | Picker state, hover/focus/check separation, bucket order, naming, destination, validation, and export execution |
| `ExportPlan` and validation core | Immutable export intent, collision checks, output validity, and deterministic execution contract |
| Worker protocol | Imported BRep ownership, asynchronous geometry/refinement, exact section work, and export writers |

Important implementation rules:

- QML delegates display model state; they do not own canonical component identity or export order.
- Async responses are accepted only for the active document/request generation.
- Cached/replayed geometry remains compacted and asynchronous.
- Section geometry remains lazy and shares immutable source buffers where possible.
- Hover requests are coalesced and stale responses discarded.
- A pointer-move path may update camera matrices, clipping values, or highlight state, but may not perform expensive CAD work.
- The source document remains immutable for the complete review session.

## 8. Performance and feedback budgets

- Direct manipulation feedback should appear within one rendered frame whenever possible.
- Orbit, pan, zoom, section preview drag, hover highlight, and reorder drag must not wait for backend geometry work.
- Operations longer than 400 ms need visible status feedback.
- Operations longer than 1 second need persistent progress; long refinement should include stage and ETA qualification.
- Workspace switching must present the target workspace immediately, then populate previews asynchronously.
- Avoid creating one heavyweight QML object per CAD face or edge when a model/batched rendering path is available.
- Do not eagerly duplicate complete assembly geometry for every preview or section overlay.
- UI animation is limited to transform/opacity-oriented feedback and must not interfere with pointer tracking.

## 9. Accessibility and multi-input requirements

- Desktop target: Windows 11 and macOS at normal desktop viewing distance, with resizable windows and high-DPI displays.
- Required inputs: mouse, trackpad, and keyboard.
- Tab order follows visible reading order and every interactive control has a visible focus state.
- Hover-only information must also be available through focus/click/pin behavior.
- Escape always provides a way out of transient menus, manipulations, and panels without losing unrelated work.
- Interactive targets remain usable at high DPI and with large system text.
- Text contrast is at least 4.5:1; non-text controls and highlights target at least 3:1 against adjacent colors.
- Selection, hover, validation, and disabled states differ by more than color alone.

## 10. UI refinement acceptance gate

A redesign is ready for user review only when all of the following remain true:

### Functional regression gate

- The complete automated suite passes. The functional handoff baseline is 87 passing tests.
- QML smoke tests cover theme, viewport navigation, pointer routing, section controls, projection controls, menus, and export workspace loading.
- Inspect and Export can be switched repeatedly without a crash or synchronous multi-second UI stall.
- A corpus STEP file can be opened, previewed, refined, inspected, and exported.
- Destination selection visibly updates and a real STEP/STL export is written and validated.

### Interaction gate

- Mouse wheel zoom and trackpad two-finger pan both work without mode toggles.
- Cursor-pivot orbit restarts without drift.
- Tree hover, viewport hover, persistent selection, measurement picks, export focus, and export inclusion remain distinct.
- Section arrow/slider drag follows the pointer smoothly and commits exact geometry only after release.
- Measurement point, edge, face, and body highlights match the selected topology and follow the camera.
- Export bucket reorder works by grip, insertion indicator, and button fallback while preserving filenames and tree scroll.

### Visual gate

- Light and dark themes include the viewport and all popup/control text remains readable.
- Menus are content-sized and remain inside the available window.
- No control labels overlap or clip at supported desktop sizes.
- Both Export previews communicate the current candidate clearly before the checkbox decision.
- Import/refinement and export progress cannot be mistaken for a frozen application.

### Document-integrity gate

- Review color, visibility, section, measurement, and capture operations do not alter exported source geometry.
- Exported component identity, placement, naming, order, and format match the validated bucket.
- Cancellation or failure never leaves a partial file under the final expected filename.

## 11. Reference implementation areas

- Application shell: `src/app/qml/Main.qml`
- Inspect workspace: `src/app/qml/inspect/InspectWorkspace.qml`
- Shared viewport: `src/app/qml/inspect/StepViewport.qml`
- Navigation and input: `src/app/qml/inspect/ViewportNavigation.qml`, `src/app/qml/inspect/PointerInputRouter.qml`
- Section UI: `src/app/qml/inspect/SectionPanel.qml`, `src/app/qml/inspect/SectionManipulator.qml`, `src/app/qml/inspect/SectionOffsetSlider.qml`
- Measurement highlighting: `src/app/qml/inspect/MeasurementFaceHighlight.qml`, `src/app/tools/MeasurementController.*`
- Export workspace: `src/app/qml/export/`, `src/app/export/ExportWorkspaceController.*`
- Theme controls: `src/app/qml/Theme.qml`, `src/app/qml/inspect/Themed*.qml`
- Export contract: `src/core/export/ExportPlan.*`, `src/core/validation/OutputValidator.*`
- Worker boundary: `src/app/worker/`, `src/worker/`, `src/protocol/`

The refinement team should treat these locations as integration points, not invitations to move protected domain behavior into visual components.
