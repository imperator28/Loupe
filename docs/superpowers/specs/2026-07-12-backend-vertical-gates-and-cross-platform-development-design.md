# Backend Vertical Gates and Cross-Platform Development Design

## Purpose

Loupe is an engineering tool whose first obligation is trustworthy geometry and output. Development therefore proceeds through backend vertical gates before shell and workspace implementation. The process must remain fast enough to iterate, but it must not trade away unit correctness, assembly fidelity, deterministic selection, export integrity, or read-back evidence.

Windows 11 is the active development environment today. Once the Apple Silicon M2 Pro station joins development, macOS becomes an immediate required platform for every subsequent vertical gate and may become the primary daily-development environment. Windows remains an equal release requirement.

## Operating Principles

- One platform-neutral source tree, protocol contract, corpus contract, and evidence schema.
- Test-first implementation for correctness-sensitive behavior.
- Focused tests for individual changes; full matrices at vertical-gate closure.
- Integrated spec and quality review per vertical gate rather than per small helper task.
- A gate cannot close while it has a known corruption, silent-wrong-output, crash, privacy, nondeterminism, unbounded-resource, required-corpus, or cross-platform-equivalence defect.
- Lower-risk hardening that does not threaten a current gate becomes explicit Phase 3 backlog work.
- Logical commits are made at reviewed vertical slices. Major backend tasks must not accumulate indefinitely in one uncommitted worktree.

## Review and Verification Cadence

### Per Change

- Add or update a focused failing test before implementation.
- Run the focused Debug test set on the active development platform.
- Run static checks relevant to the touched module.

### Per Vertical Gate

- Run the complete Debug test suite.
- Run the complete Release test suite when the gate touches units, geometry, import/export, concurrency, evidence, packaging, or another optimization-sensitive boundary.
- Perform one integrated contract review followed by one code-quality review.
- Retain named evidence for every required corpus case.
- Commit the reviewed slice before starting the next gate.

### Dual-Platform Operation

Before the M2 Pro is available, Windows provides the executable gate evidence and macOS readiness is checked structurally through portable code boundaries and Apple Silicon presets. From the first macOS development session onward:

- Every subsequent vertical gate requires Windows 11 and macOS Apple Silicon Debug and Release evidence.
- A failure on either platform keeps the gate open.
- Expected platform differences must live behind an explicit adapter and have equivalent contract tests.
- Geometry, unit decisions, stable IDs, output fingerprints, classifications, and validation outcomes must be equivalent across MSVC and Apple Clang.

## Platform Architecture

Cross-platform core code owns geometry, units, import/export, validation, domain values, worker protocol, and evidence. Operating-system behavior is isolated behind narrow adapters for:

- atomic file replacement;
- worker process creation, cancellation, and crash reporting;
- platform paths and local caches;
- diagnostics and system integration;
- packaging, signing, notarization, and update mechanisms.

The repository provides:

- `windows-debug` and `windows-release` CMake presets;
- `macos-arm64-debug` and `macos-arm64-release` CMake presets;
- equivalent PowerShell and POSIX bootstrap/verification entry points;
- a pinned dependency baseline and documented Qt/OCCT/compiler versions;
- UTF-8 and case-sensitive path tests;
- `.gitattributes` rules that prevent line-ending and filename drift;
- platform-labelled evidence using a shared schema.

Build trees, dependency caches, generated outputs, private corpus geometry, and machine-specific configuration remain local and untracked. Portable corpus metadata and redacted evidence may be committed.

## Phase 0: Backend Feasibility and Evidence

Phase 0 contains backend work only. The browser prototype is not a Phase 0 completion dependency.

### Gate A: Model Integrity

- STEP/XCAF import and classification.
- Declared and effective unit policy, including explicit overrides.
- Assembly, subassembly, definition, occurrence, and placement fidelity.
- Stable deterministic identities.
- Fail-closed errors for unresolved unit or hierarchy conditions.

### Gate B: Output Integrity

- Immutable selection and export plans.
- STEP and binary-millimetre STL output.
- Local and assembly coordinates applied exactly once.
- Correct scale and output-unit behavior.
- Atomic, replace-safe output creation.
- Mandatory read-back validation for units, bodies, bounds, centroid, shape validity, and STL orientation.

### Gate C: Required Corpus Proof

Both current private STEP corpus files are mandatory acceptance cases. Neither may pass Phase 0 by being classified as unsupported.

Each file must independently demonstrate:

- successful import and classification;
- coherent or explicitly resolved units;
- deterministic hierarchy and stable IDs across repeated runs;
- representative definition and occurrence selection;
- selected STEP export and read-back;
- selected STL export and read-back;
- correct bodies, bounds, centroid, placement, scale, and mesh orientation;
- no source-document mutation;
- no private geometry or source path leakage into evidence;
- stable named failures and reproduction data for any intermediate defect discovered before closure.

A failure in either file keeps Phase 0 open and becomes the next backend engineering task.

### Gate D: Backend Evidence and Commit

- Complete Debug and Release verification.
- Resource and performance measurements for the required corpus.
- Redacted retained evidence for every case.
- One final backend contract and quality review.
- Logical commits for model integrity, output integrity, validation/CLI, and gate evidence.
- A clean worktree and reproducible build instructions.

## Phase 1: UX Contract and Inspect Shell

Phase 1 begins only after the backend Phase 0 gate is committed. It starts with the fixture-driven browser interaction prototype, then implements the cross-platform Qt Inspect shell, isolated worker, progressive assembly tree and mesh presentation, source-unit review, measurement, sectioning, capture, and local cache.

The browser prototype validates interaction structure without becoming an alternative product architecture. The Qt shell consumes versioned backend protocol values rather than native OCCT objects.

After macOS development begins, every Phase 1 vertical gate is required on Windows and macOS.

## Phase 2: Selective Export Workspace and Pilot

Phase 2 builds the dual-view Export workspace on the proven backend: independent checked state and highlight, component list, context and isolated viewers, reviewed immutable plan, worker execution/cancellation, manifests, recovery UX, and internal workflow measurement.

Pilot output is accepted only when the same reviewed plan produces equivalent validation outcomes on Windows and macOS.

## Phase 3: Hardening and Release

Phase 3 closes pilot regressions, enforces performance and resource budgets, adds worker recovery and redacted diagnostics, completes accessibility and DPI behavior, and automates packaging and release evidence for both platforms.

## Coworker Distribution Contract

Loupe must not require administrator access for normal installation or use.

### Windows

- Default distribution is a signed per-user installer or portable package.
- Per-user installation targets a user-writable application directory such as `%LocalAppData%`.
- Qt, OCCT, the MSVC runtime, plugins, and other required runtime assets are bundled.
- Normal launch, file opening, inspection, export, cache use, and update checks do not require elevation.
- Machine-wide installation, managed deployment, or corporate policy exceptions may require IT administration, but they are optional deployment modes.

### macOS

- Distribution is a signed and notarized universal or Apple-Silicon application in a DMG or equivalent package.
- A user may install to `~/Applications` without administrator access.
- Installation into `/Applications` may prompt according to workstation policy, but Loupe itself does not require elevated runtime privileges.

The release gate includes a clean-user-account installation test on each platform. Development tools, vcpkg, Qt SDKs, and compilers must not be prerequisites for coworkers.

## Migration Gate to macOS

Before substantial development transfers to the M2 Pro, a clean macOS clone must:

1. bootstrap pinned dependencies without Windows artifacts;
2. configure both Apple Silicon presets;
3. build and run the complete backend suite;
4. reproduce stable classifications, IDs, unit decisions, and output validation for shared generated fixtures;
5. run the private corpus from local untracked storage;
6. produce the same redacted evidence schema as Windows;
7. document any approved platform adapter differences.

Once this gate passes, either platform may be used for daily development, while both remain mandatory at vertical-gate closure.
