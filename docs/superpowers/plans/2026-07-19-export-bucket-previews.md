# Export Bucket and Dual Preview Implementation Plan

**Design:** `docs/superpowers/specs/2026-07-19-export-bucket-previews-design.md`

**Goal:** Replace the plan-oriented Export workspace with two linked CAD previews, a checkbox-driven file bucket, configurable naming, automatic validation, and worker-backed STEP/STL execution.

## Task 1: Model hover, pin, and ordered bucket state

**Files:**

- Modify: `src/app/export/ExportWorkspaceController.h`
- Modify: `src/app/export/ExportWorkspaceController.cpp`
- Modify: `tests/app/test_export_workspace_controller.cpp`

- [ ] Add independent hovered and pinned node IDs with a resolved preview node.
- [ ] Replace unordered checked storage with stable bucket order.
- [ ] Expose bucket rows independently from the filtered component picker.
- [ ] Suppress redundant raw `SOLID` and `COMPOUND` rows when represented by named components.
- [ ] Preserve hover, pin, and bucket independence in focused tests.

## Task 2: Add naming strategies and automatic validation

**Files:**

- Modify: `src/app/export/ExportWorkspaceController.h`
- Modify: `src/app/export/ExportWorkspaceController.cpp`
- Modify: `src/core/export/ExportPlan.h`
- Modify: `src/core/export/ExportPlan.cpp`
- Modify: `tests/app/test_export_workspace_controller.cpp`
- Modify: `tests/core/test_export_plan.cpp`

- [ ] Add keep-name, sequential-base, and prefix-original naming modes.
- [ ] Generate zero-padded sequential names from bucket order.
- [ ] Support per-row filename overrides and bucket reordering.
- [ ] Rebuild internal validation automatically after relevant changes.
- [ ] Project exact output filenames and row blockers to QML without exposing fingerprints.

## Task 3: Make the CAD viewport reusable for Export

**Files:**

- Modify: `src/app/qml/inspect/StepViewport.qml`
- Create: `src/app/qml/export/ExportPreview.qml`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/qml/qml_smoke.cpp`

- [ ] Add presentation-only configuration that hides Inspect tools and shortcuts.
- [ ] Add an external X-ray node highlight that renders through assembly bodies.
- [ ] Add display filtering so a standalone preview renders only one component.
- [ ] Automatically fit when the standalone preview target changes.
- [ ] Keep master and standalone camera navigation independent.

## Task 4: Build the master-first Export workspace

**Files:**

- Replace: `src/app/qml/export/ExportWorkspace.qml`
- Create: `src/app/qml/export/ExportComponentPicker.qml`
- Create: `src/app/qml/export/ExportBucket.qml`
- Create: `src/app/qml/export/ExportSettings.qml`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/qml/qml_smoke.cpp`

- [ ] Implement the approved left picker, large master preview, standalone preview, and bucket rail.
- [ ] Update previews on row hover and fall back to the pinned row on pointer exit.
- [ ] Make row clicks pin without changing checkbox state.
- [ ] Synchronize checkbox and bucket remove actions.
- [ ] Replace Build output plan with automatic row validation and `Export N files`.
- [ ] Add destination-folder selection, naming controls, and inline blockers.

## Gate 1: Export UX review

- [ ] Launch the local review build with a corpus assembly.
- [ ] Review X-ray clarity, standalone fit, hover stability, bucket behavior, naming feedback, and light/dark themes.
- [ ] Revise the UX before broad validation.

## Task 5: Extend worker protocol for export execution

**Files:**

- Modify: `src/protocol/ProtocolTypes.h`
- Modify: `src/protocol/ProtocolJson.cpp`
- Modify: `src/worker/WorkerServer.cpp`
- Modify: `src/app/worker/WorkerClient.h`
- Modify: `src/app/worker/WorkerClient.cpp`
- Modify: `tests/protocol/test_protocol_json.cpp`
- Modify: `tests/worker/test_worker_process.cpp`
- Modify: `tests/app/test_worker_client.cpp`

- [ ] Add export request, row progress, row result, and batch completion messages.
- [ ] Execute validated rows against the worker-owned native import result.
- [ ] Use the existing atomic STEP and STL exporters.
- [ ] Cancel between rows and preserve completed outputs.
- [ ] Report failures per row without discarding successful outputs.

## Task 6: Wire export execution to the workspace

**Files:**

- Modify: `src/app/ApplicationController.h`
- Modify: `src/app/ApplicationController.cpp`
- Modify: `src/app/export/ExportWorkspaceController.h`
- Modify: `src/app/export/ExportWorkspaceController.cpp`
- Modify: `src/app/qml/export/ExportBucket.qml`
- Modify: `src/app/qml/export/ExportWorkspace.qml`
- Modify: `tests/app/test_application_controller.cpp`
- Modify: `tests/app/test_export_workspace_controller.cpp`

- [ ] Submit the current automatically validated bucket on `Export N files`.
- [ ] Show overall progress and queued, exporting, complete, skipped, failed, and cancelled row states.
- [ ] Add cancellation and failed-row retry.
- [ ] Confirm existing-file behavior before conflicting rows execute.

## Task 7: Focused verification and review build

- [ ] Build `loupe-app`, `loupe-worker`, export controller tests, protocol tests, worker integration tests, and QML smoke tests.
- [ ] Run only focused Export tests before UX approval.
- [ ] Export a small STEP bucket and STL bucket into a temporary directory and reopen representative outputs.
- [ ] Copy and sign the updated binary into the local review bundle.
- [ ] Launch the review build and document remaining UX findings.

Do not stage private or generated CAD files:

```text
controller-worker.step
repeated-box.step
corpus/*.stp
```
