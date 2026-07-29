# Cross-platform development contract

Use CMake presets and the platform bootstrap/verification scripts; do not commit build directories, vcpkg caches, IDE state, or private CAD corpus files. Keep source paths UTF-8, includes case-correct, and stable IDs deterministic across compilers.

Windows is the current evidence platform. The Apple Silicon presets are structural until the M2 Pro is available; after that, every phase gate requires Debug and Release verification on both platforms. Platform-specific file replacement, process launching, and system integration belong behind adapters rather than shared core code.

## Known platform divergences

Behaviour that is genuinely different per platform, and therefore cannot be
verified on one of them. A fix landing on one platform does not imply the other
was ever affected -- several entries below were macOS-only defects whose Windows
path was already correct, and fixing them must not regress Windows.

### Version surfaces

`project(Loupe VERSION ...)` in the root `CMakeLists.txt` is the single source.
It reaches the About dialog identically on both platforms via the
`LOUPE_VERSION` compile definition, so the two can never disagree with each
other -- but each platform has additional surfaces that can drift from it:

| Surface | macOS | Windows |
| --- | --- | --- |
| About dialog | `LOUPE_VERSION` | `LOUPE_VERSION` |
| OS file metadata | `CFBundleShortVersionString` / `CFBundleVersion`, both pinned to `PROJECT_VERSION` in `src/app/CMakeLists.txt` | **none** -- `assets/branding/Loupe.rc` carries only the icon, no `VERSIONINFO`, so Properties -> Details reports no version |
| Release asset name | `github.ref_name` (the tag) | `github.ref_name` (the tag) |

Two traps, both previously live:

- CMake defaults `CFBundleShortVersionString` to `MAJOR.MINOR`. Left unset, the
  bundle advertised `0.1` to Finder while the About dialog read `0.1.2`. Both
  keys are now set explicitly.
- The release tag names the assets but does not build them. Tagging without
  bumping `project(VERSION)` shipped correctly-named archives containing the
  previous version. The `verify-version` job in `release.yml` now fails the
  release in seconds if the tag and the project version disagree.

Adding a Windows `VERSIONINFO` block would close the remaining asymmetry; it is
not done yet because the resource is only compiled on Windows and so cannot be
verified from a macOS working copy.

### Pointer input

Trackpad gestures do not arrive by the same route:

- **Windows** delivers a trackpad pinch as `Ctrl`+wheel, which
  `PointerInputRouter.wheelMode` resolves to `"zoom"`. `PinchHandler` is not
  involved. Windows also routes trackpads through legacy mouse messages, so a
  two-finger drag reports `PointerDevice.Mouse` with a partial notch and no
  pixel delta -- the signature `wheelMode` keys on to pan.
- **macOS** delivers a pinch as a native zoom gesture through `PinchHandler`.
  Its centroid moves as the fingers close, so deriving a pan from
  `activeTranslation` drifted the view on every zoom. `StepViewport` now pans
  from gesture translation only for devices with no other pan route; a
  touchscreen still does, a trackpad does not.

### Asynchronous geometry replay

Fitting the camera depends on geometry that arrives asynchronously. Windows
consistently won that race and macOS consistently lost it, which made a missing
fit look like a macOS-only bug when it was really an unsequenced dependency.
Fit requests are held until they can be satisfied rather than dropped, so
neither platform depends on the ordering. Timing-sensitive behaviour like this
should never be validated on one platform alone.

### Layout

Qt Quick Controls resolve implicit sizes differently per platform. A
`ScrollView` is a `Control`: its implicit width comes from its content and its
padding includes the scroll bar, both platform-dependent. Pinning a
`Layout.preferredWidth` directly on one made Export and Drawing disagree on
macOS while matching on Windows. Pin widths on a plain layout item and put the
`Control` inside it.
