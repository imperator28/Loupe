#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_svg="$root/assets/branding/loupe-app-icon.svg"
output_dir="$root/assets/branding"

for tool in qlmanage sips iconutil; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Required macOS tool is unavailable: $tool" >&2
        exit 1
    }
done

[[ -f "$source_svg" ]] || {
    echo "Icon source is missing: $source_svg" >&2
    exit 1
}

work="$(mktemp -d "${TMPDIR:-/tmp}/loupe-icons.XXXXXX")"
trap 'rm -rf "$work"' EXIT

preview_dir="$work/preview"
iconset="$work/Loupe.iconset"
mkdir -p "$preview_dir" "$iconset"

qlmanage -t -s 1024 -o "$preview_dir" "$source_svg" >/dev/null
rendered="$preview_dir/$(basename "$source_svg").png"

[[ -f "$rendered" ]] || {
    echo "Quick Look did not produce the expected icon render." >&2
    exit 1
}

render_size() {
    local size="$1"
    local output="$2"
    sips -z "$size" "$size" "$rendered" --out "$output" >/dev/null
}

render_size 16 "$iconset/icon_16x16.png"
render_size 32 "$iconset/icon_16x16@2x.png"
render_size 32 "$iconset/icon_32x32.png"
render_size 64 "$iconset/icon_32x32@2x.png"
render_size 128 "$iconset/icon_128x128.png"
render_size 256 "$iconset/icon_128x128@2x.png"
render_size 256 "$iconset/icon_256x256.png"
render_size 512 "$iconset/icon_256x256@2x.png"
render_size 512 "$iconset/icon_512x512.png"
render_size 1024 "$iconset/icon_512x512@2x.png"
render_size 512 "$output_dir/loupe-app-icon.png"

iconutil -c icns "$iconset" -o "$output_dir/Loupe.icns"

echo "Generated $output_dir/Loupe.icns"
echo "Generated $output_dir/loupe-app-icon.png"
