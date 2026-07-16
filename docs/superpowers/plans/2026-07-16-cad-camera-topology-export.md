# Loupe CAD Camera, Topology Measurement, and Export Workspace Implementation Plan

> **Implementation note:** Execute tasks in order. Keep each gate locally launchable and reviewable. Use focused Debug tests during UX iteration; defer full corpus, Release, and cross-platform validation until the corresponding UX gate is approved.

**Goal:** Deliver orthographic-first CAD navigation without cursor-pivot drift, a model-attached section manipulator, topology-aware measurement feedback, and the first functional nonmodal Export workspace.

**Design:** [2026-07-16-cad-camera-topology-export-design.md](../specs/2026-07-16-cad-camera-topology-export-design.md)

**Tech stack:** C++23, Qt 6 Quick/QML/Quick3D, OpenCascade XDE, Qt Test, Qt Quick Test, CMake/Ninja.

## Baseline and Constraints

The worktree already contains uncommitted camera/measurement fixes in:

- `src/app/ApplicationController.cpp`
- `src/app/ApplicationController.h`
- `src/app/qml/inspect/InspectWorkspace.qml`
- `src/app/qml/inspect/StepViewport.qml`
- `src/app/qml/inspect/ViewportNavigation.qml`
- `tests/app/test_application_controller.cpp`

Preserve and integrate those edits. Do not stage private corpus files or generated STEP fixtures.

The user-facing terminology is:

- Orthographic and Perspective for projection.
- Isometric, Top, Front, and Right for orientation.
- Hide others or Isolate for visibility commands.

## Gate A: Camera and Section Interaction

Gate A produces the first review build. It uses focused navigation, QML, section, and controller tests only.

### Task 1: Replace navigation state with drift-free camera math

**Files:**

- Modify: `src/app/qml/inspect/ViewportNavigation.qml`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Create: `tests/qml/tst_ViewportNavigation.qml`
- Modify: `tests/CMakeLists.txt` only if explicit test registration is required

- [ ] Add explicit camera state: world camera position, orientation, active pivot, pending pivot, projection mode, perspective distance, and orthographic magnification.
- [ ] Make pointer press call `setPendingOrbitPivot()` without applying camera state.
- [ ] Activate the pending pivot only after the drag threshold is crossed.
- [ ] Rotate camera position and orientation around the active world-space pivot using incremental quaternion updates.
- [ ] Remove pivot retargeting through `Quaternion.lookAt()` from drag start.
- [ ] Translate camera and pivot together during pan.
- [ ] Keep the pivot fixed during zoom.
- [ ] Make Fit reset the active pivot to the fitted bounds center.
- [ ] Preserve the existing Creo-style drag direction.
- [ ] Add tests proving press-only camera invariance, no translation before first orbit delta, consecutive-drag continuity, pan-pivot coupling, and zoom-pivot stability.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-qml-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(qml-smoke|qml-inspection-tools)$'
```

### Task 2: Add orthographic default and projection switching

**Files:**

- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `src/app/qml/inspect/ViewportNavigation.qml`
- Modify: `src/app/qml/inspect/ViewportVisualTheme.qml` if projection control tokens are needed
- Create: `src/app/qml/inspect/ProjectionControl.qml`
- Modify: `src/app/CMakeLists.txt`
- Extend: `tests/qml/tst_ViewportNavigation.qml`
- Create: `tests/qml/tst_ProjectionControl.qml`

- [ ] Add both `OrthographicCamera` and `PerspectiveCamera` under the same rig; only the active camera drives `View3D.camera`.
- [ ] Default new documents to orthographic projection and the approved isometric orientation.
- [ ] Add a compact Orthographic / Perspective segmented control near the standard-view controls.
- [ ] Preserve orientation, viewport center, pivot, and apparent model size while switching projection.
- [ ] Make Fit calculate orthographic magnification or perspective distance from the same visible bounds.
- [ ] Ensure T/F/R change orientation only.
- [ ] Ensure capture uses the active camera.
- [ ] Add tests for default projection, projection round-trip framing, standard-view projection invariance, and document-open reset.

### Task 3: Replace the section overlay with a scene-space manipulator

**Files:**

- Create: `src/app/qml/inspect/SectionManipulator.qml`
- Create: `src/app/qml/inspect/SectionOffsetSlider.qml`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `src/app/qml/inspect/SectionPanel.qml`
- Modify: `src/app/tools/SectionController.h`
- Modify: `src/app/tools/SectionController.cpp`
- Create: `tests/qml/tst_SectionManipulator.qml`
- Extend: `tests/app/test_inspection_tools.cpp`
- Modify: `src/app/CMakeLists.txt`

- [ ] Expose the effective normalized plane normal, initial offset, and active offset from `SectionController`.
- [ ] Calculate a stable manipulator anchor by intersecting the section plane with the visible model bounds and choosing a point near the visible section centroid.
- [ ] Render a real scene-space shaft, arrowhead, and plane cue aligned to the effective normal.
- [ ] Scale the manipulator from camera state so its hit target remains usable without detaching visually from the model.
- [ ] Project the normal into viewport coordinates and convert pointer movement along that axis into model-space offset.
- [ ] Support Shift fine adjustment, Escape cancel, double-click reset, and numeric entry synchronization.
- [ ] Detect an edge-on projected axis using a documented pixel threshold.
- [ ] Show the slim left viewport slider only while direct manipulation is unstable.
- [ ] Keep the manipulator and fallback slider hidden from clean captures unless capture options explicitly include tools.
- [ ] Add tests for axis projection, flip direction, fine adjustment, cancellation, reset, fallback threshold, and panel synchronization.

### Gate A Review

- [ ] Build and launch `build/macos-arm64-debug/src/app/Loupe.app`.
- [ ] Use one medium assembly to review orthographic isometric default, projection switching, repeated pause/restart orbit drags, pan, zoom, and fit.
- [ ] Review section dragging at oblique, normal, and nearly edge-on camera angles.
- [ ] Record UX issues without running the full corpus or Release matrix.
- [ ] Obtain user approval before Gate B polish or broad validation.

**Suggested commit:** `feat: establish orthographic CAD navigation and section control`

## Gate B: Topology-Aware Measurement

Gate B changes the worker payload and picking model. Keep protocol changes versioned and compatible with cached geometry invalidation.

### Task 4: Define topology payload contracts

**Files:**

- Modify: `src/protocol/ProtocolTypes.h`
- Modify: `src/protocol/GeometryPayload.h`
- Modify: `src/protocol/GeometryPayload.cpp`
- Modify: `src/protocol/ProtocolJson.cpp` only for control/status messages
- Modify: `tests/protocol/test_geometry_payload.cpp`
- Modify: `tests/protocol/test_protocol.cpp`

- [ ] Define document-generation-local `TopologyId`, entity kind, triangle range, edge-polyline range, and entity metrics.
- [ ] Include exact face area, edge length, curve type, radius/diameter data, center/axis where defined, and planar face data.
- [ ] Keep visible body mesh payloads contiguous while attaching triangle-to-face ranges.
- [ ] Keep CAD edge payloads contiguous while attaching segment-to-edge ranges.
- [ ] Reject overlapping, out-of-bounds, duplicate, or non-finite ranges during decoding.
- [ ] Increment the geometry cache profile so stale payloads are not replayed.
- [ ] Add binary round-trip and malformed-payload tests.

**Focused verification:**

```bash
cmake --build build/macos-arm64-debug --target loupe-protocol-tests loupe-geometry-payload-tests -j 6
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(protocol|geometry-payload)$'
```

### Task 5: Encode stable face and edge identity in the worker

**Files:**

- Modify: `src/worker/WorkerServer.cpp`
- Modify: `src/core/inspection/TopologyAnalysis.h`
- Modify: `src/core/inspection/TopologyAnalysis.cpp`
- Create: `src/core/inspection/TopologyEntity.h`
- Create: `src/core/inspection/TopologyEntity.cpp`
- Modify: `tests/worker/test_worker_process.cpp`
- Modify: `tests/core/test_geometry_analysis.cpp` or the existing topology-analysis test target

- [ ] Enumerate faces and edges deterministically within each definition shape.
- [ ] Carry definition topology identity through occurrence placement without recomputing metrics per repeated occurrence.
- [ ] Preserve triangle ranges while grouping visible mesh segments by source color.
- [ ] Preserve edge ranges while generating CAD curve polylines.
- [ ] Calculate exact entity metrics from BRep adaptors and geometric properties.
- [ ] Emit preview ranges early and replace only refinement-dependent geometry ranges during final refinement.
- [ ] Add repeated-definition tests proving stable IDs and reused metrics.
- [ ] Add planar, cylindrical, circular-edge, spline-edge, and unsupported-radius fixtures.

### Task 6: Add render-layer topology picking and highlight geometry

**Files:**

- Create: `src/app/render/TopologyPicker.h`
- Create: `src/app/render/TopologyPicker.cpp`
- Create: `src/app/render/TopologyHighlightGeometry.h`
- Create: `src/app/render/TopologyHighlightGeometry.cpp`
- Modify: `src/app/render/MeshGeometry.h`
- Modify: `src/app/render/MeshGeometry.cpp`
- Modify: `src/app/render/CadEdgeGeometry.h`
- Modify: `src/app/render/CadEdgeGeometry.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/app/test_topology_picker.cpp`
- Extend: `tests/app/test_scene_model.cpp`

- [ ] Retain source topology ranges alongside render geometry.
- [ ] Resolve face picks from hit triangle/ray data to a face ID.
- [ ] Resolve edge picks using screen-space curve proximity with depth and visibility ordering.
- [ ] Apply mode eligibility before hover feedback.
- [ ] Build highlight geometry only for the current hover entity and accepted picks.
- [ ] Use complete face fill/outline or complete edge stroke, never a point proxy for entity modes.
- [ ] Keep highlight geometry scene-attached through orbit, pan, zoom, projection changes, and sectioning.
- [ ] Add tests for face resolution, edge priority, occlusion ordering, unsupported entities, and highlight range extraction.

### Task 7: Integrate entity picks with measurement UX

**Files:**

- Modify: `src/app/tools/MeasurementController.h`
- Modify: `src/app/tools/MeasurementController.cpp`
- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/ApplicationController.cpp`
- Modify: `src/app/qml/inspect/StepViewport.qml`
- Modify: `src/app/qml/inspect/MeasurementPanel.qml`
- Modify: `tests/app/test_inspection_tools.cpp`
- Modify: `tests/app/test_application_controller.cpp`
- Create: `tests/qml/tst_MeasurementSelection.qml`

- [ ] Replace point-only measurement picks with a typed `MeasurementPick` carrying topology ID, entity kind, metrics, anchor, and normal/axis data.
- [ ] Keep point markers only for Point to point.
- [ ] Show full-edge persistent highlights for Edge length and circular-edge radius.
- [ ] Show full-face persistent highlights for Surface distance, Angle, Surface area, and supported face radius.
- [ ] Keep body highlight for Volume and Bounds.
- [ ] Number accepted entities where ordering matters and show explicit entity descriptions in the panel.
- [ ] Clear incompatible picks when mode changes; do not record picks while the Measurement panel is closed.
- [ ] Preserve the last completed result when closing the panel until Clear or document reset.
- [ ] Add tests for eligibility, exact selected-entity values, mode transitions, close behavior, and navigation attachment.

### Gate B Review

- [ ] Review hover pre-highlight and accepted highlights for point, line, spline edge, planar face, circular edge, cylindrical face, and body.
- [ ] Verify every displayed value belongs to the highlighted entity.
- [ ] Check interaction latency on the largest local corpus assembly without running a full benchmark matrix.
- [ ] Obtain user approval before broad topology performance work.

**Suggested commit:** `feat: add topology-aware CAD measurement`

## Gate C: Functional Export Workspace

Gate C first establishes the review desk and plan UX, then wires execution after the workspace layout is approved.

### Task 8: Add export draft and component selection models

**Files:**

- Create: `src/app/export/ExportSelectionModel.h`
- Create: `src/app/export/ExportSelectionModel.cpp`
- Create: `src/app/export/ExportDraftController.h`
- Create: `src/app/export/ExportDraftController.cpp`
- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/ApplicationController.cpp`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/app/test_export_selection_model.cpp`
- Create: `tests/app/test_export_draft_controller.cpp`

- [ ] Model searchable component rows with node ID, hierarchy path, kind, quantity, warnings, checked state, and highlight state.
- [ ] Keep checked state independent from viewport/tree highlight.
- [ ] Preserve the draft while switching Inspect/Export for the same document.
- [ ] Reset the draft on document replacement.
- [ ] Expose only policy combinations accepted by `core/export/ExportPlan`.
- [ ] Build immutable plan previews and map `PlanError` to a setting or output row.
- [ ] Add tests for check/highlight separation, persistence, reset, deterministic rows, collisions, unsafe names, units, and unsupported combinations.

### Task 9: Build the nonmodal Export review desk

**Files:**

- Create: `src/app/qml/export/ExportWorkspace.qml`
- Create: `src/app/qml/export/ExportComponentList.qml`
- Create: `src/app/qml/export/ExportContextView.qml`
- Create: `src/app/qml/export/ExportFocusedView.qml`
- Create: `src/app/qml/export/ExportSettings.qml`
- Create: `src/app/qml/export/ExportPlanTable.qml`
- Create: `src/app/qml/export/ExportRunBar.qml`
- Modify: `src/app/qml/Main.qml`
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/qml/tst_ExportWorkspace.qml`
- Create: `tests/qml/tst_ExportComponentList.qml`

- [ ] Replace the temporary Export label with a lazily loaded workspace.
- [ ] Implement the selected nonmodal layout: searchable checks, assembly context, focused preview, settings, output plan, and run bar.
- [ ] Reuse scene geometry without reimporting or duplicating full CPU mesh storage.
- [ ] Synchronize focus between list, output table, context view, and isolated preview without changing checked state.
- [ ] Show destination, STEP/STL, coordinates, grouping, and units with unsupported combinations disabled and explained.
- [ ] Show deterministic filename previews and row-level warnings/blockers before Run becomes enabled.
- [ ] Keep the layout dense, restrained, keyboard-operable, theme-aware, and free of nested decorative cards.
- [ ] Add QML tests for focus/check separation, filtering, settings, blockers, workspace persistence, and narrow-width layout.

### Gate C1 UX Review

- [ ] Launch the review build with plan preview functional but export execution disabled behind the blocking state `Review workspace UX`.
- [ ] Review selection speed, information hierarchy, preview usefulness, filename clarity, settings discoverability, and error placement.
- [ ] Revise the workspace before wiring long-running execution.
- [ ] Obtain user approval for the Export workspace UX.

### Task 10: Wire worker export execution, progress, cancellation, and results

**Files:**

- Modify: `src/protocol/ProtocolTypes.h`
- Modify: `src/protocol/ProtocolJson.cpp`
- Create: `src/worker/ExportSession.h`
- Create: `src/worker/ExportSession.cpp`
- Modify: `src/worker/WorkerServer.cpp`
- Modify: `src/app/worker/WorkerClient.h`
- Modify: `src/app/worker/WorkerClient.cpp`
- Extend: `src/app/export/ExportDraftController.*`
- Create: `src/app/export/ExportResultsModel.h`
- Create: `src/app/export/ExportResultsModel.cpp`
- Create: `src/app/qml/export/ExportResults.qml`
- Modify: `tests/worker/test_worker_process.cpp`
- Create: `tests/app/test_export_results_model.cpp`

- [ ] Serialize the immutable reviewed plan and fingerprint to the worker.
- [ ] Execute supported rows against the worker-owned imported document without mutating source geometry.
- [ ] Use existing atomic STEP/STL writers and mandatory read-back validation.
- [ ] Emit overall and per-row stages: queued, writing, validating, complete, warning, failed, canceled.
- [ ] Cancel between rows and preserve atomic-write guarantees for the active row.
- [ ] Present validated results, warnings, failures, output paths, and retryable rows.
- [ ] Reject execution when the current draft no longer matches the reviewed plan fingerprint.
- [ ] Add worker integration tests for success, collision rejection, unit blocking, cancellation, partial completion, and source immutability.

### Gate C2 Functional Review

- [ ] Run one supported multi-row STEP export and one binary STL export into a temporary review folder.
- [ ] Confirm progress, cancellation, row results, output opening, and source immutability.
- [ ] Keep validation focused to the reviewed files until UX approval.

**Suggested commit:** `feat: establish selective Export workspace`

## Final Validation After UX Approval

Only after Gates A, B, and C are approved:

- [ ] Run all focused Debug tests for protocol, worker, controller, render, QML, inspection, capture, cache, and export.
- [ ] Run the generated STEP fixture import/export/read-back suite.
- [ ] Run the approved private corpus on macOS and record redacted evidence only.
- [ ] Run the macOS Release build and full CTest matrix.
- [ ] Run the Windows 11 Debug and Release equivalents before declaring cross-platform gate completion.
- [ ] Profile topology picking and Export workspace QML on the largest assembly; optimize only measured bottlenecks.
- [ ] Run `qt-qml-review` and `qt-cpp-review` before the final feature commit or pull request.
- [ ] Update `docs/review/phase-1-ux-review.md` with the new camera, section, measurement, and Export review script.

## Review Build Commands

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-worker -j 6
open -n build/macos-arm64-debug/src/app/Loupe.app
```

Do not stage these local/generated files:

```text
controller-worker.step
repeated-box.step
corpus/*.stp
```
