#!/usr/bin/env bash
# Stage the Windows app into a self-contained folder: Loupe.exe, the import worker, every
# DLL, and the Qt runtime it loads at run time (platform and other plugins, QML modules).
# Used by both release.yml and native-verify.yml, so the package a tester downloads and the
# one CI verifies are built the same way.
#
#   scripts/package/stage-windows.sh <configure-preset> <vcpkg-triplet> <destination>
set -euo pipefail

preset="$1"
triplet="$2"
dest="$3"

# OpenCASCADE's CMake config overrides output directories, so the exe is located rather
# than assumed.
EXE=$(find "build/${preset}" -maxdepth 3 \( -name Loupe.exe -o -name loupe-app.exe \) -not -path "*CMakeFiles*" | head -1)
test -n "$EXE"
mkdir -p "$dest"
cp "$EXE" "$dest/Loupe.exe"
cp "$(dirname "$EXE")"/*.dll "$dest/" 2>/dev/null || true
# The app launches loupe-worker.exe for all STEP import and geometry work; the *.dll glob
# above never picks it up, and releases once shipped without it.
cp "$(dirname "$EXE")/loupe-worker.exe" "$dest/"

VCPKG_INSTALLED="build/${preset}/vcpkg_installed/${triplet}"
WINDEPLOY=$(find "$VCPKG_INSTALLED/tools" -name windeployqt.exe 2>/dev/null | head -1)
if [[ -n "$WINDEPLOY" ]]; then
  "$WINDEPLOY" --release --qmldir src/app/qml "$dest/Loupe.exe"
else
  echo "windeployqt not found; staging the Qt runtime wholesale"
  # Plugin folders (platforms/, styles/, imageformats/, ...) go directly beside the exe,
  # which is where Qt looks for them with no configuration at all. They used to sit one
  # level down in plugins/ and depended on qt.conf to be found; a beta then failed to start
  # on Windows with "no Qt platform plugin could be initialized".
  cp -r "$VCPKG_INSTALLED/Qt6/plugins/." "$dest/"
  cp -r "$VCPKG_INSTALLED/Qt6/qml" "$dest/"
  cp "$VCPKG_INSTALLED/bin"/*.dll "$dest/"
  # QML modules still need it: a relocatable Qt takes its prefix as the parent of the
  # directory holding Qt6Core.dll, which would put the default import path outside the app.
  printf '[Paths]\nPrefix = .\nPlugins = .\nQml2Imports = qml\n' > "$dest/qt.conf"
fi

# Whatever path was taken, the app cannot start without these.
test -f "$dest/platforms/qwindows.dll" || { echo "::error::platforms/qwindows.dll missing from the package"; exit 1; }
test -f "$dest/loupe-worker.exe" || { echo "::error::loupe-worker.exe missing from the package"; exit 1; }
