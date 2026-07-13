# Apple Silicon Development Kickoff

This guide starts Phase 1 validation on a clean M-series Mac. It is intentionally user-local: no administrator access is required to build, run, or inspect Loupe.

## 1. Install developer prerequisites

Install Xcode Command Line Tools, Homebrew, CMake, Ninja, and Git. Install vcpkg in your home directory, then export its location for the current shell (or add it to your shell profile):

```bash
xcode-select --install
brew install cmake ninja git
git clone https://github.com/microsoft/vcpkg.git "$HOME/vcpkg"
"$HOME/vcpkg/bootstrap-vcpkg.sh"
export VCPKG_ROOT="$HOME/vcpkg"
```

Verify the host is native Apple Silicon and the repository checks are satisfied:

```bash
uname -m                 # arm64
./scripts/bootstrap/macos.sh
```

## 2. Build and test both presets

```bash
cmake --preset macos-arm64-debug
cmake --build --preset macos-arm64-debug
ctest --preset macos-arm64-debug --output-on-failure

cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release
ctest --preset macos-arm64-release --output-on-failure
```

Launch the Debug shell with its local Qt runtime paths:

```bash
./scripts/run-loupe-debug.sh macos-arm64-debug
```

## 3. Required P1 hardware validation

Use the two approved private corpus STEP files. Record only the case ID, source hash, result, and timings—never source paths or geometry—under `docs/evidence/platform/macos.json` and the Phase 1 evidence report.

- Open each model, confirm units, tree, component selection, point measurement, topology metrics, isolate/ghost, and X/Y/Z section flip.
- Save a 1x and 4x PNG capture; verify dimensions, alpha choice, and output readability.
- Confirm a cached reopen appears before streamed mesh completion.
- Run `./scripts/verify/macos.sh` after the Debug and Release checks. It writes the platform evidence record.

The M-series row is `pending` until all of these actions have fresh evidence. Phase 3, not Phase 1, owns signing, notarization, and a distributable macOS package.
