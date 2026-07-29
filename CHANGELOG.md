# Changelog

## v0.1.3

The first release with the **Drawing** workspace, plus a rebuilt view cube, an
Intel macOS build promoted to the tagged release path, and a projection fix that
made silhouettes work on Apple Silicon at all.

### 2D drawing export (new)

A third workspace projects a selected part to a 2D drawing for laser cutting,
waterjet, machining quotes, and documentation. Projection runs on the CAD
document through hidden-line removal rather than on display triangles.

- Export to **DXF**, **SVG**, or **PDF**. DXF opens correctly in a real
  downstream consumer.
- Three content modes: **Cut** (outer profile plus through-holes), **Silhouette**
  (only where material ends — step, chamfer, and fillet lines dropped), and
  **Technical** (every visible edge, on separate layers).
- Choose the view from the six standard views, by clicking a flat face in the 3D
  preview, or by clicking a view-cube face. The face is highlighted while
  picking, and a degenerate view is reported rather than silently exported; a
  degenerate view can be recovered by tilting, which is disclosed as inexact.
- **1:1 is exact.** Any other ratio is named in the filename so it cannot be
  mistaken at the cutter. An optional scale fiducial can be embedded.
- Queue several drawings, review the batch, and execute it in the worker.
  Colliding generated names are numbered rather than the batch being refused.
- Sticky 2D preview with a navigable canvas, bounded panning, hover previews,
  and queue thumbnails. The preview renders offscreen, and `src/core` stays
  Qt-free.

> **Known limitation.** Projection is not yet production-reliable and the
> workspace ships so it can be exercised on real parts. `loupe-spike
> drawing-audit <file.step>` measures two open defects: Cut mode frequently
> emits **unclosed** contours (on sample assemblies, most contours in a view),
> and on some parts the Silhouette bounding box comes out **larger** than the
> Cut outline it filters. Verify output before cutting. Tracked as Gate E in the
> drawing export plan.

### Viewport and navigation

- A real rotating 3D view cube replaces the flat Top/Front/Right buttons, with
  labels printed on the faces, axis arrowheads, and hover indication of the face
  under the cursor. Clicking a face keeps the current zoom.
- **Alt + left drag** rotates the view in its own plane.
- Trackpad panning and touch gestures.
- Standard view names resolve against the document up axis.
- `1` / `2` / `3` reach every viewport, and the previously ambiguous
  display-mode shortcut is resolved.

### Fixes

- **Silhouette projection on Apple Silicon.** A footprint point lying exactly on
  a triangle edge was classified as outside, because a tolerance-free sign test
  met arm64 FMA rounding. Sample points on a shared edge were dropped and the
  region was discarded, so silhouette mode failed on parts where it worked on
  x86-64. Now classified with a scale-relative tolerance. **v0.1.2 ships this
  bug.**
- **Hidden parts stayed selectable** (macOS and Windows). A ray pick still
  reports a model that is not drawn, so a hidden component kept absorbing the
  clicks and hovers meant for whatever sat behind it. Picking now follows the
  user's own hide, while Edges Only and 2D-slice modes stay selectable.
- **Trackpad pinch drifted the viewport** (macOS). macOS delivers a pinch as a
  native gesture whose centroid moves as the fingers close, and a pan was being
  derived from it. Windows was never affected: it delivers a trackpad pinch as
  Ctrl+wheel.
- **Export and Drawing did not frame the selected part on entry** (macOS). The
  fit raced an asynchronous geometry replay, and the request was dropped rather
  than retried when it lost. Windows consistently won that race.
- **Export and Drawing disagreed on the right-hand column width** (macOS).
  A `ScrollView` is a `Control`, so its implicit width and scroll-bar padding
  are platform-dependent; the width is now pinned on a plain layout item.
- Windows launched with a stray console window alongside the app.
- Several rendering-cost regressions on the cube hover path, and mesh bounds
  moved off the camera-move path.
- Export split ratios, the rename field, queue thumbnails stuck on placeholders,
  and the assembly-tree reveal.

### Packaging and versioning

- **Intel macOS** (`loupe-v0.1.3-macos-x64.zip`) is built on every tag, not only
  on manual dispatch. GitHub's Intel runners are retired, so it is an x86_64
  build produced on an Apple Silicon runner under Rosetta 2.
- The **About dialog, the macOS bundle version, and the release tag now agree**.
  A `verify-version` CI job fails a tag whose version disagrees with
  `project(Loupe VERSION)` before anything builds. On macOS,
  `CFBundleShortVersionString` was defaulting to `MAJOR.MINOR`, so Finder
  reported `0.1` while the About dialog reported the full version; both plist
  keys are now pinned to the project version.
- Windows carries no version resource in the executable at all — recorded in
  [portability.md](docs/development/portability.md) rather than fixed, since the
  resource compiles only on Windows.

### Documentation

- `docs/development/portability.md` gains a **Known platform divergences**
  section: the version surfaces, the two routes trackpad gestures take, the
  asynchronous-replay race, and the `Control` implicit-sizing difference — so a
  one-platform fix is not mistaken for a two-platform bug.

## v0.1.2 and earlier

See the [release history](https://github.com/imperator28/Loupe/releases).
