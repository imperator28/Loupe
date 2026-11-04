# Changelog

## v0.1.5-beta.11

- **Modern check marks**: checkboxes and checked menu entries draw their tick instead of
  using the "✓" character, which each platform drew in its own symbol font -- heavy and
  dated on Windows. The tick is now the same clean 2 px mark everywhere.

## v0.1.5-beta.10

- **Section finalizes much faster on large assemblies**:
  - Parts the plane does not touch, usually most of them, finish at once instead of each
    going through a background build.
  - The parts it does cut are built six at a time instead of one after another.
  - The viewport no longer recounts every part each time one finishes, which made
    finishing N parts cost N².
  - Each build measures every vertex's distance to the plane once, and skips triangles
    wholly on one side before any work.
- **Transparent parts keep their edges**: edges stay on by default.
  - For a clean ghost, Properties now has **Show edges when transparent**, under Opacity.
- **Rename keeps the suffix**: in the export bucket only the name is edited, and `.step` or
  `.stl` stays fixed after it. Clicking into the field no longer clears it.
- **Panel titles match**: every side panel's heading (Assembly tree, Components, Parts,
  properties, Export bucket, Drawing setup) is one style, at one height, with its count
  on the right.
  - Counts read "1 file" rather than "1 files".
- **Colour picker without the grey block**: it opens as a raised card over the app.
  - On Windows, Qt's dimmer had drawn as a grey rectangle in the middle of the view.
- **Icons**: the exploded view's ring icon has its arrowhead joined to the arc.
  - The export bucket's and drawing queue's arrows, close buttons and grip are drawn
    icons instead of text characters.

## v0.1.5-beta.9

- **Solid edges again**: beta.8's fitted clip planes made the pull that keeps edges in
  front of their faces tiny, so on Windows edges broke into dashes. The pull is now
  worked out from the planes and is the same small distance in the model at any zoom.
- **Export and Drawing views draw on Windows**: an orthographic camera now always sits
  in front of the model, and the clip planes fall back to the old fixed range whenever
  they cannot be worked out. A camera inside the model left a near plane behind it,
  which Direct3D did not draw.
- **A picked pivot stays with its part**: in the exploded view, a pivot picked for a
  part (or a set of parts) comes back whenever those parts are selected again, until
  **Use centre** clears it.
- **Where a dragged tab will land**: dragging a tab over another window shows a pulsing
  bar in that window's tab strip, at the place the tab will be put.
- **Cool and warm part colours tell parts apart**: each family now spans a band of hues
  and a little more chroma instead of mostly lightness, which the model's shading
  masks.
  - Cool runs from teal through steel blue to violet; warm from terracotta through sand
    to olive.
  - The colours are picked by farthest-point sampling in OKLab, so consecutive parts are
    at least about 0.075 apart (some four just-noticeable differences, up from 0.04).
    They stay well short of the vivid style's contrast.
- **Assembly export shows its progress**: exporting without hidden parts shows a card at
  the bottom right with the stage (preparing, translating to STEP, writing the file),
  the percentage and a progress bar.
  - Then it shows the result: a success clears itself, and a failure stays until closed.
  - The card uses the app's neutral surface instead of a red or blue outline.
- **Theme switch**: System / Light / Dark is a three-position switch with icons (a
  display, a sun, a moon) instead of a drop-down, with each name on hover.

## v0.1.5-beta.8

- **Update indicator in the toolbar**: a newer release shows as a download icon with a
  dot, replacing the bar under the toolbar.
  - While it downloads, a ring around the icon fills with the percentage beside it.
  - Click the icon for the version, the size downloaded, and Download, Cancel,
    **Restart to Update** or Later.
  - Help > Check for Updates… opens the result, "up to date" included.
- **Transparency you can see and set**:
  - When the whole selection is transparent, the menus offer **Make opaque** instead
    of making it transparent again, and the dock's Transparent button stays lit.
  - Every row of the assembly tree has a transparency toggle beside its eye. It is
    always shown for a transparent or partly transparent part or assembly, as hidden
    ones are.
  - **Opacity** in Properties, under Appearance: a transparent part starts at 45%
    and can go anywhere from 10%. All the way right makes it opaque again.
- **Right-click menus fit their entries**: hidden entries, such as the exploded-view
  ones when nothing is exploded, no longer leave gaps, and dividers are an inset line.
  A menu is as wide as its longest entry, so none is cut short. The menu over empty
  space gains **Save picture…**.
- **Export and Drawing side panels go narrower**: they now really go down to the same
  minimum as Inspect's. The settings column was holding itself at about twice that.
- **Perspective without shading artefacts**: the near and far clip planes now fit the
  model instead of spanning a fixed 0.01 mm to 1 km. Large models such as the door
  panel no longer show stripes and shades in perspective.
- **Consistent icons and highlights**: every control that used a text character as an
  icon now draws a real icon in the app's style. This covers ×, ▸ ▾, ⊞ ⊟ and the
  window's – ▢.
  - A tool window collapses to its title bar with a chevron and has a flat surface,
    without the gradient.
  - Exploded view's **Assembled / Exploded** is a segmented switch, and its legend
    icons share one colour and weight.
  - Highlights sit clear of the edges around them: menu bar titles are a rounded chip
    rather than the bar's full height, and the tree's buttons are inset in their row.
  - Menu rows are 26 px, per the design guide.
  - The Drawing part list highlights the row under the pointer, as the other lists do.
  - The Export standalone preview hides its view cube until it shows a part.
- **Interaction guide keys** are a plain letter in a rounded outline.
- The [impeccable](https://github.com/pbakaus/impeccable) design skill is part of the
  project, for design work in Claude Code.

## v0.1.5-beta.7

- **Auto-explode takes stacked parts apart**: parts that share a centre, such as
  layered panels, tapes and liners, used to move together and stay stacked. Siblings
  that overlap are now also pushed apart along the axis they overlap least, which for
  layers is the stacking direction, in order.
- **Exploded view, simpler**: an **Assembled / Exploded** switch compares the two
  states in one click and keeps the layout. The first Exploded spreads the parts.
  - A **Spread** slider sets how far apart the parts go.
  - Pictures explain the handles.
  - Exact offsets, move-by, snapping and the new pivot sit under **Advanced**.
  - Measuring works whenever the view shows Assembled.
- **Pick a pivot**: under Advanced, click a point on a part and the handle moves there,
  so parts move and turn about it, for example a hinge. The pivot stays on the part as it
  moves. Optionally the handle lines up with the face clicked.
- **Several parts move together**: Shift-click or Ctrl/⌘-click parts, then drag; the
  panel says how many are selected.
- **Tool panels are windows**: Measure, Section, Capture and Exploded view open in a
  small window.
  - Drag it by its title bar anywhere in the workspace; it never goes off screen.
  - Minimise it to its title bar with the – button or a double-click.
  - It scrolls when the workspace is too short for it.
- **Section: view normal looks at the cut**: "View normal to section" now looks from the
  removed side, at the cut face. "View opposite side" keeps the other half and looks at
  its cut. Both used to look at the uncut outside.
- **Narrower side panels in Export and Drawing**: the part list now goes down to 180 px
  and the settings and preview panel to 260 px, the same as in Inspect.
- **Updates from GitHub**: each launch, Loupe checks for a newer release.
  - When there is one, a bar under the toolbar offers it, downloads it with a progress
    bar, and **Restart to Update** puts it in place of this copy and opens it.
  - Settings are kept, and the downloaded app is not quarantined, so macOS does not ask
    again whether to open it.
  - Help > Check for Updates… checks on demand, and Help > Include Beta Versions chooses
    whether betas are offered (on by default when running a beta).
  - The new version is started once with its self-check before anything is replaced,
    and a failed copy puts the old version back.
- **Interaction guide with real keys**: keys are drawn as key caps (⌘ ⇧ ⌥ on a Mac), and
  mouse buttons and trackpad gestures as pictures. There is one column per section, so
  nothing is cut off, and a new Exploded view section.

## v0.1.5-beta.6

- **Exploded view: move freely** (#17): drag the ball in the middle of the handle to move
  the selection anywhere in the plane of the screen. Snapping steps each axis; Shift skips
  it. The readout shows the move along X, Y and Z.
- **Exploded view: move by an exact distance**: type X, Y and Z under "Move by" and press
  Move (or Enter). It adds to where the selection is, works for several parts at once, and
  is one step to undo. The absolute fields are now labelled Offset.
- **Export the exploded layout** (#17, phase 2): Export… in the exploded-view panel, or
  File > Export Exploded Assembly…, saves the assembly as it is laid out, without its
  hidden parts. The format follows the file's suffix:
  - `.step`: the assembly tree, names and colours kept, every part where it was moved.
  - `.stl`: one mesh of the whole layout.
  - `.dxf`, `.svg`, `.pdf`: a drawing of the layout seen from the current view, with
    the screen's up as its up, at true scale, every visible edge drawn, and read back
    before it counts as written.

## v0.1.5-beta.5

- **Exploded view** (#17): pull an assembly apart in Inspect to see how its parts fit
  together.
  - Open it with the new Explode button, or `E`. Select a part, or a sub-assembly in
    the tree, and drag the handles: the arrows move it along X, Y or Z, and the rings
    turn it about its centre.
  - A sub-assembly moves as one piece, and a part moved inside it stays put relative to
    it. A readout shows the distance or angle while dragging.
  - Moves snap to a step (1, 5, 10 or 25 mm; 5, 15, 45 or 90°). Hold Shift to move
    freely, or type exact positions and angles.
  - Undo and redo (`Ctrl/Cmd+Z`, `Ctrl/Cmd+Shift+Z`) go one move at a time. "Select
    parent" moves the whole sub-assembly a clicked part is in.
  - **Auto-explode** takes the selected sub-assembly, or the whole model, apart from its
    centre, level by level, with a slider for how far.
  - **Reset** glides every part home, from the panel or the right-click menu; one part
    or sub-assembly can be reset on its own.
  - Parts stay where they were put until reset, across workspace switches and when the
    tab moves to another window, for the session.
  - **Section** cuts the exploded layout as it is drawn, with each part's cut faces on
    the moved part, and **view normal to face** follows a turned part.
  - The file and its exports are never changed. Measuring, which works on the assembled
    model, is off while parts are moved.

## v0.1.5-beta.4

- **Every side panel can be resized**: drag the divider beside the assembly tree
  or the component properties in Inspect, the component list or the settings and
  previews in Export, or the part list or drawing setup in Drawing. Each holds
  between a minimum and a maximum, the 3D view keeps a usable width, and a
  smaller window narrows the panels and gives the width back when it grows.
  Widths are remembered, Export and Drawing share theirs so the two still line
  up, and a double click restores the default.
- **The appearance swatch matches a multicoloured part**: it shows the colour
  covering most of the part, not whichever face colour loaded last.
- **The tree's eye button is centred** in its row.
- **macOS: no crash on launch on newer macOS**. Qt 6.11.1 crashed while turning
  an image into a macOS one, such as the app icon at startup, on macOS releases
  that no longer hide its bug. The macOS build now carries Qt's own fix from
  6.11.2. macOS builds run on macOS 15 machines and still support macOS 14.

## v0.1.5-beta.3

- **Drag a tab into another window** (#15): drop it on another Loupe window's
  toolbar or tabs and the document moves there, at the drop position. A
  window's last tab can go too, and that window then closes, as in Chrome.
  Files opened from the OS now go to the window you used last.

## v0.1.5-beta.2

Fixes from testing beta 1 (#11-#16).

- **Tabs drag** (#15): drag a tab along the strip to reorder it, or drag it off
  the strip and let go to move that document into a new window. It moves as it
  is, with no reload. Closing a window closes its tabs.
- **Hidden parts show in the tree** (#14): a hidden part is dimmed with an
  eye-off marker, an assembly with some parts hidden shows a faint one, and each
  row's eye button hides or shows it.
- **The display style is remembered** (#13) for files opened afterwards,
  including after a restart; the default is Solid + Edges.
- **Clearer visibility menus** (#11): *Hide others* (the same as Isolate) is
  gone, and the two *Restore* items now read *Undo visibility change* and
  *Exit isolate*.
- **The tree's right-click menu opens at the cursor** (#12), not above it.
- **The tab close button's highlight stays inside the tab** (#16).

## v0.1.5-beta.1

A testing build of the tickets raised after 0.1.4 (#2, #3, #4, #6, #7, #8, #9,
#10) and two fixes reported against it. Published as a pre-release. #5 (opening
native NX, Creo and SOLIDWORKS files) needs a decision on a commercial import
SDK and is not in this build.

### New

- **Tabs** (#9): each STEP file opens in its own tab, with its own worker, view,
  selection, section and export queue, all kept while you work in another tab.
  A file goes into the current tab if it is empty, else a new one. Close with
  the tab's ×, a middle-click or Ctrl/Cmd+W (closing a tab mid-export asks
  first); switch with Ctrl+Tab.
- **Open STEP files from the OS** (#8): Loupe opens a file passed on the command
  line (Windows "Open with" and default-app launches) and files sent by macOS
  Finder, and declares `.step`/`.stp` in its macOS bundle so Finder offers it.
  On Windows, File → Register Loupe for STEP Files… adds it to Open With and
  Default Apps (per-user, no admin rights). A second launch while Loupe runs
  hands its files to the running window as new tabs instead of opening a second
  window.
- **Export the assembly without hidden parts** (#7): File → Export Assembly
  Without Hidden Parts… writes the whole assembly as one STEP file with its
  tree, names, placements and colours, leaving out every part you have hidden
  (find them with search, then Hide or Isolate). A subassembly left empty is
  dropped, shared parts stay shared, and the file keeps the source's unit.
  Typical use: strip fasteners or purchased parts before passing a model on.
- **View normal to face** (#10): right-click a face, View → Normal to Face, or
  **Ctrl/Cmd+8** over a face. The view turns to look straight at it, centred on
  the face, keeping the zoom; a flat face uses its exact normal, a hole or
  cylinder its axis, and the view is squared to the face's longest straight
  edge. Asking again shows the face's other side.
- **Find a part by name** (#4): a search field above the assembly tree
  (**Ctrl/Cmd+F**). Matches as you type, ignoring case; the match is selected,
  its branch expanded and scrolled to, and it is highlighted in the viewport.
  Enter / Shift+Enter step through the matches, with a count.
- **A readable assembly tree** (#2): drag the tree panel's right edge to widen
  it (remembered; double-click to reset); deep levels indent less, names are
  left-aligned, a cut-off name shows in full on hover, and Expand all /
  Collapse all buttons sit above the tree. The tree model also no longer scans
  every node for each row it draws, which made large assemblies slow.
- **Output defaults to a folder beside the STEP file** (#3): Export suggests
  `<file name> export/` and Drawing `<file name> drawings/` next to the opened
  file (under Documents/Loupe if that folder is read-only). The folder is only
  created when the first export runs; Browse still picks any folder, which then
  holds until the next file is opened.

### Fixes

- **Right-click dropped a multi-selection** (#6). Right-clicking a part that is
  already selected now keeps the selection, so Hide, Isolate and Make
  transparent apply to every selected part, and the menu says how many
  ("Hide 5 selected"). Right-clicking empty space no longer clears it.

- **Pinch-to-zoom shifted the view (macOS trackpad).** Zoom scaled about the
  viewport centre, or in perspective toward the last orbit pivot -- an invisible
  point that is off-centre after orbiting about a picked face -- so the part
  under your fingers slid away. Pinch and wheel zoom now keep the point under the
  pointer fixed, and trackpad scrolling reported during a pinch no longer pans.
- **The import time estimate was wrong and grew while you waited.** It was
  recomputed every second from fixed progress milestones, so whenever progress
  paused the estimate rose (16 s became 17 s two seconds later), and the
  milestones gave translation 45% of the bar and mesh refinement 19%, when they
  take about 21% and 50% of the time. The bar now follows measured time shares
  (translation reports real progress, refinement is weighted per part by its
  preview time), and the estimate counts down between updates instead of
  growing. On a 200-part, 16 s import it now stays within about 1.5 s of the
  truth throughout; before, it read 2 s with 10 s still to go.

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
