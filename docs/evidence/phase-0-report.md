# Phase 0 backend evidence report

Date: 2026-07-12
Evidence commit: `62e84780f7a121bcb9f7d610df9d8bb8240d53d6`

## Platform verification

Windows x64 (MSVC 19.44.35228, OpenCASCADE 8.0.0 through the pinned vcpkg manifest) passed both `windows-debug` and `windows-release`: 62 tests passed per configuration. Two symlink-escape tests were skipped because the current Windows user does not have symlink-creation privilege; the protections remain covered where the privilege is available.

Apple Silicon is `hardware_not_yet_available`. The repository includes isolated `macos-arm64-debug` and `macos-arm64-release` presets plus bootstrap and verification scripts. The first M2 development session must create the macOS platform row and rerun this gate.

## Mandatory private corpus

The two required corpus entries are represented only by generic IDs and source hashes.

| Case | Source hash | Unit decision | Four export/read-back rows | Repeat run | Source immutability |
|---|---|---|---|---|---|
| `required-case-01` | `8e40198e402c66efccb44364eb4c9c68` | mm, confirmed | PASS | PASS | PASS |
| `required-case-02` | `4d3eeb385ac913befe23fc961031704c` | mm, confirmed | PASS | PASS | PASS |

Each row comprises definition STEP/local, occurrence STEP/assembly, definition STL/local, and occurrence STL/assembly. The proof builds expectations from the selected source BRep and its pre-export mesh, then independently reopens/parses exported files. Invalid or bypassed full-flow corpus entries fail with a nonzero exit code.

Release import benchmark: case 01 = 2145 ms; case 02 = 985 ms. These are measured reference values, not performance targets.

## Gate decision

Gates A–D are complete on Windows: model import/unit integrity, selected STEP/STL export, read-back/evidence, both required corpus cases, and structural Apple Silicon readiness all passed. Phase 1 may begin; add the macOS evidence row immediately when the M2 Pro is available, then require both platforms for every subsequent vertical gate.
