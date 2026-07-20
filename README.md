<p align="center">
  <img src="assets/branding/loupe-app-icon.svg" width="152" alt="Loupe application icon">
</p>

<h1 align="center">Loupe</h1>

<p align="center">
  A cross-platform desktop application for inspecting STEP assemblies and selectively exporting validated component files.
</p>

## Status

Loupe is a functional review build under active development. The Inspect and Export workflows are available for UX evaluation, but release hardening, signing, notarization, and distribution packaging are not complete.

The current desktop targets are Windows 11 and Apple Silicon macOS. Geometry and export operations use the imported CAD document rather than treating display meshes as source data.

## Capabilities

### Inspect

- Open STEP parts and assemblies with staged import and refinement progress.
- Preserve assembly hierarchy, repeated occurrences, units, and STEP appearance data.
- Navigate with orthographic or perspective projection and standard Top, Front, and Right views.
- Switch between Solid, Solid + Edges, and Edges Only display modes.
- Select, fit, hide, isolate, recolor, and assign materials by occurrence or definition.
- Create interactive section views with caps, filled 2D slices, normal views, and direct offset manipulation.
- Measure points, edges, surfaces, radii, angles, area, volume, and bounds with topology-specific highlights.
- Capture the active view to PNG.

### Export

- Review candidates in a master assembly preview and a standalone component preview.
- Build an export bucket directly from component checkboxes.
- Reorder output files and use original, sequential, prefixed, or per-file names.
- Export separate STEP or STL files with reviewed coordinate and destination settings.
- Validate each output and report progress and failures per file.

## Navigation

| Input | Action |
| --- | --- |
| Left drag | Orbit around the cursor-based pivot |
| Middle-button drag | Pan |
| Shift + left drag | Pan |
| Mouse wheel | Zoom |
| Trackpad two-finger drag | Pan |
| Trackpad pinch | Zoom |
| Right click | Open the context menu at the cursor |
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

## Build on Apple Silicon

### Prerequisites

Install Xcode Command Line Tools, Homebrew, CMake, Ninja, and Git. Bootstrap a user-local vcpkg installation:

```bash
xcode-select --install
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

- [UI refinement handoff](docs/review/ui-refinement-handoff.md)
- [Apple Silicon development guide](docs/development/macos-apple-silicon.md)
- [Cross-platform development contract](docs/development/portability.md)
- [Implementation roadmap](docs/superpowers/plans/README.md)
- [Evidence policy and reports](docs/evidence/README.md)
- [Product design language](design.md)

## Repository hygiene

Do not commit proprietary or private STEP geometry. Keep private corpus files under `corpus/private/`; benchmark and evidence output must contain only approved case IDs, hashes, results, and timings. Build directories, vcpkg caches, IDE state, and local test artifacts also stay outside source control.

Before contributing a functional change, build the relevant preset and run its complete CTest suite.
