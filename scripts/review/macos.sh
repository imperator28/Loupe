#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
preset="macos-arm64-debug"
launch=false

case "${1:-}" in
  "") ;;
  --launch) launch=true ;;
  *)
    echo "Usage: $0 [--launch]" >&2
    exit 2
    ;;
esac

export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

"$root/scripts/bootstrap/macos.sh"
cmake --preset "$preset"
cmake --build --preset "$preset"

smoke_pattern='(spike|corpus|qml-smoke|qml-inspection-tools|inspection-tools)'
ctest --preset "$preset" --output-on-failure -R "$smoke_pattern"

if [[ "$launch" == true ]]; then
  exec "$root/scripts/run-loupe-debug.sh" "$preset"
fi

echo "UX review smoke gate passed."
echo "Launch Loupe with: $root/scripts/run-loupe-debug.sh $preset"

