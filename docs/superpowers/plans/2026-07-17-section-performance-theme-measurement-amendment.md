# Loupe Section Performance, Theme, and Measurement Amendment Plan

> **Implementation note:** Keep the build locally launchable after each task. Validate the reviewed interactions with focused Debug tests; do not run the full corpus, Release, or cross-platform matrix before UX approval.

**Goal:** Make section dragging immediate, finish dark-theme control readability, and deliver topology-correct point, edge, and face measurement feedback.

**Design:** [2026-07-17-section-performance-theme-measurement-amendment-design.md](../specs/2026-07-17-section-performance-theme-measurement-amendment-design.md)

**Parent plan:** [2026-07-16-cad-camera-topology-export.md](2026-07-16-cad-camera-topology-export.md)

**Tech stack:** C++23, Qt 6 Quick/QML/Quick3D, OpenCascade XDE, Qt Test, Qt Quick Test, CMake/Ninja.

## Constraints

The worktree contains the uncommitted Gate A camera, projection, section-manipulator, refinement, and test changes. Preserve and integrate them. Do not stage private `corpus/` STEP files or generated `controller-worker.step` and `repeated-box.step` fixtures.

Production rendering remains on Qt's selected RHI backend. Section preview optimization must not replace GPU rendering with a software viewport or change capture output.

## Task 1: Add a coalesced two-phase section pipeline

**Files:**

- Modify: `src/app/tools/SectionController.h`
- Modify: `src/app/tools/SectionController.cpp`
- Modify: `src/app/render/MeshGeometry.h`
- Modify: `src/app/render/MeshGeometry.cpp`
- Modify: `src/app/qml/inspect/SectionManipulator.qml`
- Modify: `src/app/qml/inspect/SectionOffsetSlider.qml`
- Modify: `src/app/qml/inspect/SectionPanel.qml`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `tests/app/test_inspection_tools.cpp`
- Modify: `tests/app/test_scene_model.cpp`
- Modify: `tests/qml/tst_SectionControls.qml`

- [ ] Expose explicit `beginInteraction`, preview-offset, `commitInteraction`, and cancel behavior from `SectionController`.
- [ ] Update the manipulator immediately from the requested offset while geometry catches up independently.
- [ ] Coalesce preview requests so only the latest pending offset is retained.
- [ ] Frame-limit preview rebuilds and prevent synchronous rebuild re-entry or an event backlog.
- [ ] Add a preview rebuild mode that clips visible triangles but skips cap triangulation and 2D outline reconstruction.
- [ ] Trigger one exact rebuild on release using the final offset, cap state, and selected 2D presentation.
- [ ] Apply the same preview/commit contract to the fallback slider and repeated numeric stepping.
- [ ] Add tests for lifecycle state, latest-request wins, cancel/reset, preview omission of cap work, and exact release output.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-inspection-tools-tests loupe-scene-model-tests loupe-qml-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(inspection-tools|scene-model|qml-inspection-tools)$'
```

## Task 2: Complete semantic theme controls

**Files:**

- Create: `src/app/qml/inspect/ThemedComboBox.qml`
- Create: `src/app/qml/inspect/ThemedSpinBox.qml`
- Create: `src/app/qml/inspect/ThemedSwitch.qml`
- Create: `src/app/qml/inspect/ThemedRadioButton.qml`
- Create: `src/app/qml/inspect/ThemedTextField.qml`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/qml/inspect/SectionPanel.qml`
- Modify: `src/app/qml/inspect/CapturePanel.qml`
- Modify: `src/app/qml/inspect/ContextPanel.qml`
- Modify: `src/app/qml/inspect/MeasurementPanel.qml` if native controls remain
- Create: `tests/qml/tst_ThemedControls.qml`
- Extend: `tests/qml/tst_SectionControls.qml`

- [ ] Build controls from the active semantic theme rather than platform palette defaults.
- [ ] Define normal, hover, pressed, focused, disabled, selected, placeholder, popup, and indicator colors.
- [ ] Migrate Section, Capture, Context, Material, and related dialogs to the shared controls.
- [ ] Preserve keyboard focus, editable values, popup behavior, and accessibility names.
- [ ] Test control text and popup delegate contrast in Light and Dark themes.
- [ ] Visually inspect the Position row, all section switches, material dialogs, and selection boxes from the reported screenshot.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-qml-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(qml-smoke|qml-inspection-tools)$'
```

## Task 3: Add topology payload identity and metrics

Execute Tasks 4 and 5 from the parent plan.

- [ ] Version geometry payloads with deterministic face and edge IDs and validated render ranges.
- [ ] Encode exact face area, edge length, curve type, supported radius, and face-plane/axis metrics in the worker.
- [ ] Preserve identity through color grouping, refinement replacement, caching, and repeated occurrences.
- [ ] Add binary round-trip, malformed-range, deterministic-ID, and representative topology tests.

## Task 4: Add topology picking and render-attached highlights

Execute Task 6 from the parent plan.

- [ ] Resolve face hits from triangle ranges and edge hits from visible screen-space curve proximity.
- [ ] Filter hover candidates by active measurement mode before showing feedback.
- [ ] Render a complete face fill/outline or complete edge stroke for hover and accepted picks.
- [ ] Keep highlights attached through orbit, pan, zoom, projection changes, sectioning, and refinement replacement.
- [ ] Add focused picker, range extraction, priority, and attachment tests.

## Task 5: Integrate typed measurement picks

Execute Task 7 from the parent plan.

- [ ] Replace point-only records with typed point, edge, face, or body picks carrying topology metrics.
- [ ] Keep point markers only for point-to-point mode.
- [ ] Persist and number accepted full-entity highlights while the measurement remains active.
- [ ] Prevent ordinary inspection clicks from creating measurement entities.
- [ ] Report the exact highlighted entity's value and description in the panel.
- [ ] Add mode-transition, eligibility, clear/close, and exact-entity tests.

## Review Build

- [ ] Launch `build/macos-arm64-debug/src/app/Loupe.app` with a representative corpus assembly.
- [ ] Verify immediate section-handle motion, bounded preview latency, and exact release output.
- [ ] Review every affected dialog and selector in Light and Dark themes.
- [ ] Review hover and accepted point, edge, circular edge, planar face, cylindrical face, and body highlights.
- [ ] Verify Metal or the expected platform RHI backend with `QSG_INFO=1` when diagnosing performance.
- [ ] Stop at focused UX validation and obtain approval before broad performance or metrology validation.

**Suggested implementation commits:**

1. `perf: make section interaction responsive`
2. `fix: complete themed CAD controls`
3. `feat: add topology-aware measurement highlights`
