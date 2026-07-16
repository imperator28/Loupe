# Loupe Progressive CAD Interaction Design

**Date:** 2026-07-15
**Status:** Approved for implementation planning
**Scope:** Inspect workspace interaction, geometry presentation, theme support, and progressive opening performance

## Objective

Make Loupe's Inspect workspace behave like a credible desktop CAD review tool while preserving the current Qt Quick 3D and retained OpenCascade document architecture. The milestone adds clear selection behavior, topology-aware measurement feedback, direct section manipulation, theme-aware presentation, standard-view navigation, improved curved rendering, and a progressive geometry pipeline that reaches an interactive preview within approximately five seconds for the supplied 19 MB reference assembly on the current Mac.

This milestone remains functional-first. It establishes the interaction and rendering foundation needed before broad UI polish or release validation.

## Product Decisions

- Geometry opens through progressive adaptive tessellation.
- Refinement is visible, measurable, cancellable, and never silently implied to be complete.
- Theme selection offers `System`, `Light`, and `Dark`; the explicit override is persisted.
- A 2D section defaults to `Filled + Outline`, with `Outline` and `Filled` alternatives.
- The section plane has a draggable normal arrow with live numeric offset and Shift-modified fine movement.
- Section clipping direction and camera viewing side are independent.
- Measurement uses hover pre-highlight and persistent numbered accepted picks.
- A clickable view cube occupies the upper-right of the viewport.
- Background clicks deselect the active component. During measurement, accepted measurement picks remain until `Clear`; Escape cancels the pending pick.
- Viewport bodies and assembly-tree rows expose the same context menu.

## Progressive Geometry Pipeline

Opening is divided into four user-visible stages:

1. `Reading assembly` builds the XCAF hierarchy, names, colors, units, definitions, and occurrences.
2. `Preparing interactive preview` tessellates prioritized bodies at medium quality and streams each completed body.
3. `Interactive preview ready` enables navigation, selection, sectioning, appearance, and measurement.
4. `Refining geometry` replaces preview bodies individually with final adaptive tessellation and refined BRep boundary curves.

The worker emits a final privacy-safe metrics record for every completed source import. It contains only source basename and source hash, plus STEP-read, XCAF-transfer, snapshot-build, tree-ready, first-geometry, preview-ready, and final-ready elapsed milliseconds, body count, and preview/final triangle counts. These metrics are the performance contract for subsequent integration work; they distinguish upstream XCAF transfer from geometry encoding and renderer work.

The progress area reports the current stage, completed and total body counts, percentage, elapsed time, and estimated remaining time. `Interactive preview ready` remains visually distinct from `Final quality ready`. The user may cancel refinement while retaining the preview and all completed refined bodies.

Large visible bodies are prioritized ahead of small internal bodies. Repeated occurrences are deduplicated by definition: each unique definition is meshed once and reused with occurrence transforms. Meshing uses a bounded worker pool sized conservatively from available CPU capacity. Jobs that could share mutable OCCT triangulation state are not executed concurrently; independent definitions use isolated shape copies. The UI batches incoming geometry updates so model construction does not block pointer interaction.

The first implementation step prepares OCCT triangulation once per stable definition but continues to encode occurrence-scoped payloads. This preserves current per-occurrence source colors, selection identity, and renderer compatibility. Shared GPU buffers plus occurrence transforms remain the next step after the interaction contract is settled.

Geometry transport moves from newline-delimited JSON numeric arrays wrapped in base64 to a length-prefixed protocol envelope. Small control and progress messages remain compact JSON payloads inside the envelope; geometry uses typed binary payloads. Each geometry payload carries protocol version, request and definition identity, refinement level, segment identity, source color, bounds, occurrence transforms, vertex format, and index format. A strict maximum frame size and checked vertex and index counts reject corrupt input before allocation. Preview and final payloads share the same stable definition and segment keys, allowing atomic replacement without duplicate geometry or selection loss.

The cache stores preview and refined geometry independently. Cache keys include source fingerprint, effective unit decision, importer version, renderer version, and quality profile. A valid preview cache may be used while final geometry is absent or stale. Corrupt or incompatible entries are discarded without affecting the source file.

## Rendering Quality

Preview and final quality use adaptive chordal and angular deflection derived from body scale and bounded by absolute minimum and maximum tolerances. This avoids applying one world-space tolerance to both very large and very small parts. The final profile tightens both constraints and preserves normals computed from OCCT triangulations.

Defining edges remain separate BRep-derived line geometry rather than triangle wireframe. Edge sampling also uses adaptive linear and angular deflection. Theme-aware edge colors maintain contrast without changing imported STEP colors or user appearance overrides.

Qt Quick 3D continues to rasterize surface meshes; the source BRep remains retained in the worker for topology queries and exact section computation. This milestone improves tessellation quality and boundary fidelity without claiming analytic GPU surface rendering.

Refined body segments replace their preview counterparts atomically. A failed body refinement leaves its preview visible and marks that body as incomplete. Other bodies continue refining.

## Selection And Context Actions

A left click that hits no geometry clears the active component selection. The assembly-tree selection clears at the same time. Measurement picks are separate state and are not removed by background deselection.

Right-clicking a viewport body first selects that body and opens the component context menu at the cursor. Right-clicking an assembly-tree row selects the corresponding node and opens the same action model. Both surfaces expose:

- Fit selection
- Isolate or Restore isolation
- Hide
- Hide others
- Show all
- Restore previous visibility
- Properties

Actions resolve descendants consistently for assemblies and act directly for bodies. Disabled actions communicate why they are unavailable through state and tooltip text.

## Section Interaction And Presentation

The section plane remains inspection-only and never mutates export geometry.

The viewport presents a normal arrow anchored near the visible center of the section plane. Dragging is constrained to the plane normal. The numeric position updates continuously, and direct numeric changes move the manipulator. Holding Shift applies a fine-motion multiplier. Escape restores the offset from the start of the current drag.

Interactive dragging uses the current clipped-mesh section preview so feedback remains immediate. On drag release or committed numeric entry, the retained OCCT document computes exact BRep section curves per body. Closed planar contours are classified into outer boundaries and holes, then triangulated into body-colored planar fills. Settled 2D section presentation no longer depends on surface triangle intersections.

The section display choices are:

- `Outline`: exact BRep section curves only.
- `Filled`: body-colored planar fills only.
- `Filled + Outline`: body-colored fills with theme-aware boundaries; this is the default.

`Flip section` changes which half-space is removed. `View normal to section` aligns the camera to the current section normal and fits visible section geometry. `View opposite side` rotates the camera to the opposite normal without changing the clipped half-space or section offset.

If exact section construction fails for any body, its interactive section preview remains visible and the section panel reports the affected body. Successfully computed bodies continue using exact output.

## Topology-Aware Measurement

Viewport picking sends a camera ray and pixel tolerance to the retained worker session. The worker resolves stable occurrence, face, edge, or vertex topology identifiers against the original OCCT shape. Hover queries are transient and may be coalesced so stale results are discarded.

Hover feedback uses a subtle theme-aware pre-highlight. Accepted picks remain visible until cleared:

- Faces use a translucent face overlay plus boundary emphasis.
- Edges use a thicker high-contrast BRep curve.
- Points and vertices use a screen-stable marker.
- Accepted picks receive numbered `1` and `2` badges.

The measurement panel identifies each pick by component name, topology type, and relevant coordinates. It shows the result only when the selected topology satisfies the active measurement mode. Invalid combinations preserve the existing first pick and explain the required second selection.

During measurement, a background click clears only the active component selection. Escape cancels the pending pick or hover target. `Clear` removes all accepted measurement picks and the result.

## View Cube

A clickable view cube appears in the upper-right of the viewport and rotates with the camera. Faces snap to Front, Top, Right, Back, Bottom, and Left. Edges and corners snap to standard isometric combinations. The currently facing side is visually emphasized.

Snapping preserves the navigation target and fits the current visible geometry. The cube remains operable by mouse and keyboard, exposes accessible names for every target, and uses semantic theme roles so it remains legible on both white and dark viewer backgrounds.

## Theme System

Theme selection offers `System`, `Light`, and `Dark`. `System` follows operating-system appearance changes at runtime. Explicit Light and Dark choices are persisted across sessions.

A root-owned QML theme facade supplies semantic roles rather than feature-local color literals. `Main.qml` creates it from the persisted preference and passes that exact object to every workspace descendant and the `SceneEnvironment`; nested imports must not create independent theme instances. Roles cover application background, panel surface, elevated surface, viewport background, primary and secondary text, border, control surface, focus, CAD defining edge, hover topology, accepted measurement picks, active selection, warning, and error.

Theme changes update UI chrome, text, controls, viewer background, lighting, edge colors, section outlines, hover feedback, selection feedback, view cube, and capture background choices as one transaction. Imported STEP colors, material-library colors, and per-body appearance overrides are never altered by theme selection.

`ViewportVisualTheme` is the sole renderer-facing projection of the root facade. `SceneEnvironment`, renderer lights, model/edge materials, and canvas overlays stay declaratively bound to it for the lifetime of the viewport. Transparent capture is a temporary `ViewportVisualTheme` override; it never writes or later restores `SceneEnvironment` values directly. This preserves the Light canvas binding across capture, refinement, and subsequent theme changes.

The light viewport uses a white background with dark defining edges. The dark viewport uses a near-black neutral background with light defining edges. Selection and measurement use both color and shape or line-weight changes so state does not rely on color alone.

## Error And Cancellation Behavior

- Canceling initial import returns to a stable empty or previous-document state.
- Canceling refinement keeps the interactive preview and completed refined bodies.
- A failed final refinement identifies affected bodies and retains their previews.
- A failed exact section retains the interactive mesh preview and identifies affected bodies.
- Stale hover, refinement, and section results are rejected by request generation.
- Reopening a file cancels outstanding work from the prior document without accepting late geometry.
- Cache failure never blocks opening from source.

## Performance Targets And Evidence

For every supplied corpus file, record:

- tree-ready time;
- first-body-visible time;
- interactive-preview-ready time;
- final-refinement-ready time;
- peak resident memory;
- preview and final triangle counts;
- cache-hit and cold-open status.

The supplied 19 MB motor assembly targets interactive preview within approximately five seconds on the current Mac. The 2026-07-16 cold baseline is 29.37 seconds preview-ready (18.95 seconds tree-ready, including 15.30 seconds XCAF transfer) and 73.28 seconds final-ready. Final refinement may take longer, but its progress must remain explicit and the preview must remain fully interactive. Reaching the target requires cache-first replay and definition reuse; it cannot be achieved by changing final tessellation tolerance alone.

Performance work focuses on binary transport, bounded parallel meshing, body prioritization, UI batching, and dual-level caching. Quality tolerances are not silently loosened merely to report a shorter final-refinement time.

When baseline evidence shows tree-ready dominates the open path, cache replay and source-import work take precedence over parallel refinement. In particular, a valid cached preview must be replayed before XCAF transfer, and repeated definitions must be encoded once with occurrence transforms. Parallelizing tessellation alone is not considered a remedy for source-transfer delay.

## Verification Strategy

Automated coverage includes:

- binary protocol round trips, bounds checks, version rejection, cancellation, and stale-generation rejection;
- preview-to-final body replacement without duplicate models or lost selection;
- adaptive quality behavior across small and large curved fixtures;
- exact section outer loops, holes, per-body fills, colors, and opposite-side viewing;
- section manipulator offset constraints, Shift fine movement, commit, and Escape rollback;
- background deselection and measurement-state independence;
- topology hover and accepted face, edge, and point highlights;
- shared viewport and tree context-action models;
- view-cube standard and isometric snaps;
- System, Light, and Dark persistence and contrast checks;
- preview/refinement cache invalidation;
- cold and warm corpus performance metrics.

Live review uses the supplied STEP corpus to judge curved rendering, BRep edge fidelity, section manipulation, topology clarity, view-cube behavior, theme readability, and perceived responsiveness. Broad release certification, exhaustive geometry validation, packaging, signing, and export-workspace completion remain outside this milestone until the Inspect UX is approved.

## Acceptance Criteria

- Clicking empty viewport space clears active component and tree selection.
- A 2D section supports Outline, Filled, and Filled + Outline with the approved default.
- The section arrow directly changes offset and remains synchronized with numeric input.
- The same section can be viewed from either side without changing clipping direction.
- System, Light, and Dark themes update the full UI and CAD presentation coherently.
- Measurement hover and accepted topology are visibly distinguishable and identified in the panel.
- Tree and viewport right-click actions are functionally identical.
- Final curved rendering is materially smoother than the current fixed-tolerance output and uses BRep defining edges.
- The view cube supports standard face, edge, and corner views.
- The 19 MB reference assembly reaches an interactive preview in approximately five seconds on the current Mac.
- Refinement progress is explicit through final readiness and may be canceled without losing the preview.
