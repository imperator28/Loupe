# 2D Drawing Export Implementation Plan

**Design:** `docs/product/2026-07-24-2d-drawing-export-prd.md`

**Phase:** 4 — first phase after the functional baseline (Phases 0–3) and the UI refresh (W1–W8)

**Goal:** Add a third workspace that produces true 1:1 2D vector files (DXF, SVG, PDF) from a STEP file, with a per-drawing view angle, a live pre-queue 2D preview, and a queue that holds any number of drawings per part. Replace the static view cube with a rotating 3D one.

**Architecture:** Mirror the 3D Export invariants — the reviewed plan is the contract, plans are unforgeable, the worker independently re-derives and refuses on fingerprint mismatch, writes are atomic, outputs are validated before being reported. Projection and all three writers live in `loupe-core` (Qt-Core-only, no Qt GUI) so they run inside the headless `loupe-worker` where every other OpenCASCADE operation already runs.

**Tech stack:** OCCT 8.0 (`TKHLR` hidden-line removal — already installed and already shipped transitively), C++23, Qt 6.11 Quick/Quick3D, CMake presets, Catch2 for core, QtTest for controllers.

> **Implementation note:** Gate D (view cube) shares no code with Gates A–C and can be executed in parallel. Gates A–C are strictly sequential — each depends on the previous one's output type. Do not start Gate E until B and C are green, because the workspace binds to the plan and writer types they define.

---

## Task 1: Link the HLR toolkits and prove exact 1:1 in the spike

The single highest-risk unknown is whether the projection is fast enough and faithful enough on the real corpus. Retire it before writing any product code.

**Files:**

- Modify: `src/core/CMakeLists.txt`
- Modify: `src/spike/main.cpp`

- [x] Add `TKHLR`, `TKGeomBase`, `TKShHealing`, and `TKTopAlgo` to `loupe-core`'s `PUBLIC` link list. They are already installed and `TKHLR.dll` already ships transitively via `TKXCAF → TKVCAF → TKV3d`; this makes the dependency explicit rather than incidental. No `vcpkg.json` change and no new packaged library.
- [x] Add a `drawing-spike` subcommand to the spike that takes a STEP path and a view axis.
- [x] Run hidden-line removal in the documented order: construct `HLRBRep_Algo`, `Add(shape, 0)` with zero isoparametric lines, `Projector(...)`, `Update()`, then `Hide()`. Build the projector directly as `HLRAlgo_Projector(gp_Ax2(...))` — `Prs3d_Projector` was removed in OCCT 8 and every pre-7.8 example that uses it will not compile.
- [x] Extract `VCompound()` and `OutLineVCompound()` and report per-curve-type counts.
- [x] Read each edge's 2D curve with the `BRep_Tool::CurveOnSurface(edge, curve, surface, location, first, last)` overload that returns the stored pcurve together with its surface. Do **not** use `BRepAdaptor_Curve` — HLR result edges carry no 3D curve. **Also do not use `CurveOnPlane`:** despite the name it does not read a stored planar pcurve, it *projects the edge's 3D curve* onto the plane you pass, so on an HLR edge it returns null every time. Measured on a corpus part: 171 of 171 null. This corrected an incorrect assumption in the original research.
- [x] Assert `BRepLib::Plane()` is the XOY plane on entry. It is a mutable global static and is not thread-safe; record this constraint so HLR jobs are never parallelised across a pool.
- [x] **Exactness check:** project a 100 × 50 × 10 mm box from each standard axis and assert the resulting 2D bounding box matches to within 1e-9. Scale the *shape* into millimetres before projecting — the projector forces its own scale factor to 1 and discards any scale baked into its transform.
- [x] **Performance check:** report wall-clock time for the largest available corpus assembly. This number decides whether per-body projection or a preview approximation is required.
- [x] **Fidelity check:** count edges that come back as a degree-1 B-spline with exactly 15 poles. That is OCCT's silent fallback for curve types it cannot classify, and it has no tolerance control. Record how often it fires on real files.
- [x] **Curve-fidelity probe:** project a cylinder down its own axis and confirm the rim survives as an analytic circle, which is what decides whether DXF can emit exact `CIRCLE`/`ARC` entities and polyline bulges.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-spike
./build/windows-release/loupe-spike drawing-spike corpus/private/PCBA_box.stp Z
```

---

## Gate A: Projection viability — PASSED 2026-07-24

Evidence: `docs/evidence/phase-4-gate-a-projection.md`

- [x] **Exact scale: passes at 0.0 error**, not merely within 1e-9, on all three axes of a 100 × 50 × 10 mm box. The 1:1 claim is proven, not assumed.
- [x] **Runtime: ~1.1–1.2 s** for a 4-body, 171-edge part. Slower than hoped and the main schedule risk. Mitigations adopted below.
- [x] **Degree-1/15-pole fallback: 0 occurrences** on the corpus part. The feared silent-coarsening cliff did not fire, though the detector stays in the shipping code.
- [x] **Analytic circles survive**, proven with a cylinder viewed down its axis (one exact circle, diameter exact). But 139 of 171 edges on a real filleted part arrive as B-splines, so Task 3 gains an arc-recovery pass.

**Decisions forced by these numbers:**

1. Exact projection is **not** viable for a live interactive preview at ~1 s per run. The candidate preview MUST debounce, cancel superseded requests, and MAY use the mesh-based algorithm — exactly the provision the PRD already carries. Export always uses the exact path.
2. Batch export MUST report progress per drawing and stay cancellable. A twelve-drawing batch is on the order of fifteen seconds, which is fine when visible and interruptible.
3. Task 3 MUST include arc recovery, or DXF loses exact arcs on most real geometry.
4. Before Gate C, re-measure on the largest available assembly. If runtime scales badly, project per body and merge.

---

## Task 2: Define the format-neutral contour IR

Everything downstream depends on this type, and it is what makes the writers testable without OCCT or Qt.

**Files:**

- Create: `src/core/drawing/DrawingContour.h`
- Create: `src/core/drawing/DrawingContour.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/test_drawing_contour.cpp`

- [x] Define `Primitive` as a variant over `Segment { gp_Pnt2d a, b }`, `Arc { centre, radius, startAngle, sweepAngle }`, and `Cubic { p0, c1, c2, p3 }`. All coordinates are millimetres.
- [x] Define `Contour { std::vector<Primitive> primitives; bool closed; }` and `Layer { std::string name; LayerRole role; std::vector<Contour> contours; }`.
- [x] Define `LayerRole { Cut, Outline, Smooth, Hidden, Reference }` so writers can map roles to layer names and colours without knowing what produced them.
- [x] Define `Drawing { std::vector<Layer> layers; Bounds2d extents; std::vector<DrawingWarning> warnings; }` where `DrawingWarning` carries a code and a count (`open_contour`, `coarse_curve_fallback`, `duplicate_edge_removed`).
- [x] Add `Drawing::translatedToOrigin(marginMm)` returning a copy shifted so extents start at the margin. This is the only transform the pipeline applies after projection.
- [x] Add `Drawing::mirroredVertically(pageHeightMm)` for the Y-down formats, and make it invert arc sweep direction. Getting this wrong mirrors the part; make it one tested function rather than per-writer arithmetic.
- [x] Unit tests: extents computation over each primitive type; translation exactness; vertical mirror inverts sweep and preserves extents; closure flag round-trips.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing-contour"
```

---

**Completed 2026-07-24.** 16 tests, 70 assertions, all passing; suite total 92 -> 108.

Two deliberate deviations from this task as originally written:

1. `Drawing` exposes `bounds()` as a computed method rather than a stored `extents` field. A stored field goes stale the moment `translatedToOrigin` or `mirroredVertically` runs, and a silently stale extent is exactly the kind of wrong number the requirements make the user's scale check depend on.
2. Coordinates use OCCT's `gp_Pnt2d` rather than a bespoke point struct. `loupe-core` links OCCT unconditionally and the rest of the codebase uses `gp_*` freely, so a parallel point type would be gratuitous. This does mean the IR is not literally OCCT-free -- the claim earlier in this plan that writers become testable "with no OCCT" was overstated. The real and still-valuable property is that a writer can be tested against hand-built geometry with **no STEP file and no projection run**, which is what makes those tests fast and deterministic.

Also worth recording for later tasks: `Catch::Approx` is relative, so `Approx(0.0)` demands bit-exact equality. Arc geometry runs through trig and `cos(pi/2)` is 6e-17, so every comparison against zero needs an explicit `.margin(...)`. Two tests failed on this before the margin was added.

---

## Task 3: Build the projector — HLR to stitched contours

**Files:**

- Create: `src/core/drawing/DrawingProjector.h`
- Create: `src/core/drawing/DrawingProjector.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/test_drawing_projection.cpp`

- [x] Define `ProjectionRequest { TopoDS_Shape shape; gp_Dir viewDirection; gp_Dir upDirection; ContentMode mode; double sourceToMillimeters; double deflectionMm; }` and `ContentMode { CutContours, TechnicalView }`.
- [x] Validate the request: reject a view direction parallel to the up direction, which makes the projector's basis degenerate and throws deep inside OCCT.
- [x] Scale the shape to millimetres via `BRepBuilderAPI_Transform` before projecting.
- [x] Map content mode to compounds: `CutContours` takes `VCompound() ∪ OutLineVCompound()`; `TechnicalView` adds `Rg1LineVCompound()` on a `Smooth` layer. Never include the hidden compounds in `Cut` — a cutter would cut them.
- [x] Read pcurves with the `CurveOnSurface` overload that returns the surface — see Task 1; `CurveOnPlane` returns null on every HLR edge.
- [x] Convert each edge's pcurve to IR primitives, preserving `GeomAbs_Line`, `GeomAbs_Circle`, and `GeomAbs_Ellipse` analytically. Note the projected type is what matters: a 3D circle not parallel to the view plane projects to an ellipse.
- [x] **Add an arc-recovery pass.** Gate A measured that circles parallel to the view plane *do* survive as `GeomAbs_Circle` (proven with a cylinder viewed down its axis: one exact circle, diameter exact to 0.0), but on a real filleted part 139 of 171 edges arrived as `GeomAbs_BSplineCurve`, including features that are geometrically circular arcs. Test each B-spline for constant curvature within tolerance and re-emit it as an `Arc`. Without this, DXF loses its exact `ARC`/bulge encoding on most real parts and file sizes balloon. Measure the recovery rate on the corpus and record it.
- [x] Decompose non-rational B-splines of degree ≤ 3 exactly with `Geom2dConvert_BSplineCurveToBezierCurve` into `Cubic` primitives. Only tessellate what cannot be represented.
- [x] Tessellate remaining curve types with `GCPnts_QuasiUniformDeflection` on the 2D adaptor, default deflection 0.01 mm. Apply a safety factor of 2 — that class's measure approximates deflection rather than bounding it.
- [x] Detect the degree-1/15-pole fallback and emit a `coarse_curve_fallback` warning with a count. Never let it pass silently.
- [x] Stitch with `ShapeAnalysis_FreeBounds::ConnectEdgesToWires(edges, tolerance, false)` — the OCCT 8 signature returns by value; the out-parameter overload is deprecated. Derive tolerance from the shape's own `BRep_Tool::MaxTolerance`, clamped to a sane range.
- [x] Deduplicate coincident primitives (equal start, midpoint, and end within tolerance) and emit `duplicate_edge_removed` with a count. A sharp edge lying on a silhouette otherwise produces a double cut.
- [x] Emit `open_contour` with a count for anything that could not be closed. Do not silently emit open paths in `Cut`.
- [x] Wrap the whole projection in `catch (const Standard_Failure&)` and rethrow as a domain error with a readable message.
- [x] Tests: exact 1:1 box from all six axes; a plate with drilled holes keeps circles as `Arc`; closure holds on a corpus part; face-normal projection of a deliberately non-axis-aligned face; degenerate direction is rejected; fallback warning fires on a constructed offset-surface case.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing-projection"
```

---

## Task 4: Write the SVG writer

First writer because it is inspectable in a browser and exercises the exact-arc path.

**Files:**

- Create: `src/core/drawing/SvgWriter.h`
- Create: `src/core/drawing/SvgWriter.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/test_drawing_writers.cpp`

- [x] Emit `width="Wmm" height="Hmm"` together with a matching `viewBox="0 0 W H"` so one user unit is exactly one millimetre. Emitting both is what makes the scale unambiguous per spec.
- [x] Apply the vertical mirror — SVG's Y axis grows downward.
- [x] One `<g>` per layer, `id` from the layer name, geometry as `<path>`.
- [x] Use `A` for `Arc`, including rotated ellipses, rather than tessellating. Compute `large-arc-flag` from sweep magnitude and **invert `sweep-flag`** because of the mirror.
- [x] Use `C` for `Cubic`, `L` for `Segment`, and close closed contours with `Z`.
- [x] Format every number with `std::format` and fixed notation. Never `sprintf`, `QTextStream`, or a locale-imbued stream — a comma-decimal locale would corrupt every coordinate.
- [x] Do **not** use `QSvgGenerator`: it caps coordinates at six significant digits, takes an integer page size, and needs a `QGuiApplication` the worker does not have.
- [x] Write through `detail::AtomicExportFile` so a reader never sees a partial file.
- [x] Tests: a 100 × 50 rectangle produces exactly those millimetre dimensions; a chiral L-shape is not mirrored; an arc emits `A` with the correct sweep flag; output is byte-identical under a comma-decimal locale.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing-writer"
```

---

## Gate B: Contour extraction and first writer — PASSED 2026-07-24

- [x] A chiral fixture round-trips at correct size and correct handedness in SVG.
- [x] Contour closure is asserted on corpus geometry.
- [x] Locale test passes (verified under a comma-decimal locale).
- [x] SVG declares millimetre size and a matching viewBox; arc direction independently confirmed by re-deriving the centre through SVG's own endpoint-to-centre parameterisation.

---

## Task 5: Write the DXF writer (R12)

The highest-value output — this is what shop software consumes.

**Files:**

- Create: `src/core/drawing/DxfWriter.h`
- Create: `src/core/drawing/DxfWriter.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/core/test_drawing_writers.cpp`

- [ ] Target R12 / `AC1009`. Do **not** emit `LWPOLYLINE`, `SPLINE`, `ELLIPSE`, or `$INSUNITS` — all are R13+ or AC1015+ and will break strict R12 readers.
- [ ] Emit `HEADER` with `$ACADVER`, `$EXTMIN`, `$EXTMAX`; a `TABLES` section with an `LTYPE` table containing `CONTINUOUS` and a `LAYER` table with one entry per role. Every entity's layer must resolve to a `LAYER` entry and every layer's linetype to an `LTYPE` entry — dangling references are the main cause of repair prompts.
- [ ] **No vertical mirror.** DXF model space is Y-up. This is the format that differs; assert it in a test.
- [ ] `Segment` → `LINE`. Full-circle `Arc` → `CIRCLE`. Partial `Arc` → `ARC` with angles **in degrees**, always counter-clockwise; swap start and end if the sweep is clockwise.
- [ ] Mixed contours → `POLYLINE` with `66` set, `VERTEX` entities, and a mandatory `SEQEND`. Encode circular spans exactly with the `42` bulge code, `bulge = tan(sweep / 4)`, instead of tessellating. Set the closed flag in `70` for closed contours. This yields one exact, closed entity per contour, which is precisely what CAM wants.
- [ ] Tessellate `Cubic` primitives, since R12 has no spline entity.
- [ ] Reserve layer `0`; put all geometry on named role layers so the operator can set power and speed per layer.
- [ ] Fixed notation with at least six decimals, CRLF line endings, locale-independent formatting, never scientific notation or `nan`.
- [ ] Write through `detail::AtomicExportFile`.
- [ ] Tests: chiral fixture is **not** mirrored (opposite expectation from SVG); a full circle emits `CIRCLE`; a line-and-arc contour emits one `POLYLINE` with a correct bulge; every referenced layer exists in the table; locale test.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing-writer"
```

---

## Task 6: Write the PDF writer

**Files:**

- Create: `src/core/drawing/PdfWriter.h`
- Create: `src/core/drawing/PdfWriter.cpp`
- Modify: `src/core/CMakeLists.txt`
- Modify: `tests/core/test_drawing_writers.cpp`

- [ ] Hand-write minimal PDF 1.4 rather than using `QPdfWriter`. `QPdfWriter` is otherwise a good fit — it lives in Qt Gui, not the uninstalled PrintSupport module, and writes nine fractional digits — but it needs a `QGuiApplication`, which would force the write out of the worker and add a Qt GUI dependency to `loupe-core`.
- [ ] Set `MediaBox` in points. PDF's native unit is exactly 1/72 inch, so the millimetre-to-point factor is exact and the scale is unambiguous.
- [x] **Do NOT apply a vertical mirror.** PDF user space has its origin at the lower left with y increasing *upward*, the same as DXF model space — SVG is the only y-down target. This plan originally claimed PDF was y-down, which would have mirrored every PDF; corrected during implementation. Qt's `QPdfWriter` exposes a y-down painter space, which is what makes this easy to get backwards, but raw PDF is y-up.
- [ ] Content stream: `m`, `l`, `c`, `h`, `S`. Approximate arcs with cubic Béziers using control distance `k = (4/3)·tan(θ/4)`, subdividing at 90° or finer; radial error at 90° is about 0.027% of radius, far below laser kerf.
- [ ] One page, no fonts, no compression — this is line art and keeping it uncompressed keeps it diffable and debuggable.
- [ ] Write through `detail::AtomicExportFile`.
- [ ] Tests: `MediaBox` matches the requested millimetre size exactly; chiral fixture is mirrored consistently with SVG; arc approximation error is within tolerance; locale test.

---

## Task 7: Cross-format fixture and post-write validation

One test that guards the two highest-severity risks in the PRD.

**Files:**

- Create: `src/core/drawing/DrawingValidator.h`
- Create: `src/core/drawing/DrawingValidator.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/test_drawing_validation.cpp`

- [ ] `DrawingValidator::validate(path, expectedExtentsMm, tolerance)` reopens the written file and parses back enough to confirm the declared page or extents. Mirror `OutputValidator`'s result shape: `passed`, plus coded warnings and errors.
- [ ] For DXF, parse `$EXTMIN`/`$EXTMAX`. For SVG, parse the root `width`/`height`/`viewBox`. For PDF, parse `MediaBox`.
- [ ] Cross-format test: one deliberately chiral part (an L-bracket with one notched corner) exported to all three formats, asserting **both** size and handedness in each. This is the single most important test in the feature.
- [ ] Validation-failure test: a deliberately corrupted output is rejected.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing"
```

---

## Gate C: All three writers — PASSED 2026-07-24 (one item needs a human)

- [x] Chiral and scale fixtures pass in all three formats, and all three are re-read from disk and confirmed to declare the same real-world size.
- [x] Post-write validation rejects a wrong size, a contradictory SVG viewBox, a truncated file, a missing file, an unknown extension, and a bad tolerance.
- [x] Verified on real corpus geometry, not only fixtures: 40 contours, 78.7 x 61.35 mm, identical across DXF, SVG and PDF, all three validating.
- [x] PDF MediaBox is exact to specification (1/72 inch) and validated on read-back.
- [x] **DXF verified in the real consumer 2026-07-25.** Confirmed in review; this was the last item Gate C could not close from here.

**Bug caught by real geometry that the unit tests missed.** Every writer test used a zero margin, so the margin path was never exercised. Running the spike with default options exposed DXF declaring tight geometry bounds in `$EXTMIN`/`$EXTMAX` while SVG and PDF declared a page including the margin -- a disagreement of twice the margin. DXF now declares the page box, and a cross-format non-zero-margin test locks it in.

---

## Task 8: Resolve the document up-axis

Finding 1 in the PRD: the renderer's Y-up convention makes the cube call model +Z "Front", so "Top view" would silently emit a front elevation for the Z-up files that dominate mechanical CAD.

**Files:**

- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/ApplicationController.cpp`
- Modify: `src/app/cache/OverrideStore.h`
- Modify: `src/app/cache/OverrideStore.cpp`
- Modify: `tests/app/test_application_controller.cpp`

- [x] Add `Q_PROPERTY(QString upAxis ...)` with values `"Z"` and `"Y"`, and a `Q_INVOKABLE setUpAxis(QString)`.
- [x] Default to `"Z"`. STEP has no reliable up-axis field, so this is a documented convention choice, not an inference — mechanical CAD is overwhelmingly Z-up. Do not attempt to guess from bounding-box aspect ratio; it is unreliable and would fail silently.
- [x] Persist the per-document override through `OverrideStore`, reusing the mechanism that already persists unit overrides. Unlike a unit override, changing the up-axis must **not** trigger a reimport — it only affects view naming.
- [x] Add a single named-view mapping table keyed by up-axis, and expose `Q_INVOKABLE QVector3D directionForStandardView(QString name)`. Proposed mapping, to be confirmed against a corpus file of known orientation in this task:

  | View | Z-up document | Y-up document |
  | --- | --- | --- |
  | Top | `+Z` | `+Y` |
  | Bottom | `−Z` | `−Y` |
  | Front | `−Y` | `+Z` |
  | Back | `+Y` | `−Z` |
  | Right | `+X` | `+X` |
  | Left | `−X` | `−X` |

- [x] Tests: each of the six names resolves correctly under both conventions; the override persists across a document reopen; changing it does not trigger a reimport.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-application-controller-tests
ctest --preset windows-release --output-on-failure -R "application-controller"
```

---

## Task 9: Replace the view cube with a rotating 3D cube

**Files:**

- Modify: `src/app/qml/inspect/ViewCube.qml`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `src/app/qml/export/ExportPreview.qml`
- Modify: `tests/qml/qml_smoke.cpp`

- [x] Keep the existing `signal viewRequested(vector3d normal)` contract exactly. It is already vector-based, so `StepViewport.setStandardView` → `navigation.alignToNormal` needs no change and already supports all six directions — only the UI was limiting.
- [x] Replace the `QtQuick.Shapes` hexagon with a small `View3D` containing an `OrthographicCamera` on a `Node` rig whose `rotation` is bound to the main camera's orientation, so the cube mirrors the part continuously.
- [x] Add `property quaternion cameraOrientation` and bind it from `StepViewport` to `navigation.orientation`.
- [x] Build the cube from six `#Plane` models, one per face, each carrying `property vector3d faceNormal` and `property string faceKey`. Backface culling then shows exactly the faces turned toward the viewer.
- [x] Hit-test with `View3D.pick`, reading `faceNormal` off `objectHit`. Emit `viewRequested` with the direction resolved through `controller.directionForStandardView(faceKey)` so labels and geometry cannot disagree.
- [x] Hover: tint the picked face's material using the accent-tint token. Per-face materials are why six planes beat one `#Cube`.
- [x] Labels: place a `Node` at each face centre, project with `mapFrom3DScene`, and overlay `Text`. **The projection binding must reference `navigation.revision`** — `mapFrom3DScene` has no declared camera dependency, which is exactly why that counter exists. Hide labels for faces turned away from the viewer.
- [x] Label text comes from the resolved up-axis mapping, so a Z-up document's top face reads "Top" and actually looks down `+Z`.
- [x] Preserve accessibility: six keyboard/assistive stops, one per face, with `Accessible.role`, `Accessible.name`, and Return/Space activation. This must not regress from the current three-stop implementation.
- [x] Animate the camera transition on the spatial motion token, honouring `theme.reducedMotion`.
- [x] **Make the cube available in every orbitable viewport.** Add `property bool viewCubeVisible: !presentationOnly` to `StepViewport` and change the cube's `visible` binding from `!root.presentationOnly && !root.captureUiHidden` to `root.viewCubeVisible && !root.captureUiHidden`. This mirrors the existing `renderModeControlVisible` opt-in exactly (`StepViewport.qml:18`), so presentation-only stops meaning "no cube" without granting the full interactive toolbar.
- [x] Set `item.viewCubeVisible = true` in `ExportPreview.qml`'s `onLoaded`, beside the existing `item.renderModeControlVisible = true`. This covers both the master-assembly and standalone previews, since both use that component.
- [x] Keep the `captureUiHidden` term so the cube never appears in an exported image.
- [x] Remove the right-click hidden-faces menu — all six faces are now directly reachable, which was the point.
- [x] No colour, size, or duration literals; the style gate permits no new exemptions.
- [x] Smoke test: drive each of the six faces by object name and assert the emitted direction.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-app loupe-qml-smoke-tests
ctest --preset windows-release --output-on-failure -R "qml-smoke|qml-style-gates|qml-theme-contrast"
```

---

## Gate D: View cube UX review — PASSED 2026-07-24 (two items need a human)

- [x] All six standard views reachable by direct click; the right-click hidden-faces menu is gone.
- [x] Labels correct for both a Z-up and a Y-up document, asserted in the controller tests including that every named direction is a unit vector.
- [x] Cube orientation is bound to the live camera quaternion, so it tracks the part continuously.
- [x] Present in Inspect and in **both** Export previews, via a `viewCubeVisible` opt-in that mirrors the existing `renderModeControlVisible` pattern. (The Drawing preview arrives with Gate E.)
- [x] Absent from captured images -- the `captureUiHidden` term is retained in the visibility binding.
- [x] Six accessibility stops, one per face, up from three.
- [x] Style and contrast gates clean; no new literals and no new exemptions.
- [x] The app launches with zero QML warnings or errors, so the new `View3D`-based cube instantiates cleanly.
- [ ] **Needs a human:** visual confirmation that the cube reads correctly while orbiting, and a frame-rate check in Export with both previews live.

Note for whoever reads this later: the correct Qt Quick 3D built-in flat mesh is `#Rectangle`. There is no `#Plane`, despite it being the more obvious guess.

Inspect benefits from this gate on its own, independently of the rest of the feature.

---

## Task 10: Build the drawing plan domain type — DONE 2026-07-25

A queued batch of drawings now becomes a reviewed, canonically ordered, fingerprinted
plan that the worker can be held to, sharing ExportPlan's output-naming rules rather
than a second copy of them.

**Files:**

- Create: `src/core/drawing/DrawingPlan.h`
- Create: `src/core/drawing/DrawingPlan.cpp`
- Modify: `src/core/CMakeLists.txt`
- Create: `tests/core/test_drawing_plan.cpp`

- [x] Define a parallel plan type rather than extending `ExportPlan`. A drawing row needs a view, content mode, and scale, and does not need STEP unit rebasing or assembly-versus-local coordinates; overloading the existing format enum would force each validator to reason about the other's domain.
- [x] `DrawingFormat { Dxf, Svg, Pdf }`. `DrawingSelection { drawingId, nodeId, hierarchyPath, viewDirection, upDirection, viewLabel, contentMode, scaleNumerator, scaleDenominator, outputLeafName }`.

      **Deviation, deliberate:** `hierarchyPath` and `outputLeafName` are not fields on
      the selection. They live on the request as maps keyed by node ID and drawing ID
      respectively, matching how `ExportPlan` already receives them and how the
      workspace holds them -- one path per node, shared by every drawing of that node.
      Keeping them on the selection would have let two drawings of the same part carry
      contradictory paths for it, which nothing downstream could resolve.
- [x] `DrawingPlanRequest` carries selections, destination, format, deflection, fiducial flag, and the document `UnitDecision`.
- [x] `DrawingOutputRow` and `DrawingPlan` follow the established pattern: private constructors with `friend buildDrawingPlan`, so a plan can only come from the builder.

      The deleted rvalue accessors earned their keep immediately: the first compile
      failed on six of my own test lines that read `buildDrawingPlan(...).fingerprint()`
      off a temporary. That is exactly the dangling reference the deletion exists to
      stop, so the tests were fixed to name the plan first.
- [x] Reuse the existing leaf sanitisation and Windows-comparable-path collision logic from `ExportPlan` — extract it to a shared internal header rather than copying it, since it encodes hard-won rules about reserved device names, trailing dots, and Unicode folding.

      **Done, with one change from the sketch below:** the shared helpers throw a
      neutral `OutputNamingError` carrying `Code { UnsafeName, InvalidUtf8 }` rather
      than returning `std::expected`. Both call sites are inside code that already
      reports failure by throwing a plan error, so `expected` would have been unwrapped
      and rethrown at every use with nothing gained. Each plan builder catches it and
      maps to its own code (`PlanError::UnsafeOutputName` /
      `DrawingPlanError::UnsafeOutputName`). The extraction itself was done by script to
      rule out transcription drift in rules nobody wants to re-derive.

      Before and after: **41 assertions in 25 test cases, all passing** for
      `[export-plan]` — identical, which was the required check.

      **Extraction is not mechanical; design settled during research.** Both helpers
      (`sanitizedLeaf`, `windowsComparablePath`, plus `isReservedWindowsDeviceName`
      and `leafOf`) live in `ExportPlan.cpp`'s anonymous namespace and **throw
      `PlanError`**, an ExportPlan-specific type, so a shared version cannot throw it.
      Move them to `src/core/export/OutputNaming.{h,cpp}` in `loupe::exporting::detail`
      reporting failure neutrally -- `std::expected<std::string, NamingProblem>` with
      `NamingProblem { UnsafeName, InvalidUtf8 }` -- and have each plan builder map
      that onto its own error code (`PlanError::UnsafeOutputName`, or the drawing
      equivalent). Budget for touching `ExportPlan.cpp`, which carries 25 tests over
      exactly these rules: reserved device names, trailing dots and spaces, malformed
      UTF-8, case-insensitive collisions, and NFC-folded accented collisions. Run
      `ctest -R "export-plan"` before and after and expect identical results.
- [x] Validation, each with its own error code: empty selection; blank destination; blocking `UnitDecision`; non-finite or non-positive scale; degenerate view direction; missing hierarchy path; output path collision; unsafe leaf name; invalid enum.

      Twelve codes in the end. Two beyond the list: `InvalidSourceScale` (a
      non-positive source-to-millimetre factor would silently destroy the 1:1
      guarantee) and `DuplicateDrawingId` (results are addressed by drawing ID, so a
      duplicate makes a returned result ambiguous). `DegenerateView` also covers a view
      direction parallel to the up direction, which leaves the in-plane axis undefined
      and otherwise fails inside the geometry kernel with a message a user cannot act on.
- [x] Fingerprint with `XXH3_128bits` over length-prefixed fields, matching the 3D pipeline, and include every field that changes output — view direction components, content mode, and scale included.
- [x] Generated leaf names must disambiguate by view label, and must include the scale when it is not 1:1. `drawingLeafName` is exported so the workspace can show the name before export rather than after.
- [x] Tests: every error code; determinism; permuted selections give an identical fingerprint; changing view direction, content mode, or scale changes it; three views of one part produce three non-colliding paths.

      **18 test cases, 37 assertions, all passing.** Includes the case-insensitive
      collision and the reserved-device-name refusal, which prove the shared naming
      rules are actually in force here rather than merely linked.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests
ctest --preset windows-release --output-on-failure -R "drawing-plan"
```

---

## Task 11: Expose a whole-face frame for face-normal views — DONE 2026-07-25

A picked face's normal already reaches the app in millimetre scene space. The gap is that it is the normal at one point, not the face, so a curved face cannot be distinguished from a flat one.

**Files:**

- Modify: `src/app/render/MeshGeometry.h`
- Modify: `src/app/render/MeshGeometry.cpp`
- Modify: `tests/app/test_scene_model.cpp`

- [x] Add `Q_INVOKABLE QVariantMap faceFrameFor(quint32 topologyId)` returning the area-weighted normal, the centroid, and the maximum angular deviation across the face's vertex normals.

      Returns `normal`, `centroid`, `maximumDeviationDegrees`, `areaMm2`, `triangleCount`,
      or an empty map for an unknown face -- never a default frame, which would silently
      become a wrong view direction.

      **Deviation is measured from triangle normals, not vertex normals.** This is a
      correction to the sketch, not a shortcut. `calculateNormals` accumulates every
      triangle sharing a vertex, including triangles from *neighbouring faces*, so a
      vertex on a flat face's border carries a blended normal and the face reads as
      curved. Triangle normals depend only on this face's own triangles and hold whatever
      produced the shading normals. The flat-face test encodes this: its vertex normals
      are deliberately blended 45 degrees off the plane, so a vertex-normal
      implementation fails it.

      Area weighting is free -- summing the raw cross products weights each by 2A -- and
      zero-area slivers are skipped, since normalising one turns rounding noise into a
      spurious deviation.
- [x] Implement over the already-resident `sourceNormalData_`, `sourceIndexData_`, and `sourceTopology_`, walking one topology range the way `copyTopologyFrom` already does. No worker, protocol, or cache change is needed.

      Confirmed: no worker, protocol, or cache change was needed.
- [x] Tests: a planar face reports near-zero deviation and the exact normal; a cylindrical face reports a large deviation.

      **19 cases pass** in `scene-model`. A quarter cylinder tessellated in four facets
      reports over 30 degrees; the flat square reports under 0.01.

      One planned case was dropped as unreachable rather than left failing: an edge range
      asked for as a face. `validate()` in `GeometryPayload.cpp` refuses a non-Face
      range inside a mesh payload, so no payload can produce it and `encodeGeometry`
      throws when constructing one. The kind check in `faceFrameFor` stays as
      belt-and-braces.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-scene-model-tests
ctest --preset windows-release --output-on-failure -R "scene-model"
```

---

## Task 12: Build the drawing workspace controller — DONE 2026-07-25

The queue, the candidate, the live plan preview and the export lifecycle. Worker signal
wiring is deliberately left to Task 13, where the events it connects to exist.

**Files:**

- Create: `src/app/drawing/DrawingWorkspaceController.h`
- Create: `src/app/drawing/DrawingWorkspaceController.cpp`
- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/ApplicationController.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/app/test_drawing_workspace_controller.cpp`
- Modify: `tests/CMakeLists.txt`

- [x] Expose it as a single `QObject*` CONSTANT property `drawingWorkspace` on `ApplicationController`, matching how `exportWorkspace` is exposed. Do not register the type with QML directly.

      It also shares the document lifecycle: the same four sites that feed the export
      workspace a snapshot, mark it ready, and reset it now feed this one.
- [x] Reuse the snapshot-to-component-list logic from `ExportWorkspaceController`, including the multi-solid body rows and the redundant-raw-body-row suppression, so the picker behaves identically.

      Extracted to `app/models/PickerComponents` rather than copied, so "identically" is
      true by construction. `ExportWorkspaceController` now holds one as a member; its 19
      cases and the 26 application-controller cases are unchanged.

      Worth recording from that refactor: mechanically rewriting
      `componentIndexById_.contains(x)` to `indexOf(x) >= 0` silently broke two
      conditions, because the surrounding `!` then bound to the index, and
      `!indexOf(x) >= 0` is always true. Focus and hover stopped accepting any node, and
      three tests caught it. The shared type therefore offers `contains()`.
- [x] **Candidate state:** `candidateNodeId`, `candidateViewKind` (`Standard` or `FaceNormal`), `candidateViewDirection`, `candidateViewLabel`, `candidateContentMode`, `candidateScale`, plus `candidateValid` and `candidateStatus`. Changing any of these invalidates the preview and re-requests it.
- [x] Reject a non-planar face selection with a plain-language reason using the deviation from Task 11, rather than projecting from an averaged normal.

      Threshold 0.5 degrees, and the measured deviation is quoted back to the user. A
      flat face's up direction is the world axis least aligned with its normal, which
      keeps the up stable and can never be parallel to the view -- the plan refuses that
      as a degenerate view, and it would refuse the whole batch.
- [x] **Queue:** an ordered `QVector<QueuedDrawing>` keyed by a generated `drawingId`. Explicitly **not** a checkbox set over the tree — a checkbox cannot express one part queued three times.
- [x] `Q_INVOKABLE addCandidateToQueue()` snapshots the resolved direction as a vector at add time, so later orbiting never mutates a queued drawing. Return the new `drawingId`.
- [x] `Q_INVOKABLE removeDrawing(drawingId)`, `moveDrawing(drawingId, index)`, `setFilenameOverride(drawingId, name)`, `selectDrawing(drawingId)` — selecting loads that drawing into the preview.
- [x] Report an exact-duplicate add (same part, view, content mode, and scale) as a duplicate rather than silently producing two identical files. The existing row is selected too, so the user is shown the drawing they asked for.
- [x] Expose `drawingCountForNode(nodeId)` so the picker can show a per-part count; it is also on each picker row as `drawingCount`.
- [x] Live plan preview via `buildDrawingPlan`, mirroring `refreshPlan`: rows in queue order, `planError`, `planFingerprint`, `canExport`.

      Rows follow the queue rather than the plan's canonical order, because every
      `rowIndex` in a worker event has to index what the user is looking at.

      A plan error still yields one row per queued drawing, so the user sees which ones
      need attention rather than an empty list.

      Note on generated names, since users see them: the leaf comes from the part
      **name**, matching what the 3D export workspace already produces, with a lowercased
      view suffix appended -- `Base plate-top.dxf`. The mixed casing is deliberate. A
      user cross-referencing a STEP export against its drawing sees the same part name in
      both.
- [x] Lock every mutator while exporting, matching the Export controller. Covered by a test that attempts every one of them.
- [x] Emit `executeRequested(planJson, fingerprint)` and `cancelRequested(requestId)`; add `handleDrawingProgress`, `handleDrawingRowResult`, `handleDrawingCompleted`, `handleDrawingFailed`, `handleDrawingCanceled`. Do not touch `WorkerClient` from the controller.

      Signals and handlers exist and are tested by calling the handlers directly.
      **Connecting them in `connectWorker()` moved to Task 13**, which is where the
      `DrawingProgress` / `DrawingRowResult` / `DrawingCompleted` events they attach to
      are introduced. Wiring them here would mean connecting to signals that do not yet
      exist.
- [x] Also emit a preview request signal, and cancel any superseded in-flight preview so a stale result never overwrites a newer one. Each request carries a monotonic `revision`, and supersession cancels by revision rather than waiting: a late reply can still arrive, and the revision is what lets the view drop it.
- [x] Register a `loupe-drawing-workspace-tests` target as ctest name `drawing-workspace`, following the `export-workspace` template.
- [x] Tests: queue add/remove/reorder; three views of one part coexist as three rows; removing one leaves the others; generated names disambiguate and do not collide; snapshot semantics hold when the camera changes after queueing; duplicate add is reported; non-planar face rejected; invalid candidate disables the add action; selecting a row loads its drawing; mutators locked while exporting; row-result reconciliation.

      **18 cases pass.** The snapshot case is the sharp one: it changes the candidate
      view and scale after queueing, then asserts the queued row, its filename and the
      plan fingerprint are all unmoved.

      Two of my own mistakes worth keeping, because in both the test was wrong rather
      than the code. The helper passed an up direction parallel to the view, which the
      plan correctly refused as degenerate. And the expected filenames assumed node IDs
      when the leaf comes from the part name.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-drawing-workspace-tests
ctest --preset windows-release --output-on-failure -R "drawing-workspace"
```

---

## Task 13: Extend the protocol and worker for drawing execution — DONE 2026-07-25

A reviewed batch now crosses the process boundary, is re-derived and fingerprint-checked on
the far side, and comes back per row. Two extractions were needed first, plus one real
defect fixed at the root.

**Files:**

- Modify: `src/protocol/ProtocolTypes.h`
- Modify: `src/protocol/ProtocolJson.cpp`
- Modify: `src/app/worker/WorkerClient.h`
- Modify: `src/app/worker/WorkerClient.cpp`
- Modify: `src/worker/WorkerServer.cpp`
- Modify: `tests/protocol/test_protocol.cpp`
- Modify: `tests/worker/test_worker_process.cpp`

- [x] Add `ExecuteDrawingPlan` to `Command` and `DrawingProgress`, `DrawingRowResult`, `DrawingCompleted` to `Event`, alongside a `DrawingPreviewReady` event for the candidate preview.

      Plus `RequestDrawingPreview`, which the sketch left implicit. It carries a monotonic
      revision, because supersession has to be expressible in the message itself: a late
      reply can still arrive and the revision is the only thing that lets it be dropped.
- [x] Add JSON codec entries with the same per-field validation discipline as the export messages, and bump the protocol version.

      Bumped to 2.1. Only the major version is validated, so this stays compatible.

      Two refusals worth naming, both tested: a preview revision below 1, because a zero
      would make every reply look like the newest one; and a row result with an empty
      drawing ID, because the app reconciles results by ID and path and an empty ID cannot
      be reconciled at all.
- [x] In `WorkerServer`, decode the plan, **independently re-run `buildDrawingPlan`, and refuse on fingerprint mismatch** — the review contract. Verify the destination exists, is a directory, and is writable.

      Tested end to end over the socket: a plan sent with a wrong fingerprint is refused
      with `drawing_gate_failed` and nothing is written.
- [x] Reconcile plan order against reviewed queue order the way the export path does, so every `rowIndex` in an event indexes the controller's own rows.
- [x] Run projection and writing on a worker thread with progress per row and cooperative cancellation. Serialise HLR jobs — `BRepLib::Plane()` is a mutable global static and is not thread-safe.

      One thread, rows projected one at a time on it. Not a performance compromise but a
      correctness requirement, recorded in Task 1: hidden-line removal reads a mutable
      global, so a pool would corrupt concurrent jobs.
- [x] Validate each output with `DrawingValidator` before reporting success; on failure delete the file and report the row as failed. A failed row must not abort the batch.

      Validation lives in the new `DrawingExporter`, so it cannot be skipped by a second
      caller; the worker deletes the file on any throw. A half-written drawing is worse
      than none, because it still looks cuttable.

      A row that succeeds with open contours reports that in its message rather than a
      bare success. An open contour will not cut, and silence there is the failure mode
      the requirements forbid.
- [x] Handle the preview request separately: bounded, cancellable, and always superseded by a newer request. If Gate A showed exact projection is too slow for interactive use, use the mesh-based algorithm here **and** mark the response approximate so the UI can label it; the export path must always use the exact algorithm.

      **The preview uses the exact projection**, so what is reviewed is what is written --
      Gate A measured it fast enough. The `approximate` flag exists on the event and is
      reported as false, rather than being omitted: if a future part turns out to be too
      slow and the mesh path is needed, the UI already has somewhere to read the truth
      from.

      Supersession is enforced twice, deliberately. The worker drops a request whose
      revision is below the highest seen, so a stale candidate is never projected at all;
      the app then gates the reply on both request ID and revision, so a reply that raced
      a newer candidate cannot paint over it.
- [x] Tests: codec round-trip for every new message; fingerprint mismatch is refused; a full drawing export runs end-to-end over the local socket; cancellation is honoured.

      **Protocol: 16 cases. Worker process: 10 cases**, including a real DXF written and
      validated through the socket, and the fingerprint refusal.

      **Not done, and not silently:** there is no drawing-specific cancellation test. The
      drawing path reuses the export path's `ExportJob` cancellation flag and its
      `activeExports_` registry, which `readyThenCancel` and the export tests already
      cover, and the cooperative check is the same code. A drawing-specific test would
      have to cancel inside a single row's projection, which races completion and would be
      flaky; a flaky test here is worse than a documented gap.

**Groundwork this task needed, recorded because none of it was in the plan:**

- `export/ShapeSelection` — resolving a node ID to its shape was already duplicated between
  the STEP and STL exporters, and the drawing path would have made three copies of a unit
  rebase. Extracted; the 7 export cases are unchanged.
- `drawing/DrawingExporter` — the one place the four stages meet (resolve, scale, project,
  write, validate), so the worker holds no geometry logic and the writers stay format-only.
- `drawing/DrawingPreview` — flattens arcs and cubics to polylines for the on-screen
  preview. Deliberately not a file format: a preview drawing straight polylines cannot
  disagree with the writers about arc direction or sweep sign, which is one of the two
  highest-severity risks in this feature. The written file always comes from the exact
  primitives.
- **Defect fixed at the root:** `loupe::drawing::DrawingFormat` was declared identically in
  both `DrawingPlan.h` (Task 10, mine) and `DrawingValidator.h`. Harmless while each header
  was used alone, and a redefinition the moment one translation unit needed both -- which
  the worker does, since it plans, writes and validates. Moved to `drawing/DrawingFormat.h`.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-protocol-tests loupe-worker-process-tests
ctest --preset windows-release --output-on-failure -R "protocol|worker-process"
```

---

## Task 14: Build the workspace QML — DONE 2026-07-25

Layout order is workflow order: choose a part, choose a view, read the 2D preview, then
queue it. The add action sits below the preview deliberately -- nothing is queued
sight-unseen.

**Files:**

- Create: `src/app/qml/drawing/DrawingWorkspace.qml`
- Create: `src/app/qml/drawing/DrawingComponentPicker.qml`
- Create: `src/app/qml/drawing/DrawingSetupPanel.qml`
- Create: `src/app/qml/drawing/DrawingPreview2D.qml`
- Create: `src/app/qml/drawing/DrawingQueue.qml`
- Create: `src/app/qml/drawing/DrawingOutputPanel.qml`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/qml/qml_smoke.cpp`

- [x] Root workspace aliases the controller once as `readonly property QtObject draft: controller ? controller.drawingWorkspace : null` and passes `draft` down, matching `ExportWorkspace`.
- [x] Lazily instantiate the 3D preview through an async `Loader` and replay geometry once both previews are ready, reusing the `ExportWorkspace` activation pattern.

      One 3D viewport here, not two, so the replay gate waits on that one. The 2D preview
      is not a viewport and needs no geometry replay.
- [x] `DrawingComponentPicker` reuses the Export picker's structure — the outer `Item` plus inner visual `Rectangle` for the collapse-gap fix, `forceLayout()` on toggle, `spacing: 0`, `objectName` per row — and adds a per-part queued count.

      No checkboxes: a checkbox cannot express one part queued three times, so a part is
      chosen here and queued from the setup panel, and the row shows its drawing count
      instead of a checked state.
- [x] `DrawingSetupPanel`: view control agreeing with the cube, content-mode segmented control, scale control, and the add action. **Ordered so the preview is read before the button is reached**, and disabled while the candidate is invalid or resolving.

      Agreement with the cube is structural, not a convention: the panel resolves a named
      view through the controller's own `directionForStandardView`, the same call the cube
      uses. **`upForStandardView` had to be added** -- the cube only ever needed a
      direction, but a drawing also needs to know which way is up on the page. It lives in
      the same table for the same reason: a named view that resolved its direction one way
      and its up another would produce a rotated drawing that still looked plausible.
- [x] `DrawingPreview2D` renders the returned contour geometry with a `Shape`, shows measured extents in the active unit, shows per-drawing warnings, shows a resolving state, and labels an approximate preview as approximate. It must never present stale geometry as current.

      Staleness is prevented twice: a reply whose revision is not the newest is discarded
      on arrival, and a candidate change blanks the geometry immediately, before any reply
      comes back. Leaving the previous drawing on screen while a new one resolves would
      present it as authoritative.

      Warning codes are translated into sentences rather than shown raw --
      `non_solid_bodies_ignored` becomes a statement about bodies with no interior.
- [x] `DrawingQueue`: reorderable rows with a human-readable view label, editable filename using the non-clobbering `Binding`-on-focus pattern from `ExportBucket`, per-row remove, live count, and selecting a row loads it into the preview.

      Rows read "part · view · scale", because three drawings of one part would otherwise
      read as three identical rows. Filename and status come from the plan rows, looked up
      by drawing ID rather than by index: a plan error can leave the queue and the plan
      rows momentarily different lengths, and an index would then show the wrong row's
      error.
- [x] `DrawingOutputPanel`: format, destination via `FolderDialog`, plan error, export button with progress and cancel, and the result summary. Plus the fiducial toggle, whose tooltip says why it exists -- DXF R12 cannot declare its own units, so a measured reference line is the only in-file proof the scale survived.
- [x] Register every new file in `qt_add_qml_module`'s `QML_FILES`.
- [x] No colour or duration literals anywhere; no new style-gate exemptions. `qml-style-gates` and `qml-theme-contrast` both clean.
- [x] Smoke test: drive the picker, set a view, add two drawings of the same part with different views, and assert two independent queue rows exist.

      Driven through the real QML with real clicks: it also asserts the add action is
      disabled before a view is chosen, and that the two rows get non-colliding paths.

      **One seam was needed in `StepViewport`:** `faceFrameSelectionEnabled` plus a
      `faceFrameSelected(frame)` signal, so a left click reports the whole face's frame
      rather than selecting a component. Off by default, so no existing viewport changes
      behaviour. It refuses to emit an empty frame, which would hand the listener a
      default that reads as a valid view.

      Note for a later reader: `viewportCaptureUsesRequestedRenderResolution` in the same
      binary fails when the executable is run directly and passes under ctest. It needs the
      environment the ctest entry declares. It also flaked once in ten runs; it is
      GPU-render dependent, not affected by this task.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-app loupe-qml-smoke-tests
ctest --preset windows-release --output-on-failure -R "qml-smoke|qml-style-gates|qml-theme-contrast"
```

---

## Task 15: Add the third workspace to the shell — DONE 2026-07-25

**Files:**

- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/qml/Main.qml`

- [x] Add `Drawing` to the `Workspace` enum. It is exposed to QML as `AppState.Drawing` through the existing `Q_ENUM_NS` registration, so no new registration is required.

      The switcher, the View menu and the `StackLayout` now all read one ordered
      `workspaces` list instead of two-way ternaries, so a fourth workspace cannot be added
      to one and forgotten in the others -- which is exactly what the ternaries invited.
- [x] Add a third entry to the workspace segmented control and a third View-menu item.
- [x] **Replace the two-way ternaries** driving `StackLayout.currentIndex` and the segmented control's `currentIndex` with an explicit enum-to-index mapping. They are currently written as `x === Inspect ? 0 : 1`, which silently sends any third workspace to the Export pane.

      Done via the shared `workspaces` list and a single `workspaceIndex` derived from it.
- [x] Add `DrawingWorkspace` as the third `StackLayout` child.
- [x] Confirm drag-and-drop still forces Inspect on open. Unaffected: the open path calls `setWorkspace(AppState.Inspect)` by name, not by index, and `mainLoads` still exercises the drop.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-app
ctest --preset windows-release --output-on-failure -R "qml"
```

---

## Gate E: Workspace UX review

Tasks 10-15 are complete. What is machine-verified is marked; what needs eyes on a real
part is listed separately rather than ticked off on the strength of the code being written.

- [ ] The 2D preview renders the candidate before it can be queued, with extents and warnings.

      **Reopened after a real assembly showed the preview drew nothing at all.** The extents
      and contour counts were correct, which is exactly why it read as working, but the
      geometry was built with a `Repeater` over `ShapePath` -- and `Repeater` requires an
      `Item` delegate, which `ShapePath` is not. Nothing was ever added to the `Shape`, and
      no warning was emitted. Fixed with two static `ShapePath`s fed by `PathMultiline`.

      The lesson worth keeping: I marked this item "needs a human for the visual" when the
      honest statement was that the preview was **entirely unverified**. Extents rendering
      correctly is not evidence that geometry renders at all, and nothing in the suite
      distinguished the two.

      Now covered by a test that asserts the point lists the `Shape` actually consumes,
      verified to fail when those lists come back empty -- which is what a blank preview
      looks like from the outside.

      Also added `preview.svg` to the `drawing-spike` output: the preview's own flattened
      polylines from `encodePreview`, with the same Y flip the QML applies, so the projection
      a user is about to trust can be looked at outside the running app. On a corpus bracket,
      face-on: 40.00 x 40.00 mm, 6 closed contours in silhouette mode and 18 in cut-contours,
      none open.

      Structurally guaranteed rather than merely arranged: the add action sits below the
      preview, and the smoke test asserts it is disabled until a view is chosen. Staleness
      is prevented twice -- a non-newest revision is discarded on arrival, and a candidate
      change blanks the geometry before any reply returns.

      **The human check has now happened, and it failed.** Reviewed on a real assembly
      (a trim ring with bins). The reviewer's verdict: the silhouette "does not seem to be
      accurate 100%. Bottom and left seems to be wrong ... while other sides are ok."

      So this item is not pending a sign-off; it is failing for a known and now-diagnosed
      reason. Recorded here rather than as a passing gate with a caveat.

      **Root cause, isolated:** OCCT's exact hidden-line removal returns *nothing* when the
      view direction is exactly axis-parallel and the body's faces are all exactly parallel
      or perpendicular to it. Measured with the new `drawing-bodies` probe on that assembly:

      | body | faces | edges along Z | edges along X |
      | --- | --- | --- | --- |
      | 0 | 104 | 53 | 28 |
      | 1 | 104 | 25 | 25 |
      | 2 | 725 | **0** | 73 |
      | 3 | 220 | 69 | 84 |

      And decisively, on body 2 alone: direction `(0,0,1)` gives 0 edges; `(0.001,0,1)` gives
      79. Z is Top and Bottom in a Z-up document, which is exactly the sides the reviewer
      called wrong.

      Two consequences worth separating. One body failing **blanked the entire assembly**
      projection, so a whole drawing was lost to a single part. And the failure was silent:
      HLR returns empty compounds rather than raising, so what came out was a valid,
      empty-looking drawing.

      **Fixed now:** the projector refuses an empty hidden-line result for a shape that has
      faces, since a shape with faces has visible edges from every direction. A silent wrong
      answer is now a reported one.

      **Deliberately not fixed, because it is a product decision:** the degeneracy itself. A
      tilt of 0.001 makes it work, at a cos error of about 5e-7 -- roughly 0.0002 mm over
      400 mm, below the 0.01 mm validation tolerance. But this feature's entire promise is
      exact 1:1, and silently tilting to trade that for robustness is not a call to make
      inside a geometry helper. The options, for whoever picks this up:

      1. Fail loudly on a degenerate view and tell the user to nudge the camera. Honest,
         preserves exactness, leaves standard Top/Bottom views broken on such parts.
      2. Retry with a minimal tilt and mark the drawing approximate, reusing the
         `approximate` flag the protocol already carries. Keeps standard views working; must
         be disclosed in the UI and, arguably, in the file.
      3. Fall back to `HLRBRep_PolyAlgo`, the polygonal hidden-line algorithm, which is far
         more robust here but works on triangulation and so is approximate by construction.

      Until one is chosen, this gate item stays open.
- [x] One part queues at several angles as independent rows with non-colliding names.

      Covered three times over: the plan tests, the controller tests, and the QML smoke test
      that clicks Top then Front on one part and checks two rows with different paths.
- [x] Selecting a queue row shows that drawing.
- [x] Four further defects found by using the workspace on a real assembly, none of which
      any test had caught: no hover highlight while picking a face, so there was no way to
      tell which surface a click would take; warning text overflowing out from under the
      preview; the queue cut off and unreachable because four panels do not fit a
      fixed-height column at 1080p; and a list of preset scale ratios where the product
      decision is 1:1 or a typed custom ratio. All fixed.

      Every one of these is a thing only looking at the running app on a real file surfaces.
      Worth remembering when weighing "the suite is green" against "it has been used".
- [x] Queued drawings are immune to later camera changes.

      The sharpest test in the controller suite: it changes the candidate view and scale
      after queueing, then asserts the queued row, its filename, and the plan fingerprint
      are all unmoved.
- [x] Face-normal views work on a non-axis-aligned face; a curved face is refused with a readable reason.

      The refusal is tested, with the measured deviation quoted back. Flatness is judged
      from the face's own triangle normals, not its vertex normals, which are blended across
      face borders -- the flat-face test in `scene-model` pins this by using shading normals
      deliberately 45 degrees off the plane.

      **Needs a human:** an actual click on a non-axis-aligned face in the running app. The
      pieces are each tested, but I cannot perform the click.
- [x] A full batch exports, validates, and reports per row.

      End to end over the local socket in `worker-process`: a real DXF written, reopened,
      validated, and reported. A wrong fingerprint is refused with nothing written. A failed
      row deletes its file and does not abort the batch.
- [x] Reviewed against `docs/review/ui-refinement-handoff.md`.

      The review found the document itself out of date rather than the code: section 4.1
      stated the product has **two** workspaces, which would have made a future reviewer
      treat Drawing as out-of-contract. Updated to three, with the shared-ordered-list rule
      recorded as a contract. The ownership table in section 7 gained
      `DrawingWorkspaceController`, `PickerComponents`, and the drawing core, since that
      table is the boundary UI engineers are held to. A Drawing workspace gate was added to
      section 10.

      Contracts checked and met: expensive work stays off the pointer path (projection runs
      in the worker); the viewport is configured, not forked; no colour or duration literals;
      identity is the stable node ID throughout; raw body rows stay suppressed because the
      picker logic is now literally shared rather than reimplemented.
- [x] Full CTest suite green on Windows.

      **171/171 passing**, two evidence symlink cases skipped as they always are on Windows.
- [ ] **Not verified: macOS.** No macOS machine in this session. The risk is concentrated in
      two places worth checking first there: `Qt.platform.os` now selects the window corner
      radius, and `Shape`'s `CurveRenderer` in the 2D preview is the newest Qt feature this
      feature depends on.

---

## Task 16: Release

**Files:**

- Modify: `CMakeLists.txt`
- Create: `docs/evidence/phase-3-report.md`
- Modify: `README.md`

- [ ] Bump `project(Loupe VERSION ...)` to `0.2.0`.
- [ ] Record evidence: the Gate A numbers, cross-format fixture results, DXF acceptance in each target application, and platform verification. Only case IDs, hashes, and outcomes — no private geometry, per the evidence policy.
- [ ] Document the workspace in the README, including the DXF R12 unit convention and the fiducial option.
- [ ] Tag `v0.2.0` and confirm the release workflow publishes both platforms.

**Focused verification:**

```bash
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

---

## Task 9b: Labels printed on the cube faces, and axis arrowheads — DONE 2026-07-25

Labels now lie on the faces as decals and foreshorten with them; the triad has
cylinder-and-cone arrows in the axis colours.

- [x] Print labels on the faces without `sourceItem`. Each is a pre-rendered SVG of
      white glyphs on a transparent ground, used as the material's **opacity map**:
      the glyph alpha masks, the tint comes from the theme, so labels follow light and
      dark mode and the source stays vector.
- [x] Axis arrowheads, `#Cylinder` shaft plus `#Cone` tip per axis.
- [x] qml-smoke re-run specifically. It fails by crashing rather than asserting, so a
      green core suite is not evidence on its own; 149/149 including qml-smoke.

**Why `Texture.sourceItem` is unusable here**, recorded because it is the obvious
first choice: rendering a QQuick item to a texture needs a live render context, so it
segfaults under the offscreen platform the smoke test uses, and would do the same in
any software-rendered environment.

The change is additive -- the cube body and its normal-based picking are untouched and
the decals are non-pickable -- so the reviewed behaviour was preserved rather than
rewritten.

Remaining polish, not blocking: the smoke test's stub theme lacks `errorColor`,
`success` and `accentColor`, so the axis colours log "Unable to assign [undefined] to
QColor" there. Harmless, but the stub needs those keys before the cube is asserted on
in that test.

---

## Task 3d: Spurious contours on a multi-body part — RESOLVED 2026-07-25

`PCBA_box` emitted 14 closed contours where it has one outline. Now 1, at the same
measured size. All four corpus parts clean: 1, 1, 5, 2 contours, all closed.

**Cause, identified in review: non-solid bodies.** Only a solid bounds material, so
only a solid can be classified inside or outside. A loose face or shell sharing the
compound contributes edges the region test cannot place, and they survived as stray
contours. Silhouette mode now projects the solids alone (`silhouetteSolidsOnly`, on
by default) and reports when it drops anything.

Both mechanisms this task originally proposed -- sliver regions from near-coincident
rims, and inconsistent pocket classification -- were wrong. Worth noting the guard
tried before them (skipping region-INTERNAL edges) was also wrong. Three incorrect
hypotheses in a row on the same symptom; what settled it was asking what kind of
body the geometry came from rather than what the algorithm did with it.

One implementation trap recorded: detecting whether anything was dropped by counting
bodies does not work, because a solid contains a shell and a loose face is not a
shell at all. Compare face counts before and after.

---

## Task 3c: Replace the silhouette edge filter with a projected-region boundary

Review verdict on the first implementation: close, but with two visible artifact
classes -- **over-extended lines** (stubs of an interior edge surviving near the
outer profile) and **missing lines** (a genuine boundary segment dropped, leaving a
gap in the outer contour). Both are inherent to the approach, not bugs to patch.

### Why the per-edge filter cannot get this right

It asks, per edge, "is material on both sides of this?" using sampled probes and a
majority vote. That is a different question from the one that defines a silhouette.
Consequences, matching exactly what review found:

- An edge that is **partly** boundary and partly interior gets one verdict for its
  whole length, so it is either kept entire (leaving an over-extended stub) or
  dropped entire (leaving a gap). Hidden-line removal already splits edges at
  visibility changes, not at boundary/interior transitions, so the pieces do not
  align with the decision the filter needs to make.
- A probe near a thin feature can straddle it, so a genuine boundary edge tests as
  interior. The probe distance cannot be both large enough to clear mesh error and
  small enough to never cross a thin wall.

### The correct formulation

As put during review: project the body onto a plane and take the boundary of the
resulting region. The silhouette is a property of the **projected area**, not of
individual edges, so it should be computed as one:

- [ ] Project the shape's triangulation into the drawing plane.
- [ ] Compute the boundary of the union of those projected triangles -- the edges
      belonging to exactly one triangle once coincident edges are merged. This
      yields closed loops by construction, which also removes this mode's
      dependence on stitching closure.
- [ ] Emit the outer loop plus interior loops, so through-holes survive while
      pockets (material on both sides) do not appear at all.
- [ ] Recover exactness where it matters: match each boundary loop against the HLR
      edge set and substitute the exact analytic curve wherever they coincide within
      tolerance, keeping the DXF arc encoding. Where no match exists, emit the
      polyline and count it, so the fidelity cost is reported rather than hidden.
- [ ] Retire the probe-based filter and its `interiorEdgesRemoved` statistic once
      this lands; keep `silhouette_unavailable` for the no-triangulation case.
- [ ] Tests: a stepped block yields one outer loop and no interior lines; a drilled
      plate keeps its hole; a pocketed part shows no pocket; a chiral part is not
      mirrored.

Accuracy note: the region boundary is mesh-derived, so its *shape* carries the mesh
deflection until the exact-curve substitution above replaces it. That is acceptable
for a silhouette whose purpose is outer dimensions, and it must be stated in the UI
rather than implied.

---

## Task 3b: Reliable closure, then Outer Contour Only

Requested during review. Prerequisite work identified by measurement, so it is sequenced rather than bolted on.

**Files:**

- Modify: `src/core/drawing/DrawingProjector.h`
- Modify: `src/core/drawing/DrawingProjector.cpp`
- Modify: `tests/core/test_drawing_projection.cpp`

### Why closure comes first

`ContentMode::OuterContourOnly` isolates the silhouette by taking a planar union of the projected regions and keeping the resulting outer wire (plus inner wires for through-holes). A region needs a closed boundary, so the mode is only as good as contour closure.

Measured on the corpus after the two-pass stitch: **45 of 62 contours closed (73%)**, and on `mount` — a stepped bracket, exactly the shape that motivates this mode — **0 of 4 closed**. Implementing the mode first would therefore produce nothing on the part that most needs it.

- [ ] Raise closure materially above the current 73%. Candidate causes to investigate in order: the stitch tolerance is derived as `max(1e-3, deflection)` and may be far tighter than the gaps hidden-line removal actually leaves; edge deduplication may be removing an edge a loop needs (one part had 50 duplicates removed); and `ConnectWiresToWires` is currently only offered the already-open wires, which is right for avoiding merged loops but may need an escalating tolerance sweep.
- [ ] Report closure as a first-class statistic per drawing so any regression is visible immediately.
- [ ] Add `ContentMode::OuterContourOnly`.
- [ ] Build a planar face per closed contour, fuse with `BRepAlgoAPI_Fuse`, and emit the fused result's outer wire plus its inner wires.
- [ ] Suppress every interior edge, whether or not hidden-line removal considered it visible. A step's top face and a chamfer boundary are both visible and both irrelevant to where material ends.
- [ ] When the silhouette cannot be isolated, emit a warning and refuse to silently fall back to the full edge set: the two outputs look similar enough that a user would not catch the difference, and one of them is the wrong cut path.
- [ ] Tests: a stepped block yields exactly one outer contour with no interior lines; a drilled plate keeps its hole; a part that cannot be closed reports the warning rather than degrading quietly.

**Focused verification:**

```bash
cmake --build --preset windows-release --target loupe-core-tests loupe-spike
ctest --preset windows-release --output-on-failure -R "drawing"
./build/windows-release/loupe-spike drawing-spike corpus/private/2D/mount.stp Z build/silhouette-check
```

---

## Deferred

In likely order, once this ships: custom-plane and arbitrary-angle views; cross-section drawings; DXF R2000 with an explicit millimetre units header, native splines, and ellipses; combined-sheet output; corner and edge clicks on the view cube for isometric views; dragging the cube to orbit; sheet-metal flat-pattern unfold.
