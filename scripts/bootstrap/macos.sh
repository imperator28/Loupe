#!/usr/bin/env bash
set -euo pipefail

for command in xcodebuild clang cmake ninja; do
  command -v "$command" >/dev/null || { echo "Missing required dependency: $command" >&2; exit 1; }
done
xcodebuild -version >/dev/null 2>&1 || {
  echo 'A full Xcode installation is required; Command Line Tools alone are not sufficient.' >&2
  exit 1
}
[[ -n "${VCPKG_ROOT:-}" ]] || { echo 'Missing required dependency: VCPKG_ROOT environment variable' >&2; exit 1; }
[[ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]] || { echo 'VCPKG_ROOT does not contain scripts/buildsystems/vcpkg.cmake' >&2; exit 1; }
[[ "$(uname -m)" == "arm64" ]] || { echo 'Apple Silicon verification requires an arm64 host' >&2; exit 1; }
echo 'macOS Apple Silicon native build prerequisites are available.'
