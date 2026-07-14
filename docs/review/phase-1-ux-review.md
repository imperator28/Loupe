# Phase 1 UX Review

Use this review to decide whether Loupe's Inspect workflow is understandable and worth refining. This is not the Phase 1 release, geometry-accuracy, performance, or cross-platform evidence gate.

## Prepare

Have one representative STEP assembly available. Prefer a model whose basic structure you already understand so the session can focus on Loupe rather than discovering the source data.

From the repository root, prepare the Debug build and run the narrow smoke gate:

```bash
scripts/review/macos.sh
```

Launch after the smoke gate passes:

```bash
scripts/run-loupe-debug.sh macos-arm64-debug
```

For a combined preflight and launch:

```bash
scripts/review/macos.sh --launch
```

## Review Session

Try the tasks in order without referring to implementation documentation. Mark each task `Pass`, `Issue`, or `Not tested`.

| Task | Result | Notes |
|---|---|---|
| Launch Loupe and open the representative model |  |  |
| Understand loading progress and source-unit state |  |  |
| Navigate the assembly tree and correlate selection with the viewport |  |  |
| Select, isolate, ghost, hide, and restore components |  |  |
| Create, understand, and clear measurements |  |  |
| Enable, position, reverse, and disable a section plane |  |  |
| Capture the current inspection state |  |  |
| Reopen or switch models and understand resulting state changes |  |  |

Stop after these workflows have been attempted. Do not turn this session into performance profiling or exhaustive geometry verification.

## Record Findings

Use one category per finding:

- `Blocker`: prevents a primary review task or crashes the application.
- `Workflow`: the task can be completed but its sequence or state is unclear.
- `Visual hierarchy`: controls, state, selection, or feedback are difficult to perceive.
- `Wording`: a label, message, or unit explanation is misleading or unclear.
- `Deferred validation`: plausible concern that belongs to geometry, performance, packaging, or dual-platform verification after UX approval.

Copy this block for each finding:

```text
Category:
Severity: High | Medium | Low
Task:
Observed:
Expected:
Evidence: screenshot or short note
```

## Review Decision

Choose one outcome:

- `UX approved`: the Inspect workflow is coherent enough to begin full validation.
- `Revise`: record a bounded list of interaction changes for the next review build.
- `Blocked`: a crash or missing primary workflow prevents meaningful review.

## Deferred Until UX Approval

- Full macOS Debug and Release test matrices
- Private-corpus geometry and inspection certification
- Exact picking, measurement, section, capture, and cache evidence
- Performance, memory, frame-time, and cache-reopen benchmarks
- Windows regression verification
- Signing, notarization, DMG, installer, and update behavior
- Phase 1 evidence matrix and gate-decision updates
