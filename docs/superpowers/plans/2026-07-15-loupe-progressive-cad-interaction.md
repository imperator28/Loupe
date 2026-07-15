# Loupe Progressive CAD Interaction Implementation Plan

> Execute task-by-task. Keep the supplied STEP corpus local and uncommitted. Use focused tests inside each task and one integrated macOS review gate at the end; do not run the release-validation matrix before UX approval.

**Goal:** Deliver the approved progressive CAD interaction milestone: a five-second-target interactive preview, explicit refinement progress, improved curved rendering, exact settled sections, topology-aware measurement feedback, theme switching, shared context actions, background deselection, and a clickable view cube.

**Design:** [Loupe Progressive CAD Interaction Design](../specs/2026-07-15-loupe-progressive-cad-interaction-design.md)

**Architecture:** Retain STEP/XCAF/BRep ownership in `loupe-worker`. Replace base64 geometry events with a checked, length-prefixed protocol; stream one preview and one final payload per unique definition; keep occurrence transforms in the shell. Use Qt Quick 3D for progressive surface presentation, while retained OCCT shapes answer topology and exact-section queries. QML consumes semantic theme state and shared action models rather than duplicating behavior.

**Tech Stack:** C++23, Qt 6.11 Core/Gui/Qml/Quick/QuickControls2/Quick3D/Network/Test/QuickTest, OpenCascade 8, SQLite cache, CMake/Ninja.

## Execution Rules

- Preserve the current dirty worktree. Stage only files named by the active task and review overlapping changes before editing.
- Do not commit `corpus/*.stp`, generated captures, build output, `.superpowers/`, or `controller-worker.step`.
- Add one failing focused test before each behavior change, then implement the smallest complete vertical slice.
- Use stable definition IDs for geometry and stable occurrence IDs for user selection.
- Reject stale worker results by document generation and request ID.
- Never remove the usable preview because final refinement, cache, topology, or section work fails.
- Keep all CAD work off the GUI thread.
- Run `git diff --check` before every gate checkpoint.
- Use the Qt/QML review skills at gate checkpoints, not after each small edit.

## Gate 0: Establish The Reviewed Baseline

### Task 0.1: Checkpoint the current functional recovery slice

**Review and stage only:**

- current modified files under `src/`, `tests/`, `scripts/`, `CMakeLists.txt`, and `vcpkg.json` that belong to the approved functional recovery;
- new material, viewport-navigation, and CAD-edge source files;
- exclude all user corpus and generated files.

- [ ] Run the existing focused suite:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-scene-model-tests loupe-application-controller-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(protocol|worker-process|application-controller|worker-client|qml-smoke|qml-inspection-tools|scene-model|inspection-tools|material-store)$'
scripts/review/macos.sh
git diff --check
```

- [ ] Review the staged diff for accidental corpus, capture, or build artifacts.
- [ ] Commit the recovered P1 baseline as its own logical checkpoint before protocol migration.

Expected checkpoint: the current review build remains launchable and its existing smoke gate passes unchanged.

### Task 0.2: Add timing and quality baselines

**Create:**

- `src/worker/ImportMetrics.h`
- `src/worker/ImportMetrics.cpp`
- `tests/worker/test_import_metrics.cpp`
- `scripts/benchmark/inspect-corpus.sh`

**Modify:**

- `src/worker/CMakeLists.txt`
- `src/worker/WorkerServer.cpp`
- `tests/CMakeLists.txt`

- [ ] Add a deterministic metrics model for tree-ready, first-body, preview-ready, and final-ready timestamps plus triangle and body counts.
- [ ] Emit privacy-safe metrics containing source basename and hash only, never source geometry or absolute path.
- [ ] Add a corpus runner that accepts explicit local paths and writes JSONL beneath `build/`, not the repository.
- [ ] Record the current cold-open baseline for all supplied STEP files before optimization.
- [ ] Record current screenshots of the known curved housing and section cases under `build/` as non-versioned comparison evidence.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-worker loupe-worker-process-tests loupe-import-metrics-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(worker-process|import-metrics)$'
scripts/benchmark/inspect-corpus.sh corpus/corpus-a.stp corpus/corpus-b.stp corpus/corpus-c.stp
```

## Gate 1: Binary Transport And Progressive Refinement

### Task 1.1: Introduce checked protocol framing

**Create:**

- `src/protocol/ProtocolFrame.h`
- `src/protocol/ProtocolFrame.cpp`
- `src/protocol/GeometryPayload.h`
- `src/protocol/GeometryPayload.cpp`
- `tests/protocol/test_protocol_frame.cpp`
- `tests/protocol/test_geometry_payload.cpp`

**Modify:**

- `src/protocol/CMakeLists.txt`
- `src/protocol/ProtocolTypes.h`
- `src/protocol/ProtocolJson.cpp`
- `tests/CMakeLists.txt`

- [ ] Define a protocol-major bump for length-prefixed envelopes.
- [ ] Encode control messages as compact JSON frames and geometry as typed binary frames.
- [ ] Include request ID, document generation, definition ID, refinement level, segment key, source color, bounds, occurrence transforms, vertex count, index count, stride, and scalar/index formats.
- [ ] Enforce maximum frame, vertex, index, segment, and transform counts before allocating.
- [ ] Reject truncated frames, invalid counts, unknown required flags, unsupported major versions, and indices outside the vertex range.
- [ ] Preserve minor-version forward compatibility for ignorable metadata.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-protocol-tests loupe-protocol-frame-tests loupe-geometry-payload-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(protocol|protocol-frame|geometry-payload)$'
```

### Task 1.2: Migrate worker and client transport atomically

**Modify:**

- `src/worker/WorkerServer.h`
- `src/worker/WorkerServer.cpp`
- `src/app/worker/WorkerClient.h`
- `src/app/worker/WorkerClient.cpp`
- `tests/worker/test_worker_process.cpp`
- `tests/app/test_worker_client.cpp`

- [ ] Add incremental frame decoders that tolerate partial and multiple socket reads.
- [ ] Replace newline reads and base64 geometry events only after both sides understand framed messages.
- [ ] Keep one terminal event per open request and explicit cancellation acknowledgement.
- [ ] Tag every event with document generation; test that late frames from a prior open are ignored.
- [ ] Apply backpressure with bounded outgoing and incoming frame queues.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-worker loupe-worker-process-tests loupe-worker-client-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(worker-process|worker-client|protocol-frame|geometry-payload)$'
```

### Task 1.3: Add adaptive preview and final tessellation profiles

**Create:**

- `src/worker/TessellationProfile.h`
- `src/worker/TessellationProfile.cpp`
- `src/worker/GeometryEncoder.h`
- `src/worker/GeometryEncoder.cpp`
- `tests/worker/test_tessellation_profile.cpp`
- `tests/worker/test_geometry_encoder.cpp`

**Modify:**

- `src/worker/CMakeLists.txt`
- `src/worker/WorkerServer.cpp`
- `tests/CMakeLists.txt`

- [ ] Derive preview and final linear deflection from definition bounds with absolute clamps.
- [ ] Set independent angular deflection for preview and final profiles.
- [ ] Deduplicate repeated occurrences by stable definition ID and send occurrence transforms once.
- [ ] Preserve per-face source colors and OCCT triangulation normals.
- [ ] Sample defining BRep edges with profile-aware linear and angular tolerances.
- [ ] Unit-test curved fixtures at materially different scales; final profiles must increase quality without unbounded triangle growth.
- [ ] Never execute jobs sharing mutable triangulation state concurrently; copy independent definitions before pool submission.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-tessellation-profile-tests loupe-geometry-encoder-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(tessellation-profile|geometry-encoder)$'
```

### Task 1.4: Stream preview first, then bounded refinement

**Create:**

- `src/worker/GeometryScheduler.h`
- `src/worker/GeometryScheduler.cpp`
- `tests/worker/test_geometry_scheduler.cpp`

**Modify:**

- `src/worker/ImportSession.h`
- `src/worker/ImportSession.cpp`
- `src/worker/WorkerServer.h`
- `src/worker/WorkerServer.cpp`
- `src/app/worker/WorkerClient.h`
- `src/app/worker/WorkerClient.cpp`
- `src/app/ApplicationController.h`
- `src/app/ApplicationController.cpp`
- `src/app/qml/inspect/InspectWorkspace.qml`
- `tests/worker/test_worker_process.cpp`
- `tests/app/test_application_controller.cpp`

- [ ] Rank unique definitions by visible bounding volume, then preserve deterministic stable-ID order for ties.
- [ ] Stream each preview definition immediately and emit first-body and preview body-count progress.
- [ ] Mark the document interactive after all preview definitions are available; do not wait for final edges.
- [ ] Start final jobs in the bounded pool and emit completed/total bodies, percentage, elapsed time, and ETA.
- [ ] Add `Cancel refinement`; preserve previews and completed final bodies.
- [ ] Distinguish `Interactive preview ready`, `Refining geometry`, `Refinement canceled`, `Refinement incomplete`, and `Final quality ready`.
- [ ] Keep the full viewport interactive while refinement messages arrive.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-worker loupe-app loupe-geometry-scheduler-tests loupe-worker-process-tests loupe-application-controller-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(geometry-scheduler|worker-process|application-controller|qml-smoke)$'
```

### Task 1.5: Replace body geometry without duplicates or state loss

**Modify:**

- `src/app/render/MeshGeometry.h`
- `src/app/render/MeshGeometry.cpp`
- `src/app/render/CadEdgeGeometry.h`
- `src/app/render/CadEdgeGeometry.cpp`
- `src/app/render/SceneModel.h`
- `src/app/render/SceneModel.cpp`
- `src/app/qml/inspect/StepViewport.qml`
- `tests/app/test_scene_model.cpp`

- [ ] Key render resources by definition, segment, and refinement level.
- [ ] Build final buffers before swapping visibility; destroy preview buffers after the final model is active.
- [ ] Preserve occurrence selection, visibility, isolation, appearance, section state, and camera target through replacement.
- [ ] Queue a bounded number of QML model creations per frame.
- [ ] Test that repeated definitions share one geometry buffer and that replacement never doubles visible models.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-scene-model-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(scene-model|qml-smoke)$'
```

### Task 1.6: Cache preview and final geometry independently

**Create:**

- `src/app/cache/GeometryCacheKey.h`
- `src/app/cache/GeometryCacheKey.cpp`
- `tests/app/test_geometry_cache.cpp`

**Modify:**

- `src/app/cache/CacheStore.h`
- `src/app/cache/CacheStore.cpp`
- `src/app/ApplicationController.cpp`
- `src/app/CMakeLists.txt`
- `tests/CMakeLists.txt`

- [ ] Include source fingerprint, effective unit, importer, renderer, protocol, and quality profile in the key.
- [ ] Permit valid preview replay when final geometry is absent or stale.
- [ ] Commit cache payloads atomically only after complete validation.
- [ ] Evict preview and final records as one source family while allowing independent invalidation.
- [ ] Treat cache read/write failure as recoverable and continue from source.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-geometry-cache-tests loupe-application-controller-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(geometry-cache|application-controller|cache-store)$'
```

### Gate 1 checkpoint

- [ ] Run the focused Gate 1 suite and cold/warm corpus benchmark.
- [ ] Compare first-body, preview-ready, final-ready, memory, and triangle counts with Gate 0.
- [ ] Confirm the 19 MB assembly is interactive near the five-second target or record which measured stage exceeds its budget before changing tolerances.
- [ ] Review protocol bounds, queue bounds, ownership, cancellation, and OCCT thread safety.
- [ ] Commit Gate 1 as one reviewed progressive-rendering slice.

## Gate 2: Shared Interaction And Theme Foundations

### Task 2.1: Add semantic System, Light, and Dark themes

**Create:**

- `src/app/qml/theme/ThemeTokens.qml`
- `src/app/models/ThemePreference.h`
- `src/app/models/ThemePreference.cpp`
- `tests/app/test_theme_preference.cpp`
- `tests/qml/tst_Theme.qml`

**Modify:**

- `src/app/CMakeLists.txt`
- `src/app/main.cpp`
- `src/app/qml/Theme.qml`
- `src/app/qml/Main.qml`
- `src/app/qml/OpenStepDialog.qml`
- `src/app/qml/inspect/InspectWorkspace.qml`
- `src/app/qml/inspect/InspectionDock.qml`
- `src/app/qml/inspect/ContextPanel.qml`
- `src/app/qml/inspect/SectionPanel.qml`
- `src/app/qml/inspect/MeasurementPanel.qml`
- `src/app/qml/inspect/CapturePanel.qml`
- `src/app/qml/inspect/StepViewport.qml`
- `tests/CMakeLists.txt`

- [ ] Expose `System`, `Light`, and `Dark` through one `ThemePreference` object that persists the requested mode with `QSettings`.
- [ ] Bind System mode to `QStyleHints::colorSchemeChanged` so the effective theme follows runtime OS changes.
- [ ] Expand the existing `Theme.qml` singleton into the sole QML theme facade; do not introduce a second theme singleton.
- [ ] Preserve temporary aliases for the existing `Theme.surface`, `Theme.surfaceRaised`, `Theme.onSurface`, and `Theme.accent` API while consumers migrate to semantic roles.
- [ ] Replace feature-local UI and viewer colors with semantic roles.
- [ ] Keep imported STEP and user appearance colors unchanged.
- [ ] Use dark defining edges on white and light defining edges on near-black.
- [ ] Test text and control contrast, focus visibility, viewer edges, selection, hover, and section colors in all modes.
- [ ] Verify capture solid-background choices reflect the active theme.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-theme-preference-tests loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(theme-preference|qml-smoke|qml-inspection-tools)$'
```

### Task 2.2: Centralize viewport and tree context actions

**Create:**

- `src/app/models/ComponentActionModel.h`
- `src/app/models/ComponentActionModel.cpp`
- `src/app/qml/inspect/ComponentContextMenu.qml`
- `tests/app/test_component_action_model.cpp`
- `tests/qml/tst_ComponentContextMenu.qml`

**Modify:**

- `src/app/CMakeLists.txt`
- `src/app/ApplicationController.h`
- `src/app/ApplicationController.cpp`
- `src/app/qml/inspect/InspectWorkspace.qml`
- `src/app/qml/inspect/StepViewport.qml`
- `tests/CMakeLists.txt`

- [ ] Expose one action model with labels, enabled state, tooltip reason, shortcut, and command ID.
- [ ] Select a tree row or body before opening its context menu.
- [ ] Use the same menu component and command dispatch in both surfaces.
- [ ] Preserve assembly-descendant behavior and body-direct behavior.
- [ ] Add keyboard menu invocation and Escape dismissal.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-component-action-model-tests loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(component-action-model|qml-inspection-tools)$'
```

### Task 2.3: Clear selection on empty viewport clicks

**Modify:**

- `src/app/ApplicationController.h`
- `src/app/ApplicationController.cpp`
- `src/app/qml/inspect/StepViewport.qml`
- `tests/app/test_application_controller.cpp`
- `tests/qml/qml_smoke.cpp`

- [ ] Add an explicit clear-selection command rather than assigning the active ID from QML.
- [ ] Clear viewport and tree selection when `pickAt` misses.
- [ ] Preserve accepted measurement picks and measurement result.
- [ ] Make Escape cancel pending measurement interaction before clearing component selection.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-application-controller-tests loupe-qml-smoke-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(application-controller|qml-smoke)$'
```

### Task 2.4: Add the clickable view cube

**Create:**

- `src/app/qml/inspect/ViewCube.qml`
- `tests/qml/tst_ViewCube.qml`

**Modify:**

- `src/app/qml/inspect/StepViewport.qml`
- `src/app/qml/inspect/ViewportNavigation.qml`
- `src/app/CMakeLists.txt`
- `tests/CMakeLists.txt`

- [ ] Bind cube orientation to the camera quaternion.
- [ ] Add six face, twelve edge, and eight corner targets with accessible names.
- [ ] Snap to standard and isometric quaternions without introducing pitch clamps.
- [ ] Preserve navigation target and fit visible geometry after snapping.
- [ ] Support pointer, Tab focus, Enter/Space activation, and Escape.
- [ ] Verify cube contrast and current-face indication in Light and Dark.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(qml-inspection-tools|qml-smoke)$'
```

### Gate 2 checkpoint

- [ ] Run Gate 2 focused tests.
- [ ] Launch the review build and inspect System, Light, and Dark at desktop and compact window sizes.
- [ ] Verify viewport/tree context parity, background deselection, and every view-cube snap.
- [ ] Commit Gate 2 as one shared-interaction slice.

## Gate 3: Exact Section And Topology-Aware Measurement

### Task 3.1: Extend protocol commands for topology and section queries

**Modify:**

- `src/protocol/ProtocolTypes.h`
- `src/protocol/ProtocolJson.cpp`
- `src/protocol/ProtocolFrame.cpp`
- `src/app/worker/WorkerClient.h`
- `src/app/worker/WorkerClient.cpp`
- `src/worker/WorkerServer.h`
- `src/worker/WorkerServer.cpp`
- `tests/protocol/test_protocol.cpp`
- `tests/worker/test_worker_process.cpp`
- `tests/app/test_worker_client.cpp`

- [ ] Add coalescible hover-pick, committed-pick, and exact-section requests.
- [ ] Include camera-ray origin/direction, world-space tolerance, occurrence ID, plane normal/offset, and section generation where required.
- [ ] Return stable topology IDs, topology type, hit point, normal, component identity, highlight payload, and query generation.
- [ ] Reject stale hover and section results without changing presentation.
- [ ] Bound concurrent topology and section work per document.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-protocol-tests loupe-worker-process-tests loupe-worker-client-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(protocol|worker-process|worker-client)$'
```

### Task 3.2: Compute exact per-body section contours and fills

**Create:**

- `src/worker/SectionQuery.h`
- `src/worker/SectionQuery.cpp`
- `src/app/render/SectionGeometry.h`
- `src/app/render/SectionGeometry.cpp`
- `tests/worker/test_section_query.cpp`
- `tests/app/test_section_geometry.cpp`

**Modify:**

- `src/worker/CMakeLists.txt`
- `src/app/CMakeLists.txt`
- `src/app/tools/SectionController.h`
- `src/app/tools/SectionController.cpp`
- `src/app/qml/inspect/SectionPanel.qml`
- `src/app/qml/inspect/StepViewport.qml`
- `tests/app/test_inspection_tools.cpp`
- `tests/CMakeLists.txt`

- [ ] Use OCCT section operations against retained definition shapes transformed into occurrence space.
- [ ] Build stable planar loops, classify holes, and triangulate fills per body.
- [ ] Return exact outline polylines and fill triangles with body identity and resolved source color.
- [ ] Keep mesh clipping active during drag and replace it with exact output only for the committed generation.
- [ ] Add `Outline`, `Filled`, and `Filled + Outline`; default to the approved combined mode.
- [ ] Preserve successful bodies when one body fails and report affected names.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-section-query-tests loupe-section-geometry-tests loupe-inspection-tools-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(section-query|section-geometry|inspection-tools|scene-model)$'
```

### Task 3.3: Add the direct section normal manipulator

**Create:**

- `src/app/qml/inspect/SectionManipulator.qml`
- `tests/qml/tst_SectionManipulator.qml`

**Modify:**

- `src/app/tools/SectionController.h`
- `src/app/tools/SectionController.cpp`
- `src/app/qml/inspect/StepViewport.qml`
- `src/app/qml/inspect/SectionPanel.qml`
- `tests/app/test_inspection_tools.cpp`

- [ ] Project pointer movement onto the section normal and keep a drag-start offset.
- [ ] Update mesh clipping and numeric position continuously.
- [ ] Apply a documented Shift fine-motion multiplier.
- [ ] Commit exact-section work on release or numeric acceptance, not every pointer event.
- [ ] Restore drag-start offset on Escape.
- [ ] Keep the manipulator screen-legible at all zoom levels and theme-aware.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-inspection-tools-tests loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(inspection-tools|qml-inspection-tools)$'
```

### Task 3.4: Separate clipping flip from camera side

**Modify:**

- `src/app/qml/inspect/SectionPanel.qml`
- `src/app/qml/inspect/StepViewport.qml`
- `src/app/qml/inspect/ViewportNavigation.qml`
- `tests/qml/tst_SectionPanel.qml`

- [ ] Keep `Flip section` bound only to half-space selection.
- [ ] Add `View opposite side` as a camera command.
- [ ] Make `View normal to section` and opposite-side viewing fit exact visible section bounds.
- [ ] Verify repeated toggles do not alter plane normal, offset, or clipping side.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^qml-inspection-tools$'
```

### Task 3.5: Resolve stable OCCT topology picks

**Create:**

- `src/worker/TopologyPicker.h`
- `src/worker/TopologyPicker.cpp`
- `tests/worker/test_topology_picker.cpp`

**Modify:**

- `src/worker/CMakeLists.txt`
- `src/worker/WorkerServer.cpp`
- `tests/CMakeLists.txt`

- [ ] Intersect the camera ray with retained occurrence shapes and resolve the nearest face.
- [ ] Resolve edge and vertex candidates within the supplied world tolerance.
- [ ] Generate stable topology IDs from occurrence and indexed subshape identity.
- [ ] Return face highlight mesh, edge polyline, or point position without exposing source geometry outside the local process boundary.
- [ ] Coalesce hover requests and guarantee committed-pick responses are not dropped.
- [ ] Test front/back faces, shared edges, vertices, transformed occurrences, hidden bodies, and no-hit rays.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-topology-picker-tests loupe-worker-process-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(topology-picker|worker-process)$'
```

### Task 3.6: Present hover and numbered accepted measurement picks

**Create:**

- `src/app/render/TopologyHighlightGeometry.h`
- `src/app/render/TopologyHighlightGeometry.cpp`
- `src/app/qml/inspect/MeasurementOverlay.qml`
- `tests/app/test_topology_highlight.cpp`
- `tests/qml/tst_MeasurementOverlay.qml`

**Modify:**

- `src/app/CMakeLists.txt`
- `src/app/tools/MeasurementController.h`
- `src/app/tools/MeasurementController.cpp`
- `src/app/qml/inspect/StepViewport.qml`
- `src/app/qml/inspect/MeasurementPanel.qml`
- `tests/app/test_inspection_tools.cpp`
- `tests/CMakeLists.txt`

- [ ] Add a throttled hover query path that discards stale replies.
- [ ] Render subtle face, edge, or point pre-highlight without changing active component selection.
- [ ] Lock accepted highlights and add screen-space `1` and `2` badges.
- [ ] Identify component, topology type, and coordinates for each accepted pick in the panel.
- [ ] Validate topology combinations per measurement mode while preserving a valid first pick after an invalid second pick.
- [ ] Ensure background deselection preserves accepted picks, Escape clears pending interaction, and `Clear` removes all measurement state.

Run:

```bash
cmake --build build/macos-arm64-debug --target loupe-app loupe-topology-highlight-tests loupe-inspection-tools-tests loupe-qml-inspection-tests -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(topology-highlight|inspection-tools|qml-inspection-tools)$'
```

### Gate 3 checkpoint

- [ ] Run Gate 3 focused tests and inspect exact section and topology fixtures.
- [ ] Confirm interactive section dragging remains responsive while exact results settle after release.
- [ ] Verify accepted measurement highlights remain obvious in both themes and on similarly colored bodies.
- [ ] Commit Gate 3 as one exact-inspection-tools slice.

## Gate 4: Corpus Review And UX Handoff

### Task 4.1: Extend the local review checklist

**Modify:**

- `docs/review/phase-1-ux-review.md`
- `scripts/review/macos.sh`

- [ ] Add explicit review steps for background deselection, all section styles, direct offset drag, opposite-side view, theme switching, topology hover/accepted picks, tree context menu, view cube, and refinement cancellation.
- [ ] Add timing fields for tree, first body, preview, and final readiness.
- [ ] Keep the script limited to Debug and focused UX tests.

### Task 4.2: Run automated review gates

Run:

```bash
cmake --build build/macos-arm64-debug -j4
ctest --test-dir build/macos-arm64-debug --output-on-failure -R '^(protocol|protocol-frame|geometry-payload|worker-process|worker-client|geometry-scheduler|tessellation-profile|geometry-encoder|geometry-cache|application-controller|component-action-model|scene-model|section-query|section-geometry|topology-picker|topology-highlight|inspection-tools|qml-smoke|qml-inspection-tools)$'
scripts/review/macos.sh
git diff --check
```

Expected: every focused test and the UX review smoke gate pass. Do not run packaging, signing, notarization, Release matrix, or export-workspace certification.

### Task 4.3: Run live corpus review

- [ ] Launch with `scripts/run-loupe-debug.sh`.
- [ ] Open each supplied STEP file cold and warm.
- [ ] Confirm the tree appears before geometry, first bodies stream progressively, and refinement progress remains explicit.
- [ ] Confirm the preview remains interactive during refinement and canceling refinement preserves it.
- [ ] Compare curved housing screenshots with Gate 0 for faceting and edge fidelity.
- [ ] Exercise exact section fill and outline modes, direct arrow drag, normal/opposite views, and capture.
- [ ] Exercise hover and accepted face, edge, and point measurement feedback.
- [ ] Exercise System, Light, and Dark, including viewer edges and selection contrast.
- [ ] Exercise viewport and tree context menus and every view-cube face, edge, and corner family.
- [ ] Verify saved PNG dimensions and nonzero image content.
- [ ] Record timing evidence without adding private source geometry to git.

### Task 4.4: Prepare the UX review build

- [ ] Leave the locally launchable Debug app at `build/macos-arm64-debug/src/app/Loupe.app`.
- [ ] Summarize measured performance against the five-second target, including any stage that remains over budget.
- [ ] List functional gaps separately from visual-polish feedback.
- [ ] Commit only the reviewed source, tests, scripts, and documentation; exclude corpus and generated evidence.
- [ ] Hand off the launch command and review checklist to the user.

## Completion Boundary

This plan is complete when the functional review build satisfies the approved acceptance criteria and is ready for user UX review. It does not complete Phase 2 Export, release packaging, signing, notarization, broad private-corpus certification, or final UI redesign.
