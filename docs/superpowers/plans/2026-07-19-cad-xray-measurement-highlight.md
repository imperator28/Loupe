# CAD X-Ray Measurement Highlight Implementation Plan

> **Implementation note:** Keep this pass limited to face-selection clarity. Build a locally launchable Debug review app and stop after focused UX validation; do not run the full corpus or cross-platform matrix before approval.

**Goal:** Render hover and committed measurement faces through bodies and section caps with an NX/Creo-style filled highlight and true face boundary.

**Design:** [2026-07-19-cad-xray-measurement-highlight-design.md](../specs/2026-07-19-cad-xray-measurement-highlight-design.md)

**Parent plan:** [2026-07-17-section-performance-theme-measurement-amendment.md](2026-07-17-section-performance-theme-measurement-amendment.md)

**Tech stack:** C++23, Qt 6.8+ Quick/QML/Quick3D, Qt Test, Qt Quick Test, CMake/Ninja.

## Constraints

The worktree contains uncommitted viewport, section, measurement, theme, and export work. Preserve and integrate it. Do not stage private `corpus/` STEP files or generated `controller-worker.step` and `repeated-box.step` fixtures.

The x-ray path must work on the Qt 6.8 project baseline. Do not depend on `PipelineStateOverride`, which was added in Qt 6.11. Do not alter source-body materials, section clipping, picking, or measurement calculations.

## Task 1: Extract true face boundaries

**Files:**

- Modify: `src/app/render/MeshGeometry.h`
- Modify: `src/app/render/CadEdgeGeometry.h`
- Modify: `src/app/render/CadEdgeGeometry.cpp`
- Modify: `tests/app/test_scene_model.cpp`

- [ ] Add `CadEdgeGeometry::copyFaceBoundaryFrom(QObject*, quint32)` for a face topology ID.
- [ ] Read the selected face's original triangle range from `MeshGeometry` without modifying the source geometry.
- [ ] Count canonical undirected triangle edges by original vertex index.
- [ ] Emit only edges used once, excluding internal tessellation diagonals while retaining outer and hole boundaries.
- [ ] Preserve source coordinates and upload the result as non-pickable line geometry.
- [ ] Return `false` safely for the wrong source type, missing topology, malformed ranges, or non-face topology.
- [ ] Test a two-triangle quad produces four boundary segments rather than the internal fifth edge.
- [ ] Test missing topology and empty boundary behavior.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-scene-model-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^scene-model$'
```

## Task 2: Add the Qt 6.8-compatible x-ray layer

**Files:**

- Create: `src/app/qml/inspect/MeasurementFaceHighlight.qml`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `tests/qml/qml_smoke.cpp`

- [ ] Add a transparent overlay `View3D` above the primary viewport and below 2D controls.
- [ ] Disable depth testing for the overlay environment so highlighted faces remain visible through bodies and section caps.
- [ ] Mirror the active camera rig, projection, field of view, clipping, and orthographic magnification with declarative bindings.
- [ ] Keep the overlay input-transparent and exclude it from picking, shadows, and source-scene depth.
- [ ] Implement `MeasurementFaceHighlight` as a reusable node with a translucent unlit face model and opaque boundary model.
- [ ] Give hover and committed instances explicit style properties rather than duplicating material logic.
- [ ] Keep highlight geometry independent of section clipping so the complete selected face remains visible.
- [ ] Extend the QML smoke fixture to instantiate the x-ray layer with both projections and section mode enabled.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-qml-smoke-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^qml-smoke$'
```

## Task 3: Wire hover and committed face states

**Files:**

- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `tests/qml/tst_SectionControls.qml`
- Modify: `tests/qml/tst_ViewportNavigation.qml`

- [ ] Reuse one hover face and boundary geometry, rebuilding only when node ID or topology ID changes.
- [ ] Render hover faces in the semantic accent color with moderate fill alpha and a crisp boundary.
- [ ] Render accepted faces in the semantic selection color with stronger fill alpha and boundary weight.
- [ ] Preserve the existing maximum of two accepted face picks and their numbered 2D markers.
- [ ] Clear x-ray geometry when measurement mode changes, the pointer exits, picks reset, or the scene is replaced.
- [ ] Keep edge and point measurement feedback unchanged.
- [ ] Ensure invalid topology for the active mode produces no x-ray highlight and does not disturb an accepted pick.
- [ ] Test hover-to-accepted transitions, clear/reset behavior, distinct style states, and non-pickable highlights.
- [ ] Test overlay-camera synchronization for orbit, pan, zoom, orthographic, and perspective states.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-qml-tests loupe-qml-smoke-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(qml-smoke|qml-inspection-tools)$'
```

## Task 4: Produce the focused review build

- [ ] Rebuild `build/macos-arm64-debug/src/app/Loupe.app`.
- [ ] Open a representative multi-body STEP assembly from `corpus/` without staging it.
- [ ] Verify hover faces remain visible when front-facing, edge-on, behind another body, and behind a section cap.
- [ ] Verify accepted face one and face two remain distinguishable and their numbered markers stay attached during orbit, pan, zoom, projection changes, and section movement.
- [ ] Review Light and Dark themes for sufficient contrast without obscuring model context.
- [ ] Confirm edge, point, radius, body, and ordinary inspection selection behavior is unchanged.
- [ ] Obtain UX approval before broad performance, metrology, Release, or Windows validation.

**Suggested implementation commits:**

1. `feat: extract CAD face highlight boundaries`
2. `feat: render measurement faces through geometry`
3. `test: cover x-ray measurement highlights`
