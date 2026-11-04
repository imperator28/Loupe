# Changelog

## v0.1.5

Tabs and multiple windows, an exploded view, in-app updates, and a broad interface
polish. Built from eleven betas; each beta's notes are on its GitHub pre-release.
Opening native NX, Creo and SOLIDWORKS files (#5) waits on a decision about a
commercial import SDK and is not in this release.

### New

- **Tabs and windows**: each STEP file opens in its own tab, keeping its view, selection,
  section and export queue. Drag a tab along the strip to reorder it, off the strip into a
  new window, or onto another Loupe window, which marks where it will land.
- **Open STEP files from the OS**: double-click or Open With on Windows and macOS, and
  files sent to a running Loupe open as tabs in it. On Windows, File → Register Loupe for
  STEP Files… adds it to Default Apps.
- **Exploded view** (`E`): pull an assembly apart to see how it fits together.
  - Drag a part or sub-assembly by its arrows (along X, Y, Z), its ball (freely, in the
    screen plane) or its rings (to turn it). Moves snap to a step; Shift moves freely.
    Several parts move together.
  - **Assembled / Exploded** compares the two states in one click, and **Spread** sets
    how far apart auto-explode takes the parts, stacked layers included.
  - Under **Advanced**: exact offsets and angles, move by a distance, snapping steps,
    and a picked pivot (optionally aligned to its face), which stays with that part.
  - Undo and redo each move. Section and view normal to face work on the layout.
  - **Export…** saves the layout as a STEP assembly, one STL, or a DXF/SVG/PDF drawing of
    the current view at true scale. The source file is never changed.
- **Saved views** (`V`): a Views window keeps snapshots of the model to come back to.
  - **Display** states hold what is hidden or transparent, with each part's opacity and
    edges.
  - **Camera** views hold the angle, zoom and projection, and the camera glides back to
    them.
  - Each has a thumbnail and a name you can edit. **Reset** returns to the file as it
    opened.
  - They are kept per file on this computer, and the file is never changed.
- **Opening time**: Document properties shows how long the file took to be ready to
  view, to show every part, and to be fully detailed.
  - **Opening History…** lists every open with its size and part count, marks opens
    from the cache, and copies as CSV to compare with other CAD programs.
- **Export the assembly without hidden parts**: File → Export Assembly Without Hidden
  Parts… writes one STEP file with the tree, names, placements, colours and unit kept.
  A card at the bottom right shows each stage and the progress.
- **Find a part** (`Ctrl/Cmd+F`) in the assembly tree, which is now resizable, with
  Expand all and Collapse all. It shows hidden and transparent parts.
- **View normal to face** (`Ctrl/Cmd+8` or right-click): look straight at a face, or a
  hole's axis; ask again for its other side.
- **Transparency you control**: once a part is transparent, the menus offer Make opaque.
  Properties has an **Opacity** slider (45% by default) and **Show edges when
  transparent**.
- **In-app updates**: a download icon in the toolbar, with a ring that fills as the
  update downloads. **Restart to Update** replaces Loupe in place, keeps your settings,
  and avoids macOS's open prompt. A new version that fails its self-check is never
  installed. Help → Include Beta Versions offers pre-releases.
- **What's New**: the first launch of a new version lists its highlights. Help → What's
  New in Loupe… shows them again.
- **About Loupe**: shows the release channel for a beta or release candidate, and links
  to this version's release notes, What's New, Check for Updates, all releases, the
  source and licences. Copy Version Info is there for bug reports.
- **Workspace layout**: every side panel is resizable and remembered. Measure, Section,
  Capture and Exploded view open in small windows that can be dragged or collapsed to
  their title bar.
- **Output folder beside the STEP file** by default, for both Export and Drawing.

### Improved

- **Part colours**: cool and warm palettes now spread in hue and chroma, not just
  lightness, so neighbouring parts stay distinct under shading. A high-contrast style
  is also available.
- **Section**: much faster to finalize on large assemblies. View normal to section now
  looks at the cut face.
- **Perspective**: clip planes fit the model, which removes shading artefacts on large
  parts. Edges stay solid at any zoom.
- **Interface**:
  - A System / Light / Dark switch with icons.
  - Drawn icons and check marks everywhere instead of text characters.
  - Menus sized to their entries.
  - One style for every panel title.
  - Highlights inset per the design guide.
- **Export bucket**: renaming edits the name only, keeping the `.step` or `.stl` suffix.
- **Interaction guide** (View → Interaction): keys drawn as key caps, with mouse and
  trackpad pictures, and a section for the exploded view.
- **Import time estimate** follows measured time and counts down instead of growing.

### Fixed

- Leftovers of a Loupe that crashed or was force-quit (importer copies, connections, a
  stale update download) are cleared on the next launch.

- Right-clicking a part kept in a multi-selection no longer drops the selection.
- Assembly tree selection works as in file lists: ⌘-click (Ctrl-click on Windows) adds
  or removes a row, and Shift-click selects every row between, or adds them with ⌘. The
  tree had read no modifiers at all, so every click was a plain one.
- Trackpad pinch and wheel zoom keep the point under the pointer fixed (macOS).
- The appearance swatch shows the colour covering most of a multicoloured part.
- macOS: no crash on launch on newer macOS. Builds run on macOS 15 and support 14.
- Windows: the colour picker no longer draws a grey block over the view. Export and
  Drawing previews keep the camera in front of the model.

### Known limitations

- **2D drawings**: projection is not yet production-reliable. Check output before
  cutting (see the README).
- Builds are not code-signed or notarized; see the README for first-run steps.

## v0.1.4

Fixes and additions from review of 0.1.3, tested through three pre-releases.

### New

- **Automatic part colours** (View → Part Colours, or the document properties
  panel with nothing selected): cool shades, warm shades, muted, or high
  contrast, with Shuffle. The first three are low-contrast by design; high
  contrast uses saturated hues with alternating lightness, for when telling
  parts apart matters more than calm. Applies to parts without file colours, and
  to files that give every part the same default colour; optionally to all
  parts. Display only.
- **Semi-transparent parts** instead of hiding them: **T** (or the toolbar's
  Transparent button, or right-click) toggles the selection; **Shift+T** makes
  everything else transparent. Transparent parts let clicks through to the part
  behind. Works in Inspect and in Export's master preview; Show All undoes it.
- **The display mode follows you** between Inspect, Export and Drawing.

### Fixes

- **Inch STEP files exported 25.4x too small.** OCCT reads every file into a
  millimetre document, and the unit policy took that for a conflict with the
  file's declared inches, falling back to millimetres with a 1.0 factor. The
  viewer happened to survive it; STEP, STL and drawing export did not (a 0.5 in
  part was written as 0.5 mm). Inch files now resolve as inches, and display,
  measurement, export and drawing share one scale, so they cannot disagree.
  Inch files now show **Units: in**; measurements follow.
- **Switching to Export or Drawing rebuilt the model.** Each arriving mesh
  re-walked every model in the scene, so building N parts cost N²; the replay
  also re-decoded geometry viewports already held, and the single-part previews
  built every part to show one. Measured on the viewport's own work at 4000
  segments: 72 s to build → 0.46 s, 167 s to replay → near zero, 44 s for the
  standalone preview → 0.07 s.
- **A part did not render in its swatch colour.** A face pointed at the viewer
  received ~1.5x its colour plus a specular hot spot (#3070C0 drew as #9BC0FF).
  Lighting is rebalanced so a facing face renders within a level or two of its
  colour. File colours were also read as OCCT's linear values and shown as if
  they were sRGB, so every file colour looked darker and more saturated than in
  the CAD tool (SolidWorks' #CAD1EE showed as #97A3DA); they now match.
  Uncoloured parts no longer arrive painted with a teal placeholder,
  and a subassembly's swatch shows the colour its parts are drawn in.
- **Circles in drawings became polygons.** A face-picked view carried the
  display mesh's float error, and OCCT keeps a circle a circle only when the
  view is parallel to its axis to 1e-12 rad; the view now snaps to the face's
  exact normal (or a hole's axis). Obliquely seen circles are drawn as arcs
  within tolerance rather than hundreds of segments, so DXF keeps real arcs, and
  an edge-on circle is a single line rather than fourteen overlapping segments.
  A hole stored as one closed spline (as ECAD part libraries write them) is now
  recognised as a true circle in every format. The 2D preview no longer
  flattens small arcs, so a 0.9 mm hole no longer shows as an octagon.
- **Open contours in Silhouette mode.** A misread outer region could leave the
  outline open, and the result depended on memory layout -- the same part
  could come out closed on one run and open on the next. Both fixed. Where the
  projection engine fails to produce an outline curve at all, the preview now
  says the silhouette falls short instead of passing a wrong outline.
- **The Windows package could fail to start** with "no Qt platform plugin could
  be initialized" (0.1.4-beta.2). Qt's plugins now sit directly beside
  `Loupe.exe`, and every build launches the packaged app from its own folder,
  with nothing from the build machine on its path, before it is published.

> **Still open.** Cut-mode contour closure (Gate E) is unchanged by this
> release; check drawings before cutting.

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
