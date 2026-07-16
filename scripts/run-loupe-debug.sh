#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
preset="${1:-macos-arm64-debug}"
build_root="$root/build/$preset"
app="$build_root/src/app/Loupe.app/Contents/MacOS/Loupe"
configuration_root="debug"

if [[ "$preset" == "macos-arm64-release" ]]; then
  configuration_root=""
fi

qt_root="$build_root/vcpkg_installed/arm64-osx/$configuration_root/Qt6"
qt_bin="$build_root/vcpkg_installed/arm64-osx/$configuration_root/bin"
qt_lib="$build_root/vcpkg_installed/arm64-osx/$configuration_root/lib"

[[ -x "$app" ]] || { echo "Loupe has not been built for '$preset': $app" >&2; exit 1; }
[[ -d "$qt_root/plugins" ]] || { echo "Qt platform plugins are missing for '$preset': $qt_root" >&2; exit 1; }

export QT_PLUGIN_PATH="$qt_root/plugins"
export QML2_IMPORT_PATH="$qt_root/qml"
export DYLD_LIBRARY_PATH="$qt_bin:$qt_lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"

exec "$app"
