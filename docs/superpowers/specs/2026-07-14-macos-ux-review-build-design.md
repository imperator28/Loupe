# macOS UX Review Build Design

**Date:** 2026-07-14

## Purpose

Prepare Loupe as a locally launchable Apple Silicon Debug build for focused Phase 1 UX review. The review cycle should expose broken workflows and interaction problems without spending time on release-grade validation before the UX is approved.

## Scope

This slice will:

- remove the Windows-only build failure in the CLI integration test;
- build and launch Loupe locally from the macOS Debug tree;
- run a narrow automated smoke gate before each review round;
- provide a repeatable manual review checklist and feedback record;
- separate UX approval from the later Phase 1 validation and evidence gate.

This slice will not add signing, notarization, a DMG, an installer, update behavior, performance certification, private-corpus certification, or a Phase 1 completion claim.

## Cross-Platform CLI Test Harness

`tests/core/test_spike_cli.cpp` keeps one shared test contract and uses a small platform boundary for process execution.

- Windows retains the existing wide-character `CreateProcessW` path.
- macOS and other POSIX targets launch the exact executable and argument vector without a shell, capture combined stdout and stderr through a pipe, and collect the child exit status.
- Temporary directory naming obtains the process identifier through the same platform boundary.
- JSON parsing and all CLI assertions remain shared so both platforms exercise equivalent behavior.

The implementation stays in the test harness. It does not add process APIs to production core code or add Qt as a new test-only dependency.

Process setup, launch, read, or wait failures throw a descriptive exception. A signaled POSIX child is reported as a nonzero result rather than being mistaken for successful CLI output.

## Local Review Build

The review artifact is the existing `macos-arm64-debug` build tree, not a distributable package. `scripts/run-loupe-debug.sh` remains the canonical launcher because it supplies the Qt plugin, QML import, and dynamic-library paths required by the vcpkg build.

The macOS shell scripts documented as directly executable will receive executable file modes. The review workflow will use the repository scripts rather than requiring reviewers to reproduce environment variables manually.

## Validation Strategy

Validation is split into three deliberate stages.

### Stage 1: Pre-Review Smoke Gate

Run only enough automation to establish that the review session is useful:

1. Configure and build the Apple Silicon Debug preset.
2. Run the CLI integration tests affected by the portability fix.
3. Run the existing QML smoke and inspection-tool tests.
4. Launch the app locally and confirm that it reaches the Inspect workspace without an immediate crash.

A failure in build, launch, file opening, or a primary Inspect workflow blocks UX review. Compiler warnings that do not affect the review path are recorded but do not expand this slice.

### Stage 2: Manual UX Review

Use one representative STEP assembly and complete a short task-based session:

1. Launch Loupe and open the model.
2. Understand loading and unit state without external explanation.
3. Navigate the assembly tree and correlate tree selection with the viewport.
4. Exercise select, isolate, ghost, hide, and restore behavior.
5. Create and clear measurements.
6. Enable, position, reverse, and disable a section plane.
7. Capture the current inspection state.
8. Reopen or switch models and observe whether state changes are understandable.

The reviewer records findings under five categories: blocker, workflow, visual hierarchy, wording, and deferred validation. Each finding includes the task, observed behavior, expected behavior, and severity. The session stops when the primary workflows have been attempted; it is not an exploratory performance or geometry-accuracy campaign.

### Stage 3: Post-Approval Validation

After the UX is approved, run the work intentionally deferred from the review loop:

- full macOS Debug and Release build/test presets;
- private-corpus geometry and inspection checks;
- measurement, section, picking, capture, and cache accuracy evidence;
- launch, interaction, frame-time, memory, and cache-reopen benchmarks;
- Windows regression verification;
- Phase 1 evidence matrix and gate-decision updates.

## Review Deliverables

The implementation produces:

- a macOS-buildable cross-platform CLI test;
- a successfully built local Debug app;
- a working one-command local launcher;
- a concise UX review checklist and feedback template;
- exact commands and results for the narrow smoke gate;
- an explicit list of deferred Phase 1 validation work.

## Acceptance Criteria

This slice is ready for user review when:

- the portability test compiles and its CLI cases pass on macOS;
- the selected Debug smoke tests pass;
- the app launches locally through the repository script;
- the manual checklist can be completed without developer-only setup steps;
- no release-readiness or Phase 1 completion claim is made.

UX approval means the reviewer accepts the Phase 1 Inspect workflow or records a bounded revision list. It does not imply geometry, performance, release, or dual-platform certification.
