# Loupe 2D Drawing Export — Product Requirements

**Date:** 2026-07-24
**Status:** Draft for review
**Product phase:** Phase 4 — the first phase after the functional baseline (roadmap Phases 0–3) and the UI refresh (workstreams W1–W8)
**Target release:** `v0.2.0`
**Governing UI language:** [Indigo Precision (v2)](../../design.md)
**Companion authority:** [UI refinement handoff](../review/ui-refinement-handoff.md)
**Scope:** A third workspace, peer to Inspect and Export, that produces true-scale 2D vector files from a STEP file
**Owner:** Project user for product, architecture, and corpus

---

## Purpose

An engineer or model maker who wants to prototype a part today has two bad options: run a full CAD drafting workflow to produce a 1:1 cut file, or eyeball it. The drafting path is the standard one — open the assembly in SolidWorks or Fusion, create a drawing, place a view, set the scale to 1:1, suppress the annotations nobody needs, export DXF, repeat per part. That is fifteen to thirty minutes of drafting ceremony to produce a file whose entire information content is *an outline at the right size*. It has no dimensions, no title block, no tolerances, and no revision history, because nothing downstream reads them: a laser cutter reads closed contours.

Loupe already opens a STEP file in seconds, already resolves units rigorously, and already has a reviewed-batch export pipeline. The gap between "I can see the part" and "I can cut the part" is one workspace wide.

**2D Drawing Export lets a user pick components, define a view angle per drawing, queue them, and batch out DXF, SVG, or PDF at exact 1:1 — with no drafting step.**

This is the feature that changes Loupe's value proposition from *convenient* (a fast STEP viewer with 3D export) to *load-bearing* (the tool that gets a prototype onto the laser today).

---

## Users and Jobs

### The design engineer — "is this geometry right at real size?"

Owns the CAD. Needs the output to be trustworthy before it consumes material.

| Job | Requirement it drives |
| --- | --- |
| "Cut a 3 mm acrylic plate that matches this part's footprint." | Exact 1:1, verifiable. A 0.2% scale error is a scrapped part. |
| "The face I'm cutting from isn't axis-aligned." | View normal to a selected face, not just the six standard views. |
| "Holes must stay round." | Circles and arcs preserved as arcs, not 200-segment polylines. |
| "I need to know what's in the file before I send it." | A 2D preview of the actual drawing, with its measured bounding box. |
| "Millimetres, definitively." | Reuse the existing `UnitPolicy` gate; refuse to export on an ambiguous unit decision. |

### The prototyping model maker — "give me a folder of files that just work"

Runs the laser, the craft cutter, the vinyl plotter. Lives in LightBurn, RDWorks, Glowforge, Cricut Design Space, LibreCAD.

| Job | Requirement it drives |
| --- | --- |
| "Twelve parts, one job, one folder." | Batch queue, one file per drawing, predictable names. |
| "My cutter needs closed paths." | Stitch HLR output into closed wires; report open contours as a warning, not silently. |
| "Don't cut the same line twice." | Deduplicate coincident edges — a double-cut burns the material and wastes time. |
| "Don't hand me a mirrored part." | Per-format Y-axis handedness must be correct and regression-tested. |
| "Let me set power per feature." | Named layers by content class (cut / outline / hidden / reference), not one flat soup. |
| "Prove the scale before I hit go." | An optional 50 mm fiducial on a non-cut reference layer. |

### The product manager — "does this earn its complexity?"

| Concern | Position |
| --- | --- |
| Differentiation | Strongest wedge available. No fast STEP viewer does credible 1:1 2D output; every full CAD suite does it slowly. |
| Scope risk | High. A "2D export" invites dimensions, title blocks, section views, nesting, and eventually a 2D CAD editor. The non-goals below are load-bearing, not decorative. |
| Credibility risk | One wrong-scale file that scraps a customer's stock costs more trust than this feature earns in a month. 1:1 correctness is a release gate, not a feature. |
| Cost | Low dependency cost — the projection engine is already shipping (see Architecture). The work is real but contained. |
| Format priority | DXF is the shop lingua franca and ships first. PDF is the most reliably 1:1 and is genuinely used (print, tape to stock, cut by hand). SVG serves craft cutters and illustration. |

---

## Scope

### In scope

1. A third workspace, **Drawing**, peer to Inspect and Export.
2. Selection of components and individual bodies, reusing the existing picker behaviour (search, collapse, select-all, multi-solid body splitting).
3. **View definition per drawing:** the six standard orthographic views, plus *normal to a selected face*.
4. A **rotating 3D view cube**, replacing the current static one, making all six standard views directly clickable and continuously showing the part's orientation. Shared with Inspect.
5. **Three content modes per drawing:**
   - **Outer Contour Only** — the part's silhouette: its physical outer boundary, plus through-holes. Every interior edge is suppressed, including step, chamfer and fillet lines that are visible but do not describe where material ends. This is what you want when the question is "what shape do I cut from stock".
   - **Cut Contours** — outer profile and through-holes plus interior visible edges, as closed paths. No tangent lines, no hidden lines. What most CAD tools call "no hidden lines, no shading".
   - **Technical View** — every visible edge, including smooth/tangent edges. Reads like a CAD drawing view; for templates and visual reference.
6. **Scale** per drawing, default 1:1, with a small preset set and an explicit ratio entry.
7. A **live 2D preview of the candidate drawing before it is queued**, with measured extents and per-drawing warnings.
8. A **drawing queue** holding any number of drawings per part, reorderable, with per-drawing filename override. One output file per queued drawing.
9. **Formats:** DXF, SVG, PDF, chosen once for the batch (matching how the 3D Export workspace treats format).
10. Batch execution with per-row progress, post-write validation, and atomic file replacement.
11. Named output layers by content class.
12. An optional scale-verification fiducial.

### Out of scope — explicit non-goals

These are refusals, not backlog items. Each one is a door that turns this workspace into a drafting application.

- **No dimensions, leaders, annotations, or text callouts.** Not a drawing tool.
- **No title blocks, borders, sheet frames, or revision tables.**
- **No GD&T, surface finish, or weld symbols.**
- **No nesting or part packing.** One drawing per file; layout is the CAM tool's job.
- **No kerf or tool-offset compensation.** Belongs in CAM, where the operator knows the machine and material.
- **No sheet-metal unfold / flat pattern.** Genuinely valuable, genuinely a separate feature (bend detection, K-factor, relief modelling). Deferred.
- **No cross-section drawings** in this release. Deferred as the leading candidate for the next iteration; noted in Open Decisions.
- **No perspective projection.** Orthographic only — perspective destroys 1:1 by definition.
- **No G-code or machine-specific output.**
- **No custom-plane / arbitrary-angle views in this release.** The six standard views plus face-normal cover the overwhelming majority of prototyping work; the custom-plane UI is the natural Phase 3.2 follow-on.

---

## Three Findings That Shape This Release

The first two surfaced during technical investigation, and both need an explicit product decision because both can silently produce a *wrong file that looks right*. The third is a usability ceiling in existing shared UI that this workspace cannot work around.

### 1. The view cube's axis labels do not match CAD convention for most STEP files

The worker emits STEP coordinates verbatim; Qt Quick 3D renders Y-up. The existing view cube therefore treats **model +Z as "Front" and model +Y as "Top."** Most STEP files from mechanical CAD are Z-up, which means the cube's Top and Front are effectively swapped relative to what an engineer means by those words.

In the Inspect viewport this is a cosmetic annoyance — the user orbits until it looks right. **In a drawing export it is a correctness bug**: "Top view" would produce a front elevation, and the user would not necessarily notice until the part was cut.

**Requirement:** the Drawing workspace MUST resolve standard view names against the document's own up-axis, not the renderer's. The workspace MUST display which axis convention it inferred, and MUST let the user override it. This is a required part of the first release, not a refinement.

### 2. Y-axis handedness differs per output format

DXF model space and PDF user space are both Y-up; SVG is Y-down. A drawing written to all three formats from one contour set without per-format handling produces **two correct files and one mirrored file** — and which one is the odd one out is easy to get backwards, since PDF looks like it should behave like SVG but raw PDF is Y-up like DXF. For a chiral part — an L-bracket, an asymmetric mounting plate — a mirrored file is scrapped stock, and it is not obvious on screen.

**Requirement:** handedness MUST be handled per format, and MUST be covered by a regression test using a deliberately chiral fixture, asserted in all three formats. SVG arc sweep direction MUST be inverted alongside the Y flip.

### 3. The existing view cube is a static drawing, not a cube

The current cube is a flat axonometric illustration — a hexagon split into three quads, drawn with 2D vector shapes. It never rotates, so it can only ever expose **three of the six standard views** by click. Bottom, Back, and Left are reachable only through a right-click menu, which is undiscoverable and does not read as part of the cube at all.

For Inspect this is a mild inconvenience. For a workspace whose entire purpose is choosing a view direction, it is the primary control, and it currently hides half of its own function behind a hidden menu while giving no feedback about the part's current orientation.

**Requirement:** replace it with a **real 3D view cube that rotates in lockstep with the part**, so every face becomes directly clickable as the user orbits, and the cube continuously communicates the part's current orientation. This is shared Inspect infrastructure — Inspect gets the improvement too — and it is also where Finding 1's axis-convention fix lands, since the cube is what asserts what "Top" means.

---

## Rotating 3D View Cube

Replaces `ViewCube.qml` for both Inspect and Drawing. Behaves the way a CAD view cube is expected to behave.

| Requirement | Level |
| --- | --- |
| Renders as a real 3D cube whose orientation tracks the main camera continuously, so it always mirrors how the part is currently oriented | MUST |
| All six faces directly clickable whenever they face the viewer; no standard view reachable only through a hidden menu | MUST |
| Clicking a face aligns the camera to that face's normal | MUST |
| Face labels follow the document's resolved up-axis, not the renderer's (Finding 1), and the inferred convention is visible | MUST |
| Labels stay upright and legible, and are hidden for faces turned away from the viewer | MUST |
| Hover highlights the specific face under the cursor, using the existing accent-tint treatment | MUST |
| Six keyboard/assistive stops, one per face, preserving the current accessibility behaviour rather than regressing it | MUST |
| Camera transition to a clicked face is animated on the spatial motion token, honouring the reduced-motion setting | SHOULD |
| Corner and edge clicks for isometric and half views | MAY — deferred |
| Drag the cube directly to orbit the part | MAY — deferred |

### Where the cube appears

The cube MUST be present in **every viewport the user can orbit**, because in each of them the user needs to know the part's current orientation and needs one-click access to a standard view:

| Viewport | Today | Required |
| --- | --- | --- |
| Inspect | Static cube, three clickable faces | Rotating cube, six clickable faces |
| Export — master assembly preview | No cube | Rotating cube |
| Export — standalone component preview | No cube | Rotating cube |
| Drawing — 3D preview | n/a (new) | Rotating cube, and it is the primary view control |
| Capture output | Hidden | Stays hidden — it is viewport chrome, never part of an exported image |

The Export previews are marked presentation-only, which currently suppresses the cube wholesale. Presentation-only MUST stop implying "no cube": the viewport already distinguishes suppressed chrome from opted-in chrome for the display-mode control, and the cube MUST follow that same opt-in so an embedded preview can request it without inheriting the full interactive toolbar.

Constraints:

- The cube MUST NOT introduce a measurable frame-rate cost. It is small and simple, but it adds another 3D surface to viewports that already composite several, and the Export workspace shows **two** previews at once — so the budget MUST be measured with both previews live, not assumed.
- The cube MUST stay hidden while a capture is in progress, so it never appears in an exported image.
- All colours, sizes, and durations MUST come from the `Theme` singleton — the mechanical style gate permits no literals and no new exemptions.

---

## Workflow

```
1. Open a STEP file (existing flow, shared document)
2. Switch to Drawing
3. Pick a component or body in the tree          → 3D preview highlights it
4. Choose a view:
     · click a face on the rotating 3D view cube, or
     · click a face on the part itself → "Normal to face"
5. Choose content mode (Cut Contours | Technical View) and scale (1:1)
       ⤷ the 2D preview renders the resolved drawing LIVE, with its
         measured extents, and re-renders on every change to 3/4/5
6. Confirm what the preview shows, then "Add to queue"
     (repeat 3–6; the same part may be queued at as many angles as needed)
7. Choose format (DXF | SVG | PDF) and destination
8. Export                                        → one validated file per queued drawing
```

### Outer Contour Only

The distinction matters more than it sounds. A stepped or chamfered part projects plenty of edges that are genuinely visible yet describe nothing you would cut — a step's top face reads as a horizontal line straight across the middle of the part. For cutting stock to size, those lines are noise at best and a wrong cut path at worst.

Requirements:

- The mode MUST emit only contours that bound material: the outer silhouette, and any through-hole boundaries.
- It MUST suppress interior edges regardless of visibility, including step, chamfer and fillet boundaries.
- Where the silhouette cannot be isolated reliably, the drawing MUST say so rather than silently degrading to the full edge set — the two are visually similar enough that a user would not notice.

**Known dependency, measured.** Isolating a silhouette needs closed contours to work from: the operation is a planar union of the projected regions, and a region needs a boundary. Corpus measurement puts closure at **73%** of contours (45 of 62), and on one part — the very stepped bracket that motivates this mode — **nothing closes at all**. So this mode cannot be delivered on top of the current stitching; closure has to improve first. That ordering is not a preference, it is what the numbers require.

### Preview Before Commit

The 2D preview MUST render the candidate drawing **before it is added to the queue**, and MUST update live as the component, view, content mode, or scale changes. Queueing is a confirmation of something the user has already seen, never a blind action whose result is discovered afterwards.

This is the workspace's primary defence against wasted material. A user who can see the outline and its measured extents before committing catches a wrong view, a wrong body, or a wrong scale in one glance — the failure modes that otherwise surface only after a cut. Requirements:

- The preview MUST show the resolved 2D geometry, not a 3D projection of the highlighted part.
- The preview MUST display measured extents in the active unit (for example `184.2 × 96.0 mm`).
- The preview MUST surface per-drawing warnings — open contours, curve-fallback degradation — at candidate time, so a problem drawing is never queued unknowingly.
- The preview MUST indicate when it is still computing, and MUST NOT present stale geometry as current.
- Selecting an existing queue row MUST show that row's drawing in the same preview, so a queued drawing can be re-inspected.

Because exact projection is too slow to run on every keystroke, the candidate preview MAY use a faster mesh-based projection, but MUST then be visibly labelled as a preview approximation, and the queued export MUST always use the exact path. A preview that silently differs in fidelity from the output would defeat its own purpose.

### One Part, Many Drawings

The critical structural difference from 3D Export: **a drawing is not a component.** The unit of work is a `(component or body) × view × content mode × scale` tuple.

The queue MUST therefore support **any number of drawings per part**, and this is a primary case, not an edge case — a single bracket routinely needs Top, Front, and a face-normal view. Consequences that MUST hold:

- The queue is an independent ordered list keyed by a generated drawing ID. It is **not** a checkbox state over the component tree, because a checkbox cannot express "this part, three times, at different angles."
- Adding the same part again with a different view MUST create an additional queue row, never replace the existing one.
- Adding a duplicate that matches an existing row on every field (part, view, content mode, scale) SHOULD be surfaced as a duplicate rather than silently producing two identical files under different names.
- Default filenames MUST disambiguate by view, so three drawings of one part do not collide or require manual renaming. Generated names carry the view label; a non-1:1 scale MUST also appear in the name.
- Removing one drawing MUST NOT affect other drawings of the same part.
- The component tree MAY indicate how many drawings a part currently has queued.

A queued drawing MUST capture its **resolved view direction as a vector at the moment it is added**, not a reference to live camera state. Adding a drawing is a snapshot; later orbiting must never silently change a queued drawing.

---

## Interface

Layout follows the established three-region workspace shape, and every token comes from the `Theme` singleton — no literal colours, spacings, radii, or durations, per the mechanical style gate.

Panel order deliberately follows the mental model: **configure → see → commit.** The 2D preview sits above the queue and above the add action, so the confirmation is read before the button is reached.

```
┌── Components ──┬── 3D preview ────────────┬── Drawing setup ──────┐
│ search         │                ┌─────┐   │ View:    [ Top     ▾] │
│ select/deselect│              ┌─┤ Top ├─┐ │ Content: [Cut │ Tech] │
│ tree + bodies  │   part       │ └─────┘ │ │ Scale:   [ 1:1     ▾] │
│                │   clickable  │ F  │  R │ ├── 2D preview ─────────┤
│  Plate     ②   │   faces      └────┴────┘ │                       │
│  Bracket   ①   │            rotating cube │   ▁▁▁▁▁▁▁▁▁▁▁▁        │
│  Spacer        │                          │  │  ◦        ◦ │      │
│                │                          │  │            │      │
│                │                          │   ▔▔▔▔▔▔▔▔▔▔▔▔        │
│                │                          │   184.2 × 96.0 mm     │
│                │                          │   ⚠ 1 open contour    │
│                │                          │  [ + Add to queue ]   │
│                │                          ├── Drawing queue ── 3 ─┤
│                │                          │ ⋮ Plate · Top         │
│                │                          │ ⋮ Plate · ⟂ Face      │
│                │                          │ ⋮ Bracket · Front     │
│                │                          ├── Output ─────────────┤
│                │                          │ Format ▾   Destination │
│                │                          │ [ Export 3 files ]    │
└────────────────┴──────────────────────────┴───────────────────────┘
```

Requirements:

- The component picker MUST reuse the Export picker's interaction model exactly — same disclosure behaviour, same search, same select-all/deselect-all, same multi-solid body rows. A user who has learned Export has learned this.
- The 2D preview MUST render the candidate drawing live, before queueing, and MUST show its measured bounding box in the active unit. This is the single highest-value confidence signal in the workspace: a scale error is instantly visible as a wrong number.
- The add action MUST be positioned after the preview in reading order, and MUST be disabled while the candidate drawing is invalid or still resolving.
- The queue MUST accept any number of drawings per part, MUST show a live count, and MUST NOT collapse or replace rows that share a part.
- Queue rows MUST be reorderable and individually removable, and MUST show a human-readable view label ("Top", "Normal to face"), not a raw vector.
- Selecting a queue row MUST render that drawing in the 2D preview.
- Every queue row MUST have an editable filename with the same override semantics as the Export bucket, and generated names MUST disambiguate by view.
- Face picking MUST highlight the candidate face using the existing measurement-highlight treatment.
- A non-planar face selection MUST be rejected with a plain-language reason rather than silently projecting from an averaged normal.
- The view control MUST agree with the 3D view cube: clicking a cube face and choosing that view from the control are the same action, and both MUST reflect the resolved document axis convention (Finding 1).

---

## Correctness Requirements

1:1 is the feature. These are gates, not preferences.

1. **Exact scale.** A queued 1:1 drawing MUST produce geometry whose real-world dimensions match the model to within 1×10⁻⁶ mm before any user-chosen scale factor. The projection pipeline is exact by construction — the orthographic projector is a rigid transform with a forced unit scale factor — so any error is a bug in *our* transform handling, not an inherent approximation.
2. **Unit gate.** Export MUST refuse when the document's `UnitDecision` blocks export, reusing the existing policy rather than a parallel one.
3. **Closure.** Cut Contours output MUST be stitched into closed wires. Contours that cannot be closed within tolerance MUST be reported per drawing as a warning naming the count, and MUST NOT be silently emitted as open paths.
4. **No duplicate geometry.** Coincident edges arising from a sharp edge lying on a silhouette MUST be deduplicated. A double-cut is a material defect.
5. **Curve fidelity.** Circular and elliptical arcs MUST be emitted as native arcs where the format supports them (DXF arcs and polyline bulges, SVG elliptical arcs) rather than tessellated. Where tessellation is unavoidable, chordal deviation MUST default to 0.01 mm — an order of magnitude below typical laser kerf — and MUST be user-adjustable.
6. **Degradation must be visible.** The projection engine has a known fallback that silently reduces certain curve types to a 15-point, degree-1 polyline with no tolerance control. When it fires, the drawing MUST be flagged with a warning identifying the affected curve count. Silent coarsening on a cut path is unacceptable.
7. **Handedness.** Per Finding 2, verified by a chiral-fixture regression test in all three formats.
8. **Post-write validation.** Every output MUST be reopened and checked before being reported as succeeded, mirroring the 3D pipeline's behaviour. A file failing validation MUST be deleted, not left on disk.
9. **Atomic writes.** Outputs MUST be written via the existing partial-then-rename mechanism so a reader never observes a partial file.
10. **Locale independence.** Coordinate formatting MUST be locale-independent. A German locale emitting `1,5` would corrupt every coordinate in every file. This MUST be covered by a test that runs under a comma-decimal locale.
11. **Optional fiducial.** The user MAY enable a 50 mm reference line on a dedicated non-cut layer. Cheap to build, and it catches every downstream unit misinterpretation at a glance.

---

## Architecture

The pipeline mirrors 3D Export's invariants, which exist for good reasons and should not be re-litigated: the reviewed plan is the contract, plans are unforgeable, the worker independently re-derives and refuses on mismatch, writes are atomic, and outputs are validated before being reported.

```
QML  src/app/qml/drawing/*.qml
  ↓  controller.drawingWorkspace   (one QObject* CONSTANT property, aliased once)
src/app/drawing/DrawingWorkspaceController   — queue state, live plan preview
  ↓  buildDrawingPlan()                       ↑ handleDrawing*() callbacks
src/core/drawing/DrawingPlan                — pure domain: validation, paths, fingerprint
  ↓  executeRequested(planJson, fingerprint)
ApplicationController::connectWorker()      — the single wiring seam
  ↓  protocol
loupe-worker  (separate process, OCCT, cancellable)
  ↓  DrawingProjector  — hidden-line removal → stitched contours in millimetres
  ↓  contour IR (format-neutral: layers → contours → line | arc | cubic)
  ↓  DxfWriter | SvgWriter | PdfWriter → post-write validation
```

### Decisions and rationale

**The projection engine is already shipping.** Hidden-line removal comes from OCCT's `TKHLR` toolkit, which is already installed *and already packaged in current releases* — it arrives transitively through the existing XCAF dependency chain. This feature adds **no new vcpkg dependency and no new runtime library.** `TKHLR`, `TKGeomBase`, `TKShHealing`, and `TKTopAlgo` should be listed explicitly on `loupe-core` rather than relied on transitively.

**Exact projection, not mesh projection.** Use the exact analytic algorithm rather than the faster mesh-based variant. The mesh path emits polylines only and permanently discards circles — unacceptable when the output drives a cutter. The exact path preserves analytic arcs, which is what makes requirement 5 achievable. The trade is speed; it runs in the existing out-of-process worker, which already provides progress and cancellation.

**Writers are hand-written and Qt-GUI-free.** OCCT writes none of these formats (its DXF support is a paid commercial add-on, and the old view-export path was removed in OCCT 8). The available third-party DXF libraries are GPL-2.0, incompatible with this project's Apache-2.0 licence. Qt's `QSvgGenerator` caps coordinates at six significant digits and takes an integer page size — two independent precision ceilings that cannot be configured away. `QPdfWriter` is genuinely good (nine fractional digits, physical page units, and it lives in Qt Gui rather than the uninstalled PrintSupport module) but requires a `QGuiApplication`, which the headless worker is not.

Writing all three by hand keeps the entire pipeline in `loupe-core`, callable from the worker, with full `double` precision and no new dependency. These are small, well-specified text formats; the total is roughly 800 lines with no external surface area.

**A format-neutral contour IR sits between projection and writing.** Layers → contours → primitives, in millimetres. This makes all three writers independently unit-testable with no OCCT and no Qt in the test target, which is where most of the correctness confidence will come from.

**DXF R12 first, R2000 when needed.** R12 has the widest compatibility with older CAM and inexpensive laser controllers, and is dramatically less structure to emit correctly. Its one real weakness is that it has no units header — "1 unit = 1 mm" is convention, which is exactly what the optional fiducial mitigates. R2000 adds an explicit millimetre units header plus native spline and ellipse entities; it is the right follow-on, not the right start.

**A separate plan type, not an extension of `ExportPlan`.** 2D output rows need a view, a content mode, and a scale; they do not need STEP unit rebasing or assembly-versus-local coordinates. Overloading the existing format enum would force 3D plan validation to reason about 2D concerns and vice versa. A parallel `DrawingPlan` in `src/core/drawing/` keeps both validators honest, at the cost of some deliberate structural duplication.

**Face normals need almost no new plumbing.** A picked face's outward normal already flows from the viewport to the app side in millimetre scene space, and is already used to derive a plane for the section tool. The one addition worth making is a small accessor that computes a whole-face normal with a planarity residual from mesh data already resident in the app, so a curved face can be rejected with a clear message instead of projected from an averaged normal. No worker, protocol, or cache change is required.

**Per-drawing view state needs a new home.** The section tool's plane state is document-global; drawings need per-drawing state. The Export controller's per-node maps are the precedent to follow.

---

## Delivery Plan

Sequenced so the riskiest unknown is retired first and each gate produces something verifiable.

### Gate A — Projection spike (de-risk before committing)

Prove the projection pipeline against the real corpus in the existing scratch target before building any UI.

- Project a known box from a standard view; assert the resulting bounding box is exact to 1×10⁻⁹.
- Measure runtime on the largest corpus assembly. This is the main schedule risk.
- Count how often the silent degree-1 curve fallback fires on real files.
- Confirm arcs survive as arcs.

**Exit:** exact-scale assertion passes, runtime is characterised, fallback frequency is known.

### Gate B — Contour extraction and the SVG writer

Planar extraction, stitching, deduplication, extent normalisation, the contour IR, and the first writer. SVG first because it is inspectable in a browser and exercises the exact-arc path.

**Exit:** a chiral fixture round-trips at correct size and handedness; contour closure is asserted; locale test passes.

### Gate C — DXF and PDF writers

DXF R12 with arcs and polyline bulges; PDF with its exact physical unit. Validate DXF in LibreCAD, Inkscape, and the actual shop software.

**Exit:** all three formats pass the chiral and scale fixtures; DXF opens cleanly in real cutting software.

### Gate D — Rotating 3D view cube

Replace the static cube with a real rotating one for both Inspect and Drawing, including document up-axis resolution (Finding 1), six clickable faces, per-face hover, six accessibility stops, and the animated camera transition.

Touches no OCCT and no projection code, so it **can run in parallel with Gates A–C.** Ships value to Inspect on its own even before the Drawing workspace exists.

**Exit:** all six views reachable by direct click; labels correct for both Z-up and Y-up documents; no measurable viewport frame-rate regression; accessibility stops verified; style gates clean.

### Gate E — Workspace and queue

The third workspace, component picker reuse, view selection including face-normal, content mode, scale, the live candidate preview, the multi-drawing queue, the plan and its fingerprint, and worker execution with progress and validation.

**Exit:** UX review against the handoff document; full test suite green including new style gates.

### Gate F — Release

Evidence record, cross-platform verification, `v0.2.0`.

Deferred to 3.2 and beyond, in likely order: custom-plane views, cross-section drawings, DXF R2000, combined-sheet output, flat-pattern unfold.

---

## Success Metrics

| Metric | Target |
| --- | --- |
| STEP open → first correct cut file | Under 2 minutes, versus 15–30 in a CAD drafting workflow |
| Scale correctness | 100%. Any wrong-scale output is a release blocker, not a bug report |
| Files needing rework before cutting | Under 5% across the corpus |
| Batch throughput | 12 drawings queued and exported without leaving the workspace |
| DXF acceptance | Opens without repair prompts in LibreCAD, Inkscape, and the shop's cutting software |
| Contour closure | Over 95% of Cut Contours drawings fully closed on the corpus; the remainder warned, never silently open |

---

## Testing

Following house practice — test-first inside each gate, focused tests per change, full matrix at gate closure.

**Unit, no OCCT or Qt required** (the highest-value tier, enabled by the contour IR):
- Each writer against hand-built contour sets: exact scale, handedness, arc encoding, layer structure, locale independence, degenerate input.
- Plan validation: every error code, determinism, fingerprint stability under reordering, fingerprint change on any option change.

**Geometry:**
- Exact 1:1 on a known box from all six standard views.
- Arc preservation for a plate with drilled holes.
- Contour closure on corpus parts.
- Face-normal projection against a deliberately non-axis-aligned face.
- Non-planar face rejection.
- Curve-fallback detection fires when expected.

**Controller:**
- Queue add/remove/reorder; snapshot semantics (orbiting after queueing must not alter a queued drawing); filename overrides; locked-while-exporting; result-row reconciliation.
- Multiple drawings per part specifically: three views of one part coexist as three rows; removing one leaves the others intact; generated names disambiguate by view and do not collide; an exact-duplicate add is reported as such.
- Candidate preview state: invalid or unresolved candidates disable the add action; selecting a queue row loads that drawing into the preview; a superseded in-flight preview never overwrites a newer one.

**View cube:**
- A named view resolves to the correct direction for a Z-up document and for a Y-up document.
- All six faces are hit-testable; the face under the cursor is the face reported.
- Six keyboard/assistive stops exist and each activates its own view.
- Cube orientation tracks the camera quaternion.

**Cross-format fixture:**
- One chiral part exported to all three formats, asserting size and handedness in each. This single test guards the two highest-severity risks.

**QML smoke:**
- Drive the workspace by object name, matching the existing Export picker smoke test.

**Mechanical gates:**
- New QML must pass the existing colour-literal and duration-literal gates with no exemptions.

---

## Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| Mirrored output from per-format handedness | **High** | Chiral fixture asserted in all three formats (Gate B). |
| Wrong standard view from the renderer's axis convention | **High** | Resolve against document up-axis; surface and allow override (Gate D, Finding 1). |
| Silent curve coarsening via the degree-1 fallback | **High** | Detect and warn per drawing; measure frequency in Gate A. |
| Open or duplicated contours reaching a cutter | **High** | Stitch, deduplicate, validate closure, warn on failure. |
| Projection too slow on large assemblies | Medium | Characterise in Gate A before committing; already async and cancellable in the worker; per-body projection if needed. |
| Live candidate preview too slow to feel live, or a fast preview silently differing from the exact output | Medium | Debounce and cancel superseded requests; label an approximate preview as approximate; always export via the exact path. |
| View cube adds frame-rate cost to a viewport that already composites several 3D surfaces | Low | Measure against the existing performance budget in Gate D; the cube is small and static-geometry; keep it hidden in presentation-only viewports. |
| DXF R12 unit ambiguity | Medium | Document the convention; ship the fiducial; add R2000 when a user needs an explicit units header. |
| SVG consumers disagreeing about scale | Medium | Emit physical dimensions and a matching viewBox; fiducial; verify in real target applications. |
| Scope creep toward a drafting tool | Medium | The non-goals section is normative. Each addition needs an explicit product decision. |
| Projection failures on poor-quality STEP | Medium | Catch and surface a real error; compare edge counts as a sanity check; pre-heal shapes. |

---

## Open Decisions

1. **Cross-section drawings.** Deliberately deferred, but it is arguably the *more* common real need for cutting a plate that matches an internal profile. Should it move into this release, or stay the first follow-on? Recommendation: stay deferred — it is a different algorithm and this release is already carrying two high-severity correctness risks.
2. **Fiducial default.** Off by default with an opt-in, or on by default for DXF R12 specifically where units are ambiguous? Recommendation: off by default, prominently offered.
3. **Content mode scope.** Should Technical View also offer hidden lines on a separate dashed layer? Cheap to add once the layer model exists, and it is what makes the output read as a real drawing. Recommendation: yes, as a sub-option of Technical View.
4. **Multiple views in one action.** Should there be a "queue all six standard views" convenience action? Common for documenting a part. Recommendation: defer until the single-drawing flow is validated.
5. **Scale presets.** Which ratios beyond 1:1 — 1:2, 1:5, 2:1, and arbitrary? Recommendation: 1:1, 1:2, 1:5, 2:1, plus arbitrary entry; anything non-1:1 must be visibly labelled in the queue and in the output filename.
6. **Local PRD location.** This is the first product requirements document kept in-repo; the existing PRD of record is external. Confirm `docs/product/` as its home, and whether the external document should now point here for this workspace.

---

## Acceptance Checklist

- [ ] A 1:1 drawing measures correct to within 1×10⁻⁶ mm in all three formats
- [ ] A chiral part is not mirrored in any format
- [ ] Standard view names match the document's own up-axis, with the inferred convention visible and overridable
- [ ] The view cube rotates with the part and all six faces are directly clickable, with no view reachable only by hidden menu
- [ ] The 2D preview renders the candidate drawing, with extents and warnings, before it can be queued
- [ ] One part can be queued at several angles as independent rows with non-colliding names
- [ ] Cut Contours output is closed, or warned with a count
- [ ] No duplicated coincident geometry
- [ ] Arcs are emitted as arcs where the format supports them
- [ ] Curve-fallback degradation is surfaced, never silent
- [ ] Export refuses on a blocking unit decision
- [ ] Outputs are atomic and post-write validated; failures leave no file
- [ ] Coordinate output is locale-independent
- [ ] DXF opens without repair prompts in LibreCAD, Inkscape, and the shop's cutting software
- [ ] Queued drawings are immune to later camera changes
- [ ] No literal colours, spacings, radii, or durations outside the `Theme` singleton
- [ ] Full CTest suite green on both platforms
- [ ] UX review passed against the UI refinement handoff
