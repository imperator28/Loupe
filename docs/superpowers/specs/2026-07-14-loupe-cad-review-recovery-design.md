# Loupe CAD Review Recovery Design

**Date:** 2026-07-14  
**Status:** Approved architecture, pending written-spec review  
**Scope:** Consolidated Phase 1 functional recovery  
**Supersedes:** The viewport rendering, navigation, selection, section, measurement, capture, assembly-tree, and display-style portions of `2026-07-14-loupe-functional-review-viewport-design.md`

## Purpose

Make Loupe a credible CAD inspection application by preserving OpenCASCADE BRep topology as the source of truth while using Qt Quick 3D only as the interactive display layer. The recovery is delivered as one review build covering the twelve defects identified during hands-on review. Broad certification, performance tuning, final visual design, Windows packaging, signing, and notarization remain outside this pass.

## Product Decisions

1. Imported STEP geometry remains an XCAF/OpenCASCADE document for the lifetime of the open document session.
2. Surface triangles are a display cache, not the product geometry and never the source of visible CAD edges.
3. Visible edges come from original `TopoDS_Edge` curves sampled for display at an adaptive tolerance.
4. Picking, measurement, and sectioning query retained BRep entities rather than inferring topology from rendered triangles.
5. Loupe keeps the Qt Quick 3D viewport. Replacing the application viewport with an embedded AIS/V3d native window is rejected for this phase because native child-window stacking and graphics-backend integration would compromise the QML tool interface on macOS.
6. The pass is implemented in dependency order but delivered for user review as one consolidated local build.

## Goals

- Smooth shaded bodies without visible faceting under normal review zoom.
- Correct CAD boundary curves in shaded-edge and edge-only display modes.
- Continuous unrestricted orbit and familiar CAD mouse navigation.
- Clear, persistent component selection synchronized between viewport and tree.
- Non-modal inspection tools whose panels can collapse independently of tool state.
- Exact, clean section views that can align the camera and be captured.
- Measurement picks that identify the selected component and entity.
- A concise assembly presentation tree without definition or compound noise.
- Reliable image capture with explicit success or failure feedback.
- Appearance controls that always show the currently resolved body color.
- Contrast-safe text, controls, outlines, and geometry on the dark workspace.
- A compact display-style control that cannot overlap other tools.

## Non-Goals

- Editing or healing STEP BRep geometry.
- Writing review colors, materials, visibility, or sections back into STEP.
- Pixel-perfect final UI redesign or final branding.
- Geometry certification or metrology-grade tolerance claims.
- Broad Windows regression, release packaging, signing, notarization, or full performance certification.
- Replacing OpenCASCADE tessellation with analytic GPU surface rendering. Raster display still uses triangles internally; the BRep remains authoritative and the display cache is generated from it.

## Architecture

### Persistent CAD Session

The worker owns one `DocumentSession` for the currently open source. It contains the imported XCAF document, occurrence-to-definition mappings, occurrence transforms, a topology index, source-unit conversion, and generated display caches. Completing mesh streaming does not destroy this session. Opening another source or closing the document explicitly retires it and invalidates outstanding query IDs.

The application process stores only presentation data and stable IDs. Exact geometry operations remain in the worker so the UI process does not duplicate the full BRep document or block on OpenCASCADE work.

Every document command carries a document generation and request ID. Responses from superseded documents or stale interactive queries are ignored.

### Topology Identity

Each selectable occurrence, body, face, edge, and vertex receives a stable session topology ID. IDs are derived from source identity, occurrence path, subshape kind, and deterministic subshape order within the definition. Occurrences keep independent IDs even when they share a definition.

The worker maintains bidirectional lookup between topology IDs and `TopoDS_Shape` values. Display payloads and query results use these IDs. Definition IDs remain metadata for material scope and repeated-instance behavior; they are not presentation-tree rows.

### Surface Display Data

The worker meshes each body from the retained BRep using scale-aware linear and angular deflection. The tolerance is bounded relative to body extent so small parts are not under-tessellated and large parts do not create unbounded payloads.

Each surface payload contains:

- Positions in effective millimetres.
- Per-vertex normals derived from the underlying BRep surface and corrected for face orientation.
- Triangle indices.
- Owning occurrence and body IDs.
- Face IDs or deterministic face ranges for query correlation.
- Resolved imported face, occurrence, or definition color.

Qt Quick 3D geometry declares position, normal, and index attributes. Shaded modes use the normals for smooth lighting. No triangle index is reused as an edge payload.

### CAD Edge Display Data

For every visible `TopoDS_Edge`, the worker evaluates its original curve over the valid parameter range and produces an adaptive polyline. Sampling respects curvature and display deflection; circles, arcs, splines, and trimmed curves retain their defining shape within the selected display tolerance. Degenerated edges are omitted.

Duplicate edges shared by adjacent faces are emitted once per body. The payload groups line strips by body and retains edge topology IDs. Section curves are a separate layer and are never mixed with body-boundary edges.

Display styles are:

| Style | Surface layer | CAD edge layer |
|---|---:|---:|
| Solid | On | Off |
| Solid + Edges | On | On |
| Edges Only | Off | On |

The current triangle-edge reconstruction in `MeshGeometry` is removed.

### Picking And Selection

Viewport selection sends the worker an occurrence/body candidate plus a model-space pick ray and active selection filter. The worker intersects the ray against retained BRep geometry and returns the nearest valid body, face, edge, or vertex with its topology ID, world point, normal or tangent, and display label.

Normal body-selection mode selects the body or occurrence, not an arbitrary tessellation segment. Measurement mode can request face, edge, vertex, or automatic entity selection. Stale query results are discarded when the camera, document, or active query generation changes.

Selection does not replace imported or user-defined color. A selected body receives a high-contrast outline generated from its CAD edge layer and a restrained surface emphasis. Hover and selected states are distinct. Clicking a tree row produces the same viewport selection state; clicking geometry expands ancestors, selects the matching tree row, and scrolls it into view.

### Camera And Navigation

The camera uses a quaternion trackball/orbit orientation around a stable pivot. There is no pitch clamp and no Euler singularity at top or bottom views. The model follows the configured Creo-style drag direction.

Mouse mapping is:

| Input | Action |
|---|---|
| Left click | Select |
| Left drag | Orbit |
| Middle drag | Pan |
| Shift + left drag | Pan |
| Wheel or pinch | Zoom |
| Right click | Context menu at cursor |

Pan uses the camera-local right and up vectors. Fit assembly and fit selection update both pivot and distance. Opening a new document resets to a fitted isometric view. Standard front, rear, left, right, top, bottom, and isometric orientations are represented as quaternions so every orientation remains a valid orbit starting point.

### Assembly Presentation Tree

The tree is rebuilt as a user-facing occurrence hierarchy:

- Named assemblies and subassemblies appear as grouping rows.
- Component occurrences appear as selectable rows.
- A single-solid component selects its body directly from the occurrence row.
- Multi-solid components expand to stable body child rows.
- Definition records, XCAF label entries, and anonymous technical compounds do not appear as peer roots.
- Anonymous one-child wrappers are flattened when doing so does not remove a named assembly boundary or change the occurrence transform.

The model keeps definition and compound information in metadata for export and appearance scope. Search matches component and body names. Viewport selection reveals the corresponding row automatically.

### Inspection Tool State

Tool operation state and panel visibility are separate properties. Collapsing or hiding a Section, Measurement, or Capture panel leaves the active operation unchanged. Disabling a tool requires its explicit toggle, context action, or cancellation command.

Task panels are non-modal anchored panels that reserve or overlay only a compact edge area. They never center over the geometry. Each panel has collapse, restore, and disable actions with distinct accessible names.

### Exact Section Workflow

The active section plane is stored in model coordinates as an origin and normalized normal. Plane changes are debounced and sent to the worker with a section query generation. The worker intersects the plane with visible BRep bodies and returns exact section curves, closed-wire grouping, and fill geometry where a valid closed section exists.

Normal section mode shows clipped shaded bodies, section boundaries, and optional cut-face fills. `2D Slice Only` hides every normal body surface and body-edge layer, leaving only section curves and optional fills. Visibility filtering is applied before section generation and consistently to all payload segments.

`Normal to Section` rotates the camera to the section normal, switches to orthographic projection, preserves the section center as pivot, and fits the visible section extents. Flip reverses both clipping direction and the normal-view direction. Returning to the prior view restores its projection and camera state. The aligned section state can immediately invoke Capture.

### Measurement Workflow

Each accepted measurement pick records:

- Pick sequence number.
- Component and body display names.
- Entity type and topology ID.
- Point coordinates.
- Face normal, edge tangent, radius, or other applicable entity metadata.

The measurement panel shows explicit `From` and `To` pick rows before showing a result. Viewport markers and entity outlines use matching numbered accents. Replacing or clearing a pick updates both the panel and viewport. Unsupported entity combinations show a local explanation and do not silently calculate from unrelated triangle points.

Distance, edge length, radius, and angle calculations use BRep query results in the worker. The UI controller formats effective units and presentation text only.

### Capture Pipeline

Capture uses the current resolved viewport presentation, including appearance overrides, selection only when requested, measurements when requested, and section fills when requested. The UI acquires a rendered `QImage`; a C++ capture writer converts the selected file URL to a local path and writes atomically through `QSaveFile`.

The operation reports `Saving`, `Saved`, or an actionable failure. Success is emitted only after the file exists, is non-empty, and decodes as the expected image format. Canceling the file dialog does nothing. Capture restores temporary background and overlay settings even when rendering or writing fails.

Section capture offers `Fit Section and Capture`, which performs normal-to-section, fits the section bounds, waits for the settled viewport frame, and then opens the capture destination flow.

### Appearance Resolution

The property panel swatch binds to the selected body's resolved appearance:

1. Manual body-color override.
2. Assigned material color.
3. Imported STEP face, occurrence, or definition color.
4. Neutral fallback.

There is no fixed neon placeholder. The swatch changes immediately when selection, material, scope, or override changes. `Clear Override` reveals the next layer without changing the imported source. User-created material colors remain synchronized with the material library and assigned bodies.

### Contrast And Display Controls

Workspace colors come from semantic theme roles rather than local literals. Primary and secondary text maintain readable contrast against every panel surface. Inputs, placeholders, disabled states, tree rows, menus, measurement markers, CAD edges, section curves, and selection outlines receive explicit foreground roles.

Normal CAD edges choose a light or dark theme edge role based on viewport-background luminance. Selection and section accents remain distinct from source colors and meet a minimum 3:1 contrast against the viewport. Text targets 4.5:1 contrast.

Display style moves out of the crowded bottom tool strip into one compact `Display Style` menu in the viewport controls. The menu contains the three mutually exclusive styles and exposes the current style through its icon, tooltip, accessible value, and checked menu item. Changing style never changes selection, visibility, section, or appearance.

## Protocol Changes

The worker protocol minor version is incremented. Existing open, progress, snapshot, and failure semantics remain, with these additions:

- Surface payloads include normals, body IDs, and face correlation data.
- CAD-edge payloads carry line strips and edge topology IDs.
- `PickQuery` / `PickResult` support BRep entity identification.
- `SectionQuery` / `SectionResult` support exact section curves and fills.
- `MeasurementQuery` / `MeasurementResult` support exact entity measurements.
- `CloseDocument` explicitly retires the persistent worker session.

Interactive queries are asynchronous, generation-tagged, and cancelable by replacement. A failed optional edge, pick, section, or measurement query does not invalidate the imported document; the UI reports the local failure and preserves the last valid viewport state.

## State And Error Handling

- Import failure returns the workspace to a recoverable empty state with the source path retained for retry.
- A body may render shaded if its edge extraction fails; Loupe reports a non-blocking display warning and never substitutes tessellation edges.
- Missing normals fall back to computed face normals for the affected face and record a warning.
- Pick queries time out locally without changing the current selection.
- Section updates preserve the last completed section until a newer result arrives, avoiding a blank flicker while dragging the plane.
- Capture failures retain the viewport and destination choice and state the write or encoding error.
- Invalid appearance data falls through to the next valid resolution layer.
- Empty or technically anonymous assembly labels receive stable human-readable `Component N` or `Body N` names within their parent.

## Implementation Order

Although delivered as one review build, implementation follows these dependencies:

1. Persistent worker document session and topology IDs.
2. Surface normals and true BRep edge payloads; removal of triangle-edge display.
3. Quaternion camera, corrected mouse mapping, and selection outline/tree synchronization.
4. Presentation-tree simplification and exact pick/measurement queries.
5. Exact section results, normal-to-section, and independent panel visibility.
6. Atomic capture writer and resolved appearance binding.
7. Semantic contrast pass and compact display-style control.
8. Corpus-based live review and focused regression fixes.

## Focused Verification Strategy

Validation remains proportional to functional UX approval. This pass does not run broad release certification.

### Deterministic Tests

- Import-session lifetime and stale-generation rejection.
- Surface payload includes normalized normals and contains valid triangle indices.
- Edge payload is derived from BRep edge count and never includes a face tessellation diagonal.
- Circle and spline edge sampling remain within configured display deflection.
- Tree fixtures omit definition roots and flatten only eligible wrappers.
- Quaternion orbit crosses top and bottom without clamping or discontinuity.
- Middle drag and Shift + left drag dispatch pan; left drag dispatches orbit.
- Selection synchronization resolves occurrence/body IDs in both directions.
- Slice-only mode disables all normal surface and edge layers.
- Measurement results retain component and entity identity.
- Capture writer handles macOS file URLs and verifies a non-empty decodable PNG.
- Appearance swatch follows override, material, imported color, and fallback precedence.
- QML smoke tests cover collapsed active panels and the display-style menu.

### Corpus Review With Computer Use

Each supplied STEP file is opened in the locally launched macOS build. The review checks:

1. Smooth shaded surfaces at fit and close zoom.
2. Correct curved BRep boundaries in `Solid + Edges` and `Edges Only`.
3. Full orbit through top and bottom, plus both pan gestures.
4. Selection outline and automatic tree reveal on representative bodies.
5. Tree clarity for single-part, multi-solid, and assembly sources.
6. Panel collapse while section and measurement states remain active.
7. Clean slice-only output, normal-to-section, and section capture.
8. Identified measurement picks and result calculation.
9. Successful PNG save to a temporary review destination.
10. Correct current-color swatch and readable text, lines, and controls.
11. Non-overlapping display-style and inspection controls at the review window size.

## Acceptance Criteria

- No tessellation diagonals appear in either edge display style.
- Curved surfaces shade smoothly enough that the supplied reference part does not show visible triangular facets at ordinary review zoom.
- CAD boundaries visually match the source STEP topology within the configured display tolerance.
- Camera orbit is continuous across every orientation and middle drag pans.
- Selected bodies are unmistakable without losing their source or review color.
- The tree exposes the component/body hierarchy without definition or anonymous-compound clutter.
- Tool panels can hide and restore without disabling their active feature.
- Slice-only shows no unrelated component surfaces or body edges.
- Normal-to-section produces a fitted orthographic section frame suitable for capture.
- Measurement identifies every accepted pick before presenting its result.
- Capture saves and verifies a non-empty PNG.
- Appearance always displays the selected body's resolved current color.
- Text, geometry edges, section curves, and selection outlines remain visible on the dark workspace.
- Display-style switching is compact, mutually exclusive, and non-overlapping.
- All supplied corpus files complete the focused Computer Use review without an application crash.

## Review Boundary

Passing this specification makes the functional build ready for UX polish and visual redesign. It does not constitute geometry certification, release readiness, or final UI approval.
