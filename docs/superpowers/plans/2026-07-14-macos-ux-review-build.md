# macOS UX Review Build Implementation Plan

**Goal:** Produce a locally launchable Apple Silicon Debug build for Phase 1 UX review without running the later release-validation gate.

**Design:** [macOS UX Review Build Design](../specs/2026-07-14-macos-ux-review-build-design.md)

## Task 1: Make the CLI integration test portable

**Modify:** `tests/core/test_spike_cli.cpp`

1. Keep the existing `CreateProcessW` implementation under `_WIN32`.
2. Add a POSIX implementation using `posix_spawn`, a pipe, and `waitpid`.
3. Share output trimming, final-line JSON parsing, and CLI assertions across platforms.
4. Use a platform helper for temporary-directory process IDs.
5. Build `loupe-core-tests` on macOS and run the CLI cases from this file.

The POSIX path must pass arguments directly without invoking a shell and must report pipe, spawn, read, and wait failures clearly.

## Task 2: Add the focused macOS review runner

**Create:** `scripts/review/macos.sh`

**Modify file modes:**

- `scripts/bootstrap/macos.sh`
- `scripts/run-loupe-debug.sh`
- `scripts/verify/macos.sh`
- `scripts/review/macos.sh`

The review runner will:

1. verify the macOS prerequisites;
2. configure the `macos-arm64-debug` preset;
3. build the Debug tree;
4. run CLI portability, QML smoke, and inspection-tool tests selected by an explicit CTest regular expression;
5. launch through `scripts/run-loupe-debug.sh` when passed `--launch`;
6. otherwise print the launch command for the reviewer.

It must not configure Release, run the full CTest matrix, execute private-corpus certification, collect benchmarks, or update gate evidence.

## Task 3: Provide the UX review checklist

**Create:** `docs/review/phase-1-ux-review.md`

Document:

- the one-command preflight and launch paths;
- the representative-model prerequisite;
- the eight approved task-based review steps;
- pass, issue, and not-tested result states;
- blocker, workflow, visual hierarchy, wording, and deferred-validation categories;
- a compact finding template;
- the validation work intentionally deferred until UX approval.

## Task 4: Verify the review slice

Run:

```bash
VCPKG_ROOT="$HOME/vcpkg" cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug --output-on-failure -R '<review smoke expression>'
scripts/review/macos.sh
```

Then launch Loupe locally and confirm that the Inspect shell reaches an interactive state without an immediate crash. Do not perform the full manual UX review on behalf of the user and do not close the Phase 1 gate.

Record exact automated results in the handoff. Any failure in build, CLI integration, QML smoke, inspection tools, or launch blocks the review build and is fixed within this slice when the fix remains narrowly scoped.
