#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source_svg="$root/assets/branding/loupe-app-icon.svg"
output_dir="$root/assets/branding"

for tool in qlmanage sips iconutil python3; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "Required macOS tool is unavailable: $tool" >&2
        exit 1
    }
done

python3 -c 'from PIL import Image, ImageChops, ImageDraw' >/dev/null 2>&1 || {
    echo "Python Pillow is required to generate transparent rounded app icons." >&2
    exit 1
}

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
masked="$work/loupe-app-icon-masked.png"

[[ -f "$rendered" ]] || {
    echo "Quick Look did not produce the expected icon render." >&2
    exit 1
}

# Quick Look renders transparent SVG corners against opaque white. Reapply the
# source icon's rounded mask so every platform artifact has real alpha corners.
python3 - "$rendered" "$masked" <<'PY'
import sys

from PIL import Image, ImageChops, ImageDraw

source = Image.open(sys.argv[1]).convert("RGBA")
width, height = source.size
scale = 4

mask = Image.new("L", (width * scale, height * scale), 0)
draw = ImageDraw.Draw(mask)
draw.rounded_rectangle(
    (
        round(width * scale * 28 / 500),
        round(height * scale * 28 / 500),
        round(width * scale * 472 / 500),
        round(height * scale * 472 / 500),
    ),
    radius=round(min(width, height) * scale * 105 / 500),
    fill=255,
)
mask = mask.resize((width, height), Image.Resampling.LANCZOS)
source.putalpha(ImageChops.multiply(source.getchannel("A"), mask))
source.save(sys.argv[2])

if source.getpixel((0, 0))[3] != 0 or source.getpixel((width // 2, height // 2))[3] != 255:
    raise SystemExit("Generated icon did not retain the expected rounded alpha mask")
PY

render_size() {
    local size="$1"
    local output="$2"
    sips -z "$size" "$size" "$masked" --out "$output" >/dev/null
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

python3 - "$masked" "$output_dir/Loupe.ico" <<'PY'
import sys

from PIL import Image

icon = Image.open(sys.argv[1]).convert("RGBA")
icon.save(
    sys.argv[2],
    format="ICO",
    sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)],
)
PY

echo "Generated $output_dir/Loupe.icns"
echo "Generated $output_dir/Loupe.ico"
echo "Generated $output_dir/loupe-app-icon.png"
