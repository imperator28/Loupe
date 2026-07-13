# Phase 1 Inspection Evidence

Commit: `<git revision>`
Manifest baseline: `<vcpkg baseline>`

## Gate C: inspection workflow

| Requirement | Windows 11 x64 | macOS Apple Silicon | Evidence |
|---|---|---|---|
| OCCT measurement modes return geometry-derived values | pending | pending | test IDs and corpus case IDs |
| Section plane/face, flip, cap, and 2D slice are presentation-only | pending | pending | test IDs and capture IDs |
| PNG capture honors alpha, scale, and overlay options | pending | pending | output hashes |
| Cache invalidates source/importer/profile/unit changes | pending | pending | test IDs |
| Cached snapshot appears before mesh refinement | pending | pending | timing JSON |

## Gate D: dual-platform matrix

| Preset | Build | Tests | Corpus inspection | Benchmark | Notes |
|---|---|---|---|---|---|
| windows-debug | pending | pending | pending | pending | |
| windows-release | pending | pending | pending | pending | |
| macos-arm64-debug | pending | pending | pending | pending | native M-series hardware required |
| macos-arm64-release | pending | pending | pending | pending | native M-series hardware required |

## Required benchmark fields

Record one JSON object and CSV row per source hash with shell-ready, file acknowledgement, tree-ready, coarse-view, first-interaction, cached-reopen, selection latency p50/p95, frame time p50/p95, peak memory, and idle CPU. Never include private paths, geometry, or exported artifacts.

## Decision

`OPEN` until all rows above are supported by fresh native evidence. Cross-compilation and an unexecuted macOS script are not macOS validation.
