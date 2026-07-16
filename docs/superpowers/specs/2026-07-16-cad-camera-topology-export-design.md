# Loupe CAD Camera, Topology Measurement, and Export Workspace Design

**Date:** 2026-07-16
**Status:** Approved design

## Objective

Improve Loupe's CAD interaction model in five connected areas:

1. Open assemblies in an orthographic isometric view while retaining an explicit Perspective option.
2. Replace the viewport-centered section offset overlay with a model-attached section manipulator.
3. Make measurement selection and highlighting match the topology being measured.
4. Eliminate cursor-pivot drift when a new orbit drag begins.
5. Establish the first functional Export workspace vertical slice on the existing export contracts.

The pass remains focused on inspection and selective export. Loupe does not become a CAD editor.

## Interaction Decisions

### Projection and standard views

Loupe opens each document with an orthographic camera in an isometric orientation. Isometric describes the initial orientation; orthographic describes the projection that removes perspective convergence.

A compact projection control offers:

- Orthographic
- Perspective

Switching projection preserves orientation, orbit pivot, viewport center, and apparent model size. Top, Front, and Right remain orientation shortcuts and do not implicitly change projection.

Fit operations calculate the required orthographic magnification or perspective distance from the same visible bounds. Capture uses the active projection.

### Cursor-pivot orbit

The orbit pivot is separate from the navigation target and camera rig. Pressing over geometry records a pending world-space pivot but does not move, pan, or re-aim the camera. The pending pivot becomes active only when the pointer crosses the drag threshold.

Each orbit update rotates the camera position and orientation around the active world-space pivot. Starting a second drag therefore cannot cause a retargeting frame before rotation. A press over empty space retains the previous pivot.

Pan translates the camera and active pivot together. Zoom changes the projection scale or camera distance without changing the pivot. Fit resets the pivot to the fitted bounds center.

### Section manipulator

The primary section interaction is a scene-space arrow:

- anchored to the active section plane inside the visible model bounds;
- aligned with the effective section normal, including the flipped direction;
- transformed with the scene so it follows orbit, pan, and zoom;
- visually scaled to remain usable without appearing detached from the model;
- labeled with the current offset in the effective document unit.

Dragging projects pointer movement onto the arrow's screen-space axis and converts that motion to model-space offset. Shift-drag applies fine adjustment. Escape restores the drag-start value. Double-click resets to the section plane's initial offset.

When the projected normal is too short for stable manipulation because it is nearly parallel to the camera direction, the arrow remains visible as an orientation cue and a compact vertical slider appears along the left viewport edge. The fallback slider does not cover the model center and disappears when direct manipulation becomes stable again.

The numeric offset field in the Section panel remains available for exact entry.

## Topology-Aware Measurement

### Current limitation

The current viewport pick returns a body-level model, one point, and one normal. Edge length, radius, and surface values are body-level aggregate metadata. This cannot accurately identify or highlight the edge or face intended by the user.

### Imported topology identity

The worker preserves stable document-generation-local identifiers for selectable topology:

- body or solid;
- face;
- edge;
- circular edge or cylindrical/circular face where radius is defined.

Mesh triangle ranges map to face identifiers. CAD edge polyline ranges map to edge identifiers. Each entity carries the exact metrics required by its supported measurement modes, such as face area, edge length, curve type, radius, center, axis, and face normal or plane.

Topology identifiers are deterministic for the same imported document and refinement generation. They are not persisted as cross-file universal IDs.

### Picking and highlights

A C++ topology picker owns ray and proximity selection. It resolves visible mesh triangles and CAD edge curves to topology identifiers without creating one QML object per face or edge.

Measurement mode constrains eligible entities:

| Mode | Eligible entity | Feedback |
|---|---|---|
| Point to point | Surface hit point | Numbered point marker |
| Edge length | Edge or curve | Full edge highlight |
| Surface distance | Planar face | Full face highlight plus pick number |
| Radius / diameter | Circular edge or supported curved face | Full circular entity highlight and center cue |
| Angle | Edge or face | Full selected entities plus pick numbers |
| Surface area | Face | Full face highlight |
| Volume / bounds | Body or component | Existing body highlight |

Hover uses a lightweight pre-highlight. Accepted measurement picks use persistent numbered highlights. Changing mode clears incompatible picks. Closing the Measurement panel stops measurement capture but retains the last result until explicitly cleared or a new document opens.

Highlight geometry is generated only for the current hover entity and accepted picks. It follows all camera movement because it remains in scene space. Point labels are projected overlays whose positions are invalidated by navigation revision.

### Protocol and refinement

Topology identity and coarse pick ranges arrive with the preview geometry so measurement becomes usable early. Exact metrics and refined ranges may replace preview data during refinement without changing entity semantics. Refinement progress continues to report its current stage and percentage.

Malformed topology metadata disables entity-specific measurement for the affected body and reports an actionable status; it does not crash the viewer or silently present aggregate data as an exact entity result.

## Export Workspace

### Structure

Export is a nonmodal review desk rather than a wizard. It loads when first entered and preserves its draft while the document remains open.

The workspace contains:

1. **Component selection:** searchable hierarchy with persistent export checkboxes independent from active highlight.
2. **Assembly context:** shows checked and focused components in assembly placement.
3. **Focused output preview:** isolates the highlighted output candidate for confirmation.
4. **Export settings:** destination, STEP or binary STL, coordinates, grouping, and output unit choices supported by the core plan.
5. **Output plan:** deterministic filename preview, hierarchy source, format, coordinate policy, status, warnings, and blockers per row.
6. **Run bar:** selected output count, blocking summary, cancellation while running, and one explicit Export action.

No critical export policy is hidden in a modal dialog. Highlighting a row changes preview focus but does not add or remove it from the checked export set.

### First functional vertical slice

The initial implementation supports the behavior already proven by the core:

- select occurrences, definitions, bodies, or supported subassemblies;
- export separate STEP or binary-millimeter STL files;
- choose supported coordinate and STEP output-unit policies;
- choose a destination;
- build an immutable reviewed plan;
- display unsafe names, collisions, unresolved units, and incompatible policy combinations before execution;
- execute without mutating the imported source document;
- show per-output progress and cancellation;
- display validated, warning, skipped, and failed results per row.

Controls whose backend policy is not yet supported remain unavailable with a concise reason. The UI does not imply that preserve-assembly or flattening behavior works before the core accepts that plan.

### Failure and recovery

Plan errors are attached to the affected setting or output row. Execution failures preserve successful outputs and make failed rows retryable after correction. Cancellation stops scheduling new rows, waits for the active atomic write to finish or roll back, and reports what completed.

Unit uncertainty continues to block export until reviewed. Existing files are never partially overwritten; atomic output rules remain authoritative.

## Architecture Boundaries

- `ViewportNavigation` owns projection-independent camera state, pending and active orbit pivots, and fit calculations.
- `StepViewport` owns presentation, input routing, section manipulator display, and projection controls.
- `SectionController` remains the source of section plane, normal, offset, flip, cap, and slice state.
- Worker geometry encoding owns topology enumeration, identifiers, exact entity metrics, and pick-range payloads.
- A render-layer topology picker and highlight geometry own interactive resolution and feedback.
- `MeasurementController` owns mode eligibility, accepted entity picks, results, descriptions, and clearing rules.
- The Export module owns draft selection, settings, plan construction, execution state, and results; immutable core `ExportPlan` remains the execution contract.

## Delivery Order

1. Replace camera state and orbit math; add orthographic default and projection switching.
2. Replace the section overlay with the scene-space manipulator and edge-on fallback.
3. Extend worker geometry and protocol with topology identity and exact entity metrics.
4. Add topology picking, hover feedback, persistent entity highlights, and measurement integration.
5. Build the Export workspace vertical slice against the existing core contracts.
6. Run focused interaction tests after each step, then the full debug gate and corpus review build.

The order keeps camera and section behavior reviewable while the larger topology protocol change is developed.

## Verification

Automated coverage includes:

- projection switching preserves framing and orientation;
- orthographic isometric is the document default;
- a pivot-only press does not change camera state;
- consecutive orbit drags produce no pre-rotation translation;
- pan and zoom preserve world attachment of pivots, manipulators, and highlights;
- section drag follows projected normal direction and fallback thresholds;
- face and edge ranges round-trip through the worker protocol;
- measurement modes reject incompatible entities and use exact entity metrics;
- hover and accepted highlights represent complete selected entities;
- checked export state remains independent from highlight;
- plan errors and output rows match the immutable core plan;
- cancellation and per-row results remain deterministic.

Manual corpus review covers both themes, both projections, repeated cursor-pivot drags, oblique and edge-on section normals, every measurement entity type, large-assembly responsiveness, and one STEP plus one STL export with read-back evidence.

## Acceptance Criteria

- Assemblies open in a distortion-free orthographic isometric view.
- Switching projection causes no camera jump or unexpected reorientation.
- Re-initiating orbit over any visible component causes no drift before rotation.
- The section arrow is spatially attached to the plane and moves along its normal.
- Measurement feedback highlights the actual selected point, edge, face, circle, or body.
- Reported edge, face, and radius values belong to the selected entity rather than a body-level proxy.
- Export users can construct, review, run, cancel, and inspect a supported selected-output plan without leaving the workspace.
- Source STEP data remains immutable throughout inspection and export.
