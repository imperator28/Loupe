<p align="center">
  <img src="assets/branding/loupe-app-icon.svg" width="152" alt="Loupe application icon">
</p>

<h1 align="center">Loupe</h1>

<p align="center">
  A lightweight, engineer-oriented STEP inspector for fast CAD review, measurement, sectioning, materials, split-part export, and 2D drawing projection.
</p>

## Status

Loupe is a functional desktop review build under active development. It is designed to make a typical engineering inspection workflow feel immediate: open a STEP file, understand the assembly, inspect geometry and mass, capture evidence, then prepare only the parts a vendor or prototype shop needs — as 3D files, or as 2D drawings a cutter can run.

The work is organised into three workspaces: **Inspect** for review and measurement, **Export** for split-part 3D output, and **Drawing** for 2D projection. Inspect and Export are the mature paths; Drawing is newer and its projection is not yet production-reliable — see the limitation noted under [2D Drawing Export](#2d-drawing-export).

The desktop targets are Windows 11 x64, Apple Silicon macOS, and Intel macOS. Geometry and export operations use the imported CAD document rather than treating display meshes as source data. Native CI gates build and test the release preset for Windows and Apple Silicon on every push, and every tag additionally publishes an Intel macOS build; signing, notarization, installer packaging, and legal distribution review remain separate release steps.

## Inspection Tools

Loupe is deliberately focused on the inspection controls engineers use repeatedly, rather than becoming a full CAD authoring system.

| Tool | Control level |
| --- | --- |
| **Open and review** | Open `.step` and `.stp` files from the picker or by dropping them onto the app. Review inferred source units before import; keep working while staged geometry refinement reports progress. |
| **Assembly navigation** | Expand or collapse assemblies, hover to pre-highlight, select one or many occurrences, fit, isolate, hide, show all, and recolor a single occurrence or a shared definition. |
| **Visual inspection** | Choose orthographic or perspective projection, use the view cube for canonical views, and switch among Solid, Solid + Edges, and Edges Only. Edges are generated from CAD topology rather than display triangles. |
| **Materials and mass** | Assign materials to an occurrence or definition, manage a persistent custom material library, and calculate mass for a body, subassembly, or multi-selection. Standard materials cover common consumer-electronics use cases including aluminum, zinc alloy, copper, PP, HDPE, PA, PMMA, PC, FR4 PCBA, silicone, and HNBR rubber. |
| **Section** | Pick a principal axis or a selected face plane, adjust offset, flip the clipped side, inspect normal or opposite views, cap cut faces, and create an outline, filled, or filled-plus-outline 2D slice. Slice outline color and thickness are adjustable. |
| **Measure** | Inspect points, edges, faces, radii, angles, area, volume, and bounds with topology-specific hover and persistent pick highlights. |
| **Capture** | Export the current inspection view as a PNG with transparent background and optional measurement/section content. Use 2x, 3x, or 5x presets or enter a custom render scale; the app renders at the requested resolution and reports capture progress. |

## Split-Part Export

The Export workspace turns a reviewed assembly into a pick-and-choose export bucket. Hovering a component shows its relationship to the master assembly and its standalone output preview before the checkbox is changed.

- Collapse subassemblies to keep large export trees manageable.
- Add candidates directly from component checkboxes and review the ordered export bucket.
- Reorder bucket rows with buttons or drag and drop.
- Choose original component names, a prefix plus original name, a sequential base name, or a per-file custom name. Sequential rows show both the source component and final filename.
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
| `H` / `Shift+H` | Hide selection / hide others |
| `Ctrl/Cmd+Shift+H` | Show all |
| `M` | Open Measure |
| `S` | Open Section |
| `1`, `2`, `3` | Solid, Solid + Edges, Edges Only |
| `Ctrl/Cmd+O` | Open STEP |
| `Escape` | Cancel the active manipulation or task state |

The in-app **View > Interaction** guide contains this same mouse, trackpad, and keyboard reference.

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
