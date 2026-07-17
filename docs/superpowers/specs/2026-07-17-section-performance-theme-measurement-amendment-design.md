# Section Performance, Theme, and Measurement Amendment

**Date:** 2026-07-17

**Status:** Approved

This amendment refines the approved topology-aware CAD workflow after UX review of the first camera and section-manipulator build. It covers interactive section performance, complete theme readability, and topology-correct measurement feedback. It does not broaden numerical validation beyond what is needed for UX review.

## Goals

- Keep the section handle visually attached to the cursor without accumulating delayed geometry updates.
- Preserve an exact capped or 2D section result after interaction ends.
- Make every dialog, selector, field, and toggle readable in System, Light, and Dark themes.
- Pre-highlight and persist the actual point, edge, or face used by a measurement.
- Keep validation focused on interaction correctness until the UX is approved.

## Interactive Sectioning

Section dragging uses a two-phase CPU pipeline.

While the pointer is down, the manipulator position updates immediately. Geometry updates are coalesced so only the newest requested offset is processed, and preview refreshes are frame-limited. The preview clips visible bodies but omits cap generation and expensive 2D outline reconstruction. This prevents stale pointer events from forming a multi-second rebuild queue.

When the pointer is released, Loupe performs one exact rebuild at the final offset. The rebuild restores the selected cap state and the selected Outline, Filled, or Filled + outline 2D presentation. Numeric offset controls use the same preview/commit contract.

The section controller exposes an explicit interaction state and separates preview offset changes from committed changes. Render geometry must never infer dragging from timing alone.

## Theme Controls

Native control defaults are not relied on for themed panels. Loupe provides shared themed controls for combo boxes, spin boxes, switches, radio buttons, text fields, and dialog actions. Each component defines text, placeholder, disabled, focus, selection, popup, indicator, and background colors from the active semantic theme.

The controls are applied consistently to Section, Capture, Context, Material, and related dialogs. Focused QML tests verify readable foreground/background contrast and popup delegates in both Light and Dark themes.

## Measurement Feedback

Measurement selection follows the topology-aware contract in the parent design. Worker geometry carries stable face and edge identifiers plus render ranges. Picking resolves the visible entity under the cursor instead of reducing every hit to a surface point.

- Point mode displays a point marker.
- Edge length and radius modes pre-highlight and persist the complete edge.
- Face area and face-based distance or angle modes pre-highlight and persist the complete face.
- Accepted picks remain visible and numbered until the measurement is cleared or replaced.
- Ordinary inspection clicks do not create measurement markers.

The first UX review validates correspondence between the highlighted entity and the reported measurement. Broad metrology and tolerance validation remains a later gate.

## GPU Diagnostics

Loupe does not force a software renderer in production. Runtime verification uses Qt scene-graph diagnostics:

```sh
QSG_INFO=1 QT_LOGGING_RULES='qt.scenegraph.general=true;qt.rhi.general=true' /path/to/Loupe
```

The output must identify the selected QRhi backend and adapter. On the review Mac, Loupe reports the Metal backend on Apple M2 Pro with software-device preference disabled. Automated QML tests may intentionally use the software backend and are not evidence of production renderer selection.

## Verification Scope

- Drag a section handle continuously across a representative large assembly and confirm the handle remains immediate and no delayed update queue continues after release.
- Confirm the final released result matches the requested cap and 2D presentation.
- Inspect all affected controls and popup states in Light and Dark themes.
- Hover and accept point, edge, and face measurements and confirm the rendered entity type matches the measurement mode.
- Run focused controller, geometry, and QML tests. Defer exhaustive geometry accuracy and performance benchmarking until the UX gate is approved.
