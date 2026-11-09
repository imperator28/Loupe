# Changelog

## v0.1.6-beta.6

A testing build from feedback on beta.5. Published as a pre-release; everything in
v0.1.6-beta.5 is included.

### Changed

- **Sections about an axis**: picking a hole or shaft sections square to its axis, where
  it was picked; the position slider moves it along the axis. "Through the axis" in the
  Section panel turns a plane containing the axis to any angle about it.
- **Axis to face**: surface to surface measures from an axis to a flat face parallel to it,
  square across (a hole's centre to a datum face).
- **Explode lines** run through the middle of each part along the way it moved. Only a real
  round feature (a hole, pin or boss) the part moved along lines it on an axis; fillets no
  longer pull lines to edges. Where a part stands in front of a line, the line shows faintly
  through it instead of disappearing.
- **The Explode panel's line controls** have room: the style sits on its own row, and the
  part's line buttons on two.
- **Export**: clicking a part in the bucket selects it in the component tree and the master
  view, and the standalone preview keeps it shown and lit.

### Fixed

- **Picking a face with a section on**: a click went to the part the section had cut away,
  so a flat face could be refused as curved. Clicks now go through what is cut away, and
  picking a face plane turns the section off while picking (back on if cancelled).
- **Measurements after resizing the window** stayed where they were until the view was
  turned; they follow the resize now.
- **5x capture greyed out with no reason**: a scale out of reach now says why (the capture
  would be wider or taller than 16,384 px at this view's size) and what the most is.

## v0.1.6-beta.5

A testing build that picks the centerlines of round features, draws explode lines, adds
touch support, selects every part with Cmd/Ctrl+A, and puts measurements back into
captures. Published as a pre-release; everything in v0.1.6-beta.4 is included.

### New

- **Centerlines** (#40): hover a round face (a hole, boss, pin or fillet) and its axis is
  drawn as a dash-dot centerline over the feature and every coaxial face of the part: a
  counterbore and its hole are one axis. The axis is exact, read from the CAD geometry.
  - Point to point measures to it: square from a point, between two parallel axes, or the
    closest approach of two skew ones. Angle takes it as a straight edge.
  - A pivot picked on an axis lines the handle up with it, to slide along a shaft or turn
    about a hinge.
  - Section cuts through a round face's axis, facing you, or across it with Alt.
  - Alt takes the face (or a point on it) instead of the axis, and the other way round.
    Corners, midpoints and hole centres still snap first.
- **Explode lines** (#41): a thin dashed line from where each moved part seats to where it
  is now, drawn in the scene (parts in front hide it) and in captures.
  - A pin in its bore or a screw in its hole is lined along the shared axis, as a centerline.
  - In the Explode panel: lines on or off, dashed or centerline; for the selected part, pick
    where its line starts (a corner, a hole's centre or an axis), go back to its centre, or
    hide its line. These are saved with explode layouts.
- **Touch** (#42), on Windows touchscreens and touch Macs:
  - One finger orbits, two fingers pan, pinch zooms, and twisting rolls.
  - Fingers get more slop before a tap becomes a drag, and bigger targets.
  - A double tap fits the part, or everything on empty space.
  - Press and hold opens the menu. In Measure, Section and pivot picks it aims instead:
    lift to take what is shown.
  - Not yet tried on touch hardware.
- **Cmd/Ctrl+A** selects every part shown in Inspect, and checks every part in Export.
- **What's New lists every release since yours**: releases down the left, the chosen one's
  highlights on the right. An update through the app over several releases marks each one
  new, and earlier releases stay a click away (or Up and Down, or the button to the older
  one). The 0.1.6 highlights are rewritten shorter.

### Fixed

- **Capture with measurements** (#43): "Include active measurement" put nothing in the
  image since the measure rework. The measurements are back, where they are on the model,
  at any capture size.
- **Measure drew outside the viewport** (#39): a highlighted edge running off the view went
  on over the panels beside it, and a line passing behind the camera was drawn mirrored.
  Both are cut where they should be.
- The first open of each file after updating rebuilds its cache, since round faces now
  carry their axis.

## v0.1.6-beta.4

A testing build that opens the launch file sooner, exports assemblies with repeated parts,
restyles the view toolbar, sections along a face you pick, snaps the exploded view's pivot,
fixes in-app updates of a copy in `C:\Program Files` on Windows,
and lets pinned measurements be dragged and coloured. Published as a pre-release;
everything in v0.1.6-beta.3 is included.

### New

- **Opens the file you launched with sooner**: its import starts as Loupe starts, not once
  the window is up, and the window's first frame already shows the model in the remembered
  theme, without a dark-to-light flash. Export and Drawing are built when first opened (or
  in the background a few seconds in), and on Windows the graphics driver starts while the
  window is built.
- **Section along a face you pick**: Section > Pick face plane asks for a face in the view.
  The face under the pointer is lit, with an arrow showing which way its plane faces; a
  curved face says it has no plane to cut along. The face picked stays lit while the section
  uses it, and the panel names it ("Plane of: Flat face on Bracket-2"). Esc cancels.
- **The pivot snaps** (exploded view > Pick a point): to corners, edge midpoints, hole and
  arc centres, and points on edges, with the same marker as measuring shows before the
  click. A hole's centre turns the handle to the hole's axis, and the panel says what the
  pivot is on. Point-to-point measuring snaps to edge midpoints too.
- **The view toolbar is restyled**: a compact floating bar with outline icons over short
  labels, every tool the same width; the lit tool sits on an accent wash. Tools, order and
  shortcuts are unchanged.
- **Pinned measurements, laid out your way**: drag a pinned tag anywhere in the view; its
  leader keeps pointing at what was measured as the view turns, and double-clicking puts it
  back. Each pin takes a colour, in turn, and its swatch in the panel picks another (amber,
  blue, teal, rose, violet, slate).
- **Measurement tags are cards**: the theme's surface and text with an outline in the
  result's colour, instead of white on brown, so they read in both themes.
- **Bounds reads as a box**: its faces are shaded faintly under the dashed edges, with its X,
  Y and Z sizes on three of them.

### Fixed

- **Exporting an assembly with repeated parts**: Select all in Export refused the whole
  batch when a part or sub-assembly was placed twice, or two parts shared a name. Every
  copy of a part is now one file, written once; different parts that share a name are
  numbered "-2", "-3" in tree order, and a name you typed is never changed. The bucket
  shows one row per file, with "×N" for copies.
- **The section arrow ran off the cut** when dragged quickly: the cut moved by a pixel scale
  taken at the press, which drifts in perspective, and nothing kept it on the model. The cut
  now goes where the pointer is along the section axis, held a little past the model's ends.
- **Clicking a part moved a face-plane section** to that part's plane. The plane in use now
  changes only when another is picked.
- **The view cube held up the first frame** on Windows: its labels asked for a font
  Windows does not have, and the search for a stand-in blocked drawing for up to a second.
- **The bounds box drew only its sizes** on Windows in beta.3, without the box: its dashed
  edges never appeared. Measurement lines are now drawn the way the app's icons are, which
  every graphics back end draws.
- **Adding to a measurement with Shift or Ctrl/⌘** did nothing in beta.3: edge length and
  area did not add up, and volume and bounds did not take more parts. The click passed the
  modifier keys on as a number where a yes or no was expected.
- **Updating a copy in Program Files, again** (Windows): the update still left the old version
  in place. Before asking for administrator permission, the update first tried the copy as
  you, and Windows refused each of the package's ~2500 files only after two retries, five
  times over: hours, in a hidden window, before the prompt. It now checks once whether it can
  write to Loupe's folder and, when it cannot, asks for permission straight away. The update
  panel says beforehand that Windows will ask.

Coming from beta.2 or beta.3 installed in Program Files: those versions still run the old
update, so install this beta once by hand (unzip it over the Loupe folder as an
administrator). Updates after this one work in place.

## v0.1.6-beta.3

A testing build of the reworked Measure tool, filled section caps, and saved exploded
layouts. Published as a pre-release; everything in v0.1.6-beta.2 is included.

### New

- **Pin measurements to the view**: Pin keeps a result drawn where it was measured, its
  leader on the geometry as the view turns, while you measure the next. Any number, of
  any kind, for a review or a capture; unpin one or all from the panel.
- **Angle shows what it is between**: an arc from one face or edge to the other, with a
  leg along each, and its value on the arc. It is the angle the two open at, so a V-groove
  reads as its opening, with the supplement alongside.
- **Edges and faces add up**: in Edge and Area, Shift or Ctrl/⌘-click more to total them;
  the same one again takes it back out. Picked edges are drawn bold, so it is clear which
  edge is measured.
- **Volume and bounds of anything**: of the selection (a part, a sub-assembly, several),
  or of the whole model when nothing is selected. Bounds draws the box with its X, Y and Z
  sizes on its edges.
- **Selectable**: a transparent part stays selectable. Properties > Selectable in the view,
  or the right-click menus, makes a part let clicks through to what is behind it.

- **Measure, shown before and after**: the face or edge a click would take lights up, and a
  chip by the pointer names it ("Flat face · 240.00 mm²") or says why the mode cannot use
  it. Point to point snaps to vertices, circle centres and edges. The result is drawn where
  it was measured (a dimension line, a radius leader, a tag), the picks are numbered in the
  view and the panel, and the panel gives ΔX/ΔY/ΔZ, diameter or supplement, how the value
  was found, and Copy.
- **Saved exploded layouts**: save a layout to come back to in one click, save over it,
  rename or delete it, in the Exploded view panel and under Views > Exploded.

### Fixed

- **Surface to surface** measured between the two click points. It is now the gap square
  to two parallel flat faces, or the true minimum distance between any two faces.
- **Angle** was between the normals at two click points, so a curved face gave whatever its
  normal was there. It is between flat faces or straight edges now, and a curved one is
  refused with the reason.
- **Section caps**: a solid part could show hollow when cut. Caps were built per colour, so
  a part with any face of another colour was cut as open shells; a hairline gap left a
  contour open; and a bore was filled over. Caps are built per part, small gaps are closed,
  holes stay open, and dense sections build 10 to 30 times faster.
- **Exploded view, turning a part**: dragging a ring far flung the part away, because the
  turn followed a pivot that moved with every step. The pivot is fixed when the drag
  starts, and successive turns of a lopsided sub-assembly no longer creep it away.
- **Exploded view, Assembled**: the handles disappeared on the assembly, and Exploded
  brought the old layout back, so the assembled positions could not be edited from.
  Moving a part on the assembly now starts a new layout; the old one is saved first.
- **Transparent parts could not be clicked**: making a part transparent also made it
  unselectable in the view. Those are separate choices now.
- **Volume and bounds of a sub-assembly** gave nothing: only a single part was measured.

## v0.1.6-beta.2

A testing build that fixes in-app updates on Windows when Loupe is installed in
`C:\Program Files`. Published as a pre-release; everything in v0.1.6-beta.1 is included.

### Fixed

- **Updating a copy in Program Files** (Windows): the update downloaded and Loupe
  restarted, but it was still the old version, with no error, because Windows refused the
  copy into a folder only an administrator may change. The update now asks for
  administrator permission when it needs it, through the usual Windows prompt, and Loupe
  starts again as you.
- **An update that could not be installed says so**: if the copy still fails (the prompt
  was declined, or a file was held open), the old version says which version could not be
  installed, into which folder, and what to do, instead of offering the same update as if
  it had never been tried. macOS does the same.

Coming from 0.1.5 installed in Program Files: 0.1.5 still runs its old update, so install
this beta once by hand (unzip it over the Loupe folder, or run Loupe as an administrator
for this update). Later updates work in place.

## v0.1.6-beta.1

A testing build of faster opening and of drawing repeated parts once, with a fix for
assemblies that use a sub-assembly more than once. Published as a pre-release.

### Faster

- **Opening large assemblies**: each part's volume, area and face measures are worked out
  once instead of for every copy and every pass, mass properties run alongside meshing
  instead of holding it up, and parts are meshed on every core. On a 4-core machine a
  27.7 MB assembly that took 36 s to open fully takes 22 s, and a 12-part 1.5 MB one
  0.94 s instead of 1.35 s.
  Times to the first view are unchanged: reading the STEP file still runs on one core.
- **Repeated parts**: every copy of a part is drawn from one shared shape, sent and loaded
  once. An assembly of 24 identical stages opens about five times faster in the app and
  uses a third less memory.
- **Windows and macOS allocator**: the import worker uses mimalloc, about 20% faster at
  opening on Windows and 3-7% on Apple Silicon.

### Fixed

- **Sub-assemblies used more than once**: every copy shared the first copy's parts, so
  only one copy was shown (the others drawn over it) and exporting a part in any copy gave
  the first. Each copy is now its own, in the tree, the viewport and exports.

## v0.1.5

Tabs and multiple windows, an exploded view, in-app updates, and a broad interface
polish. Built from eleven betas and five release candidates; each one's notes are on
its GitHub pre-release.
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
  Capture, Views and Exploded view open in small windows that can be dragged or
  collapsed to their title bar.
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
  - The Inspect toolbar sets the buttons that act on the model apart from those that
    open a tool window, lights a tool while its window is open, and drops to icons when
    the viewport is narrow, so it never runs over the side panels.
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
