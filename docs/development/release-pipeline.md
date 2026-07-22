# Release pipeline

## Shape

Two workflows share one vcpkg binary cache:

| Workflow | Trigger | Purpose | Output |
|---|---|---|---|
| `native-verify.yml` | every push / PR | Build + full ctest suite on both platforms | Smoke artifacts (14-day retention) |
| `release.yml` | tag `v*` (or manual dispatch) | Same build + tests, then publish | Versioned GitHub Release with `loupe-<tag>-macos-arm64.zip` and `loupe-<tag>-windows-x64.zip` |

Cutting a release is: tag and push.

```bash
git tag v0.1.0 && git push origin v0.1.0
```

## Why dependency builds are the whole cost

Loupe itself compiles in ~3 minutes. Qt 6 + OpenCASCADE from source is hours.
Three decisions keep that cost out of the day-to-day loop:

1. **Release-only overlay triplets** (`triplets/*-release.cmake`): vcpkg's
   default builds every dependency twice (debug + release). A release
   pipeline never links the debug half, which measured ~9.3 GB of a ~9.6 GB
   installed tree. CI passes `-DVCPKG_TARGET_TRIPLET=<t>-release
   -DVCPKG_OVERLAY_TRIPLETS=triplets`; local developer builds keep the
   default dual-variant triplets. `VCPKG_HOST_TRIPLET` is pinned to the same
   release-only triplet — Qt ports pull host tools (moc, rcc, qmlcachegen)
   through the host triplet, which otherwise silently builds a second,
   debug+release dependency world. `--clean-buildtrees-after-build
   --clean-packages-after-build` bounds peak runner disk, which killed the
   first macOS bootstrap attempt.
2. **Incremental, always-saved binary caching**: vcpkg archives each built
   port individually. The workflows save the cache with `if: always()` and a
   unique per-run key, so even a run that times out mid-Qt banks its progress
   and the next run resumes. Cold bootstrap is a one-time (per dependency
   change) cost; warm runs restore in minutes.
3. **Pinned reproducibility**: the dependency set is fixed by
   `vcpkg-configuration.json`'s registry baseline plus the `VCPKG_COMMIT`
   tool pin in the workflows. Cache keys hash the manifest, configuration,
   and triplets, so a dependency change cleanly invalidates.

Expected timings: cold dependency bootstrap ~1–2 h per platform (once per
baseline/manifest change); warm verify or release run ~10–20 min.

## Compiling locally

macOS (Apple Silicon):

```bash
export VCPKG_ROOT=~/vcpkg
cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release --parallel
ctest --preset macos-arm64-release
ditto -c -k --keepParent build/macos-arm64-release/src/app/Loupe.app dist/loupe-macos-arm64.zip
```

Windows 11 (x64, from a VS developer prompt):

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"   # checked out at the workflow's VCPKG_COMMIT
cmake --preset windows-release
cmake --build --preset windows-release --parallel
ctest --preset windows-release
Compress-Archive build/windows-release/src/app/* dist/loupe-windows-x64.zip
```

Local builds intentionally use the default dual-variant triplets so debug
builds stay possible; only CI passes the release-only overlay triplets. If a
local configure must rebuild Qt ports at a new baseline and fails with
"Can't determine Xcode version", install full Xcode and run
`sudo xcode-select -s /Applications/Xcode.app` — Command Line Tools are not
sufficient for Qt 6.11 port builds. As a stopgap, an already-installed tree
can be reused:

```bash
cmake --preset macos-arm64-release \
  -DVCPKG_MANIFEST_INSTALL=OFF \
  -DVCPKG_INSTALLED_DIR=$PWD/build/macos-arm64-debug/vcpkg_installed
```

## Known gates before public distribution

- **Unsigned binaries**: macOS Gatekeeper and Windows SmartScreen will warn.
  Needs an Apple Developer ID (+ notarization) and an Authenticode
  certificate; both are owner decisions with real-world identity/cost.
- **Local macOS release configures need full Xcode** when a baseline bump
  forces a Qt port rebuild (Qt 6.11 hard-requires `xcodebuild`; Command Line
  Tools are not enough). CI runners ship Xcode and are unaffected.
