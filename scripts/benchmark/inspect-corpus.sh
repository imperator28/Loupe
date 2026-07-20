#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
runner="$root/build/macos-arm64-debug/src/worker/loupe-import-benchmark"
output_dir="$root/build/benchmark"
budgets="$root/scripts/benchmark/macos-arm64-debug-budgets.json"

if [[ $# -eq 0 ]]; then
  echo "Usage: $0 <STEP file> [<STEP file> ...]" >&2
  exit 2
fi
if [[ ! -x "$runner" ]]; then
  echo "Build loupe-import-benchmark before running this script." >&2
  exit 2
fi

mkdir -p "$output_dir"
timestamp="$(date +%Y%m%d-%H%M%S)"
output="$output_dir/import-$timestamp.jsonl"
"$runner" --budget-file "$budgets" "$@" | tee "$output"
echo "Wrote $output" >&2
