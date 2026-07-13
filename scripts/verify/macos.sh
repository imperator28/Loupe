#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
evidence_path="${1:-docs/evidence/platform/macos.json}"
"$root/scripts/bootstrap/macos.sh"
for preset in macos-arm64-debug macos-arm64-release; do
  cmake --preset "$preset"
  cmake --build --preset "$preset"
  ctest --preset "$preset" --output-on-failure
done
mkdir -p "$(dirname "$evidence_path")"
printf '{\n  "platform": "macos-arm64",\n  "architecture": "arm64",\n  "compiler": "%s",\n  "commit": "%s",\n  "presets": [\n    {"preset": "macos-arm64-debug", "configured": true, "built": true, "testsPassed": true},\n    {"preset": "macos-arm64-release", "configured": true, "built": true, "testsPassed": true}\n  ]\n}\n' "$(clang --version | head -n 1 | sed 's/"/\\"/g')" "$(git -C "$root" rev-parse HEAD)" > "$evidence_path"
echo "Wrote verification evidence: $evidence_path"
