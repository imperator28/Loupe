<p align="center">
  <img src="assets/branding/loupe-app-icon.svg" width="152" alt="Loupe application icon">
</p>

<h1 align="center">Loupe</h1>

<p align="center">
  A lightweight, engineer-oriented STEP inspector for fast CAD review, measurement, sectioning, materials, split-part export, and 2D drawing projection.
</p>

<p align="center">
  <a href="https://imperator28.github.io/Loupe/">Website</a> · <a href="https://github.com/imperator28/Loupe/releases/latest">Download</a>
</p>

## Status

Loupe is a functional desktop review build under active development. It is designed to make a typical engineering inspection workflow feel immediate: open a STEP file, understand the assembly, inspect geometry and mass, capture evidence, then prepare only the parts a vendor or prototype shop needs — as 3D files, or as 2D drawings a cutter can run.

The work is organised into three workspaces: **Inspect** for review and measurement, **Export** for split-part 3D output, and **Drawing** for 2D projection. Inspect and Export are the mature paths; Drawing is newer and its projection is not yet production-reliable — see the limitation noted under [2D Drawing Export](#2d-drawing-export).

The desktop targets are Windows 11 x64, Apple Silicon macOS, and Intel macOS. Geometry and export operations use the imported CAD document rather than treating display meshes as source data. Native CI gates build and test the release preset for Windows and Apple Silicon on every push, and every release additionally publishes an Intel macOS build; signing, notarization, installer packaging, and legal distribution review remain separate release steps.

## Opening STEP Files with Loupe

To open `.step` and `.stp` files with a double-click:

- **macOS:** select a STEP file in Finder, choose File → Get Info, pick Loupe under
  *Open with*, and click **Change All…**.
- **Windows:** Loupe ships as a zip, so it registers itself: choose File → **Register
  Loupe for STEP Files…**. Windows then opens Default Apps, where you confirm Loupe for
  `.step` and `.stp` (Windows does not let an app make itself the default).

If Loupe is already running, a double-clicked file opens as a new tab in the existing
window.

## Inspection Tools

Loupe is deliberately focused on the inspection controls engineers use repeatedly, rather than becoming a full CAD authoring system.

| Tool | Control level |
| --- | --- |
| **Open and review** | Open `.step` and `.stp` files from the picker, by dropping them onto the app, or by double-clicking them once Loupe is their default app (see above). Each file opens in its own tab, which keeps its view, selection and export queue while you work in another; drag tabs to reorder them, off the strip into a window of their own, or onto another Loupe window to move them there. Review inferred source units before import; keep working while staged geometry refinement reports progress. |
| **Strip parts from an assembly** | Hide the parts to drop (search, multi-select, Hide or Isolate), then File → Export Assembly Without Hidden Parts… writes the rest as one STEP file, keeping the assembly tree, names, placements, colours and the source unit. Empty subassemblies are dropped. |
| **Assembly navigation** | Find a part by name (`Ctrl/Cmd+F`), expand or collapse assemblies (or all at once), and widen the tree by dragging its edge. Hover to pre-highlight, select one or many occurrences (⌘/Ctrl-click to add one, Shift-click for a range), fit, isolate, hide, or make semi-transparent; the tree shows what is hidden or transparent, and each row toggles both. Show all, and recolour a single occurrence or a shared definition. |
| **Transparency** | A transparent part starts at 45% opacity; the **Opacity** slider in Properties sets it anywhere from 10%, and **Show edges when transparent** keeps or drops its outline. Once a selection is transparent, the menus offer **Make opaque**. A transparent part stays selectable; **Selectable in the view** (Properties, or **Make unselectable** in the right-click menus) lets clicks pass through a part to what is behind it, and the tree still selects it. |
| **Visual inspection** | Choose orthographic or perspective projection, use the view cube for canonical views, look straight at a face with **View normal to face** (`Ctrl/Cmd+8`), and switch among Solid, Solid + Edges, and Edges Only. Edges are generated from CAD topology rather than display triangles. |
| **Part colours** | Keep the file's colours, or give every part an automatic colour -- cool shades, warm shades, muted, or high contrast -- with Shuffle. Useful when a file leaves parts uncoloured or gives them all one colour. The cool and warm palettes spread across related hues, so neighbouring parts stay distinct under shading. Display only. |
| **Materials and mass** | Assign materials to an occurrence or definition, manage a persistent custom material library, and calculate mass for a body, subassembly, or multi-selection. Standard materials cover common consumer-electronics use cases including aluminum, zinc alloy, copper, PP, HDPE, PA, PMMA, PC, FR4 PCBA, silicone, and HNBR rubber. |
| **Layout** | Every side panel is resizable (drag its divider; double-click to reset) and remembered. The toolbar under the view keeps Fit, Isolate, Hide and Transparent, which act on the model at once, apart from Section, Explode, Measure, Views and Capture, which open small windows over the view: drag one by its title bar, or collapse it to the title bar, and its button stays lit while it is open. In a narrow viewport the toolbar drops to icons, named in their tooltips. The switch at the top right picks System, Light or Dark. |
| **Section** | Pick a principal axis or a selected face plane, adjust offset, flip the clipped side, inspect normal or opposite views, cap cut faces, and create an outline, filled, or filled-plus-outline 2D slice. Slice outline color and thickness are adjustable. |
| **Exploded view** | Pull an assembly apart to see how its parts fit. **Assembled / Exploded** compares the two states, and **Spread** sets how far auto-explode takes the selected sub-assembly (or the whole model) apart. Drag a part or sub-assembly (Shift- or Ctrl/⌘-click for several) by its arrows along X, Y or Z, its ball freely in the screen plane, or its rings to turn it; moves snap, and Shift moves freely. Under **Advanced**: exact offsets and angles, move by a distance, and a picked pivot that stays with its part until **Use centre**. Every move can be undone; section and view normal to face work on the layout, and measuring waits for Assembled. Moving a part while Assembled starts a new layout from the assembly, and the one kept aside is saved first. **Saved layouts** keep a layout (a spread, a hinge turned open) to come back to in one click, per file on this computer, also under the **Exploded** tab of Views. **Export…** saves the layout as a STEP assembly, one STL, or a DXF/SVG/PDF drawing of the current view at true scale, leaving hidden parts out. The file itself is never changed. |
| **Measure** | Point to point, edge length, surface to surface, radius and diameter, angle, area, volume and bounds. Before a click, the face or edge it would take lights up and a chip by the pointer names it, or says why the mode cannot use it; point to point snaps to vertices, circle centres and edges. Surface to surface is the gap square to two parallel flat faces, or the true minimum between any two; an angle is drawn as an arc from one face or edge to the other. Shift or Ctrl/⌘-click adds edges or faces to a total; volume and bounds are of the selection or the whole model, and bounds draws its box with X, Y and Z on its edges. **Pin** keeps a result in the view, pointing at what was measured, so several of any kind show at once for a review or a capture. The panel gives ΔX/ΔY/ΔZ and how each value was found, and Copy puts it on the clipboard. |
| **Saved views** | **Views** (`V`) keeps snapshots to come back to, as in Photoshop or a CAD program's saved orientations: **Display** states hold what is hidden or transparent (opacity and edges included); **Camera** views hold the angle, zoom and projection, and the camera glides back to them. Each has a thumbnail and a name; **Reset** returns to the file as it opened. Kept per file on this computer; the file is never changed. |
| **Opening time** | Document properties shows how long the file took to be ready to view, to show every part, and to be fully detailed. **Opening History…** lists every open (size, parts, from cache or not) and copies as CSV, to compare with another CAD program. |
| **Capture** | Export the current inspection view as a PNG with transparent background and optional measurement/section content. Use 2x, 3x, or 5x presets or enter a custom render scale; the app renders at the requested resolution and reports capture progress. |

## Split-Part Export

The Export workspace turns a reviewed assembly into a pick-and-choose export bucket. Hovering a component shows its relationship to the master assembly and its standalone output preview before the checkbox is changed.

- Collapse subassemblies to keep large export trees manageable.
- Add candidates directly from component checkboxes and review the ordered export bucket.
- Reorder bucket rows with buttons or drag and drop.
- Choose original component names, a prefix plus original name, a sequential base name, or a per-file custom name. Renaming a row edits the name only; the `.step` or `.stl` suffix stays. Sequential rows show both the source component and final filename.
- Output goes to a folder beside the STEP file by default (`<file name> export/`), created when the first export runs; Browse picks any other.
- Export separate STEP or STL files with reviewed coordinate, grouping, destination, and overwrite settings.
- Validate every output, write files atomically, and report per-file progress and failures.

## 2D Drawing Export

The Drawing workspace projects a selected part to a 2D drawing at true 1:1 scale, for laser cutting, waterjet, machining quotes, and documentation. Projection runs on the CAD document through hidden-line removal rather than on display triangles, so curves are not faceted. Read the limitation at the end of this section before relying on the output.

Pick the view by choosing one of the six standard views, by clicking a flat face in the 3D preview, or by clicking a view-cube face. The picked face is highlighted while you choose, and a view that would be degenerate is reported rather than silently exported.

Three content modes cover different downstream uses:

| Mode | Output |
| --- | --- |
| **Cut** | Outer profile plus through-holes as closed contours a cutter can follow. The default. |
| **Silhouette** | Only the outline where material actually ends. Step, chamfer, and fillet lines are dropped, so a stepped face does not read as a line across the part. |
| **Technical** | Every visible edge, including smooth ones, on separate layers. Reads like a CAD view; for reference rather than cutting. |

- Export to **DXF**, **SVG**, or **PDF**. DXF files open correctly in a real downstream consumer.
- **1:1 is exact.** Any other ratio is named in the filename so it cannot be mistaken at the cutter. An optional scale fiducial can be embedded.
- Queue several drawings, review the batch, and execute it in the worker. Colliding generated names are numbered instead of the batch being refused.
- Hover a queued row for a preview; the 2D canvas is navigable, with bounded panning.

> **Known limitation — check drawings before cutting.** Projection is not yet
> considered production-reliable, and the workspace is included so it can be
> exercised on real parts. `loupe-spike drawing-audit <file.step>` measures two
> defects that are still open: Cut mode frequently emits **unclosed** contours
> (on sample assemblies, most contours in a view), and on some parts the
> Silhouette bounding box comes out **larger** than the Cut outline it filters.
> Treat output as a starting point to verify, not as a cut-ready file. Progress
> is tracked as Gate E in the [drawing export plan](docs/superpowers/plans/2026-07-24-2d-drawing-export.md).

## Input and Shortcuts

| Input | Action |
| --- | --- |
| Left drag | Orbit around the cursor-based pivot |
| Middle-button drag | Pan |
| Shift + left drag | Pan |
| Alt + left drag | Rotate the view in its own plane |
| Mouse wheel | Zoom |
| Trackpad two-finger drag | Pan |
| Trackpad pinch | Zoom |
| Right click | Open the context menu at the cursor |
| Click a view-cube face | Align to that standard view |
| Background click | Clear component selection |

Useful shortcuts:

| Shortcut | Action |
| --- | --- |
| `F` / `Shift+F` | Fit assembly / fit selection |
| `I` | Isolate or restore |
| `H` | Hide selection (Isolate, `I`, shows only the selection) |
| `T` / `Shift+T` | Make selection transparent (again: opaque) / make others transparent |
| `Ctrl/Cmd+F` | Find a part in the assembly tree (Enter / Shift+Enter: next / previous) |
| `Ctrl/Cmd+8` | View normal to the face under the pointer (again: its other side) |
| `Ctrl/Cmd+Shift+H` | Show all |
| `M` | Open Measure |
| `S` | Open Section |
| `V` | Open saved views |
| `E` | Open the exploded view; `Ctrl/Cmd+Z` / `Ctrl/Cmd+Shift+Z` undo / redo a move while it is open |
| `1`, `2`, `3` | Solid, Solid + Edges, Edges Only |
| `Ctrl/Cmd+O` | Open STEP (in a new tab if one is already open) |
| `Ctrl/Cmd+W` | Close tab |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab |
| `Escape` | Cancel the active manipulation or task state |

The in-app **View > Interaction** guide contains this same mouse, trackpad, and keyboard reference.

## Updates

Loupe checks GitHub for a newer release each time it starts. When there is one, a download icon appears in the toolbar: click it, then **Download** (a ring round the icon fills as it goes) and **Restart to Update**, which replaces the running copy and opens the new one with your settings kept. The new version runs a self-check before anything is replaced, and a failed copy puts the old one back.

- **Help > Check for Updates…** checks on demand.
- **Help > Include Beta Versions** chooses whether pre-releases are offered (on by default when running a beta).
- The update needs write access to Loupe's folder. On macOS, run Loupe from that folder rather than from inside the downloaded zip.
- Set `LOUPE_NO_UPDATE_CHECK=1` to turn the check off.

The first launch of a new version shows **What's New**, a short list of its highlights; **Help > What's New in Loupe…** shows it again.

Maintainers: each release adds its highlights to [`src/app/resources/whats-new.json`](src/app/resources/whats-new.json) (the build's tests fail without them). A manual run of the Release workflow labelled like `v0.1.5-beta.9` or `v0.1.5-rc.1` builds, tags and publishes a pre-release. A full release is published by pushing a `vX.Y.Z` tag, or by a manual run on `master` labelled with the project version (`v0.1.5`).

## Install a Release Build

Download the platform archive from the [latest release](https://github.com/imperator28/Loupe/releases/latest) and unzip it. Each release publishes three archives:

| Archive | For |
| --- | --- |
| `loupe-<tag>-macos-arm64.zip` | Apple Silicon Macs (M1 and later) |
| `loupe-<tag>-macos-x64.zip` | Intel Macs |
| `loupe-<tag>-windows-x64.zip` | Windows 11 x64 |

### macOS (Apple Silicon and Intel)

Both macOS archives install the same way. Release builds are **not yet code-signed or notarized**, so macOS quarantines the download and refuses to open it with a misleading *"Loupe is damaged and can't be opened"* message. The app is not damaged — this is Gatekeeper blocking an unsigned download. Clear the quarantine flag once, using whichever you prefer:

- **Finder:** in **System Settings → Privacy & Security**, scroll to the message about Loupe being blocked and click **Open Anyway**.
- **Helper script:** the macOS zip includes **`Open Loupe (first run).command`**. Put it beside `Loupe.app`, right-click it → **Open** (needed the first time because it, too, is a download), and it clears the flag and launches Loupe. It is a short, readable shell script — open it in a text editor first if you like.
- **Terminal:** `xattr -dr com.apple.quarantine /path/to/Loupe.app`

After clearing it once, Loupe opens normally from then on.

### Windows 11 (x64)

Unzip and run `Loupe.exe`. SmartScreen may warn about an unrecognized publisher (again, because the build is unsigned) — choose **More info → Run anyway**. Keep the app's folder intact; `Loupe.exe` loads its bundled Qt runtime and the `loupe-worker.exe` import process from the same directory.

## Build on Apple Silicon

### Prerequisites

Install full Xcode (not only Command Line Tools), Homebrew, CMake, Ninja, and Git. Select the Xcode developer directory if necessary, then bootstrap a user-local vcpkg installation:

```bash
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
brew install cmake ninja git
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
```

Clone and verify Loupe:

```bash
git clone https://github.com/imperator28/Loupe.git
cd Loupe
./scripts/bootstrap/macos.sh
```

### Configure, build, and test

```bash
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug --output-on-failure
```

Launch the debug application with its local Qt runtime paths:

```bash
./scripts/run-loupe-debug.sh macos-arm64-debug
```

Release and profiling presets are also available in [`CMakePresets.json`](CMakePresets.json). Windows development uses the checked-in Windows presets; see the [cross-platform development contract](docs/development/portability.md) before recording platform evidence.

## Build on Windows 11

Install Visual Studio 2022 with the Desktop development with C++ workload, CMake, Ninja, Git, and vcpkg. Open an x64 Visual Studio Developer PowerShell and set `VCPKG_ROOT` to your local vcpkg checkout:

```powershell
git clone https://github.com/imperator28/Loupe.git
cd Loupe
$env:VCPKG_ROOT = "$HOME/vcpkg"
.\scripts\bootstrap\windows.ps1
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release --output-on-failure
```

For a full platform gate, run `./scripts/verify/macos.sh` on Apple Silicon or `./scripts/verify/windows.ps1` from an x64 Visual Studio Developer PowerShell.

## Architecture

Loupe separates responsive UI work from native CAD operations:

```text
src/app/       Qt Quick desktop application and workspace controllers
src/worker/    Isolated OpenCascade import, refinement, section, and export work
src/core/      Stable IDs, export plans, validation, units, materials, and shared contracts
src/protocol/  Versioned application/worker messages and geometry payloads
tests/         Core, worker, controller, rendering-contract, and QML tests
```

The UI displays controller-owned state. Stable node IDs, immutable export plans, request generations, atomic output writes, and worker-owned source geometry remain the correctness boundaries.

## Documentation

- [Changelog](CHANGELOG.md)
- [2D drawing export requirements](docs/product/2026-07-24-2d-drawing-export-prd.md)
- [UI refinement handoff](docs/review/ui-refinement-handoff.md)
- [Apple Silicon development guide](docs/development/macos-apple-silicon.md)
- [Cross-platform development contract](docs/development/portability.md)
- [Implementation roadmap](docs/superpowers/plans/README.md)
- [Evidence policy and reports](docs/evidence/README.md)
- [Product design language](design.md)

## License and Third-Party Software

Loupe source code is licensed under [Apache License 2.0](LICENSE). This permissive license is a good fit for engineering teams that need to inspect or split supplier geometry while retaining the ability to build upon the application.

The distributed application also contains third-party components with their own terms. In particular, Qt open-source modules and Open CASCADE Technology have LGPL obligations that must be met for every binary distribution. Read [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) before distributing a build, and obtain legal advice for commercial, static-linking, app-store, or device-distribution decisions.

## Repository hygiene

Do not commit proprietary or private STEP geometry. `corpus/*.step` and `corpus/*.stp` are intentionally ignored; benchmark and evidence output must contain only approved case IDs, hashes, results, and timings. Build directories, vcpkg caches, IDE state, and local test artifacts also stay outside source control.

Before contributing a functional change, build the relevant preset and run its complete CTest suite.
