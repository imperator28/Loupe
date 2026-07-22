# W8 UI Refresh Acceptance

**Date:** 2026-07-21
**Result:** Pass for the macOS review build
**Authority:** `ui-refresh-strategy.md` W8 and `ui-refinement-handoff.md` section 10

## Automated gates

| Gate | Result | Evidence |
| --- | --- | --- |
| Complete suite | Pass | `ctest --preset macos-arm64-debug --output-on-failure`: 89/89 tests passed in 22.92 s. This includes the 87-test functional baseline plus the two W8 style gates. |
| QML smoke | Pass | Main shell, inspect tools, projection, pointer routing, menus, viewport theme, and the real export-picker checkbox/controller path are exercised. |
| Theme contrast | Pass | `qml-theme-contrast` checks required light and dark text/control pairs. |
| QML style gate | Pass | `qml-style-gates` reports no production hex literals outside `Theme.qml` and no literal numeric animation durations. |
| Build hygiene | Pass | macOS ARM64 debug app builds successfully and `git diff --check` is clean. |

Qt's advisory `qmllint` target was also reviewed. Its remaining diagnostics are dominated by runtime-registered C++ types and intentionally generic injected `QtObject` properties; they are not accepted as substitutes for the executable QML smoke tests above.

## Corpus acceptance walk

Source: `corpus/corpus-a.stp` (2.7 MB)

- Opened through the native file chooser without a startup stall.
- Assembly tree and coarse preview became available before final refinement.
- Refinement displayed stage, percentage, elapsed time, and ETA; the observed in-progress state was 80%, 8 s elapsed, about 1 s remaining.
- Inspect and Export switched repeatedly while retaining the document and preview state.
- Pointer-clicking `INNER-1L86-550` checked the picker, changed the count to one, populated the bucket, and kept export focus/inclusion distinct.
- Typing `/tmp/loupe-w8-export-2` visibly updated the destination and enabled `Export 1 files`.
- STEP output was written and validated: `INNER-1L86-550.step`, 4,821,874 bytes.
- STL output was written and validated: `INNER-1L86-550.stl`, 713,984 bytes.
- No `.partial` or `.tmp` output remained after either export.

Two release-blocking issues found during this walk were fixed before the final run:

1. The export row's full-width tap handler intercepted checkbox pointer input. Row activation is now limited to the component label, and a real-mouse QML smoke test protects the checkbox-to-controller path.
2. Qt could synchronously stall while opening an unnecessary worker working directory on macOS. The worker uses absolute paths, so that configuration was removed and corpus import startup completed normally.

## Handoff section 10 gate walk

### Functional regression

- Pass: complete suite, QML smoke, repeated workspace switching, corpus open/preview/refine/inspect/export, visible destination update, and real STEP/STL validation.

### Interaction

- Pass: automated navigation tests cover wheel zoom, trackpad pan, cursor-pivot orbit restart, and projection behavior.
- Pass: selection/focus/check separation is covered by controller tests and the live pointer-selection walk.
- Pass: section manipulation, topology-specific measurement highlights, and export reorder/fallback behavior remain covered by the inspection, scene, export-controller, and QML suites.

### Visual

- Pass: light and dark viewport/theme captures are readable; menus and controls remain token-driven and content-sized.
- Pass: the export workspace exposes both master and standalone previews before inclusion, with visible bucket and validation status.
- Pass: import/refinement progress is persistent enough to distinguish work from a frozen application.

### Document integrity

- Pass: export continues through the reviewed controller/core/worker plan rather than preview geometry.
- Pass: selected identity, filename, format, and destination matched the bucket in both real exports.
- Pass: output validation succeeded and no partial final files remained.

## Captures

- [Inspect, dark theme](captures/w8-inspect-dark.jpg)
- [Inspect, light theme](captures/w8-inspect-light.jpg)
- [Export, dark theme](captures/w8-export-dark.png)

## Platform note

This W8 execution was performed on macOS ARM64. The token/style gates and cross-platform Qt tests pass here, but a Windows 11 live high-DPI and native-window visual walk remains part of release qualification; it is not represented as completed by this macOS acceptance record.
