#!/usr/bin/env bash
set -euo pipefail

src="docs/design/system-settings-ui-north-star.webp"
out="assets/ui"
mkdir -p "$out"

if command -v magick >/dev/null 2>&1; then
  IM=(magick)
elif command -v convert >/dev/null 2>&1; then
  IM=(convert)
else
  echo "ImageMagick is required to regenerate the UI art assets." >&2
  exit 2
fi

identify_cmd=identify
if command -v magick >/dev/null 2>&1; then
  identify_cmd="magick identify"
fi

read -r width height < <(eval "$identify_cmd -format '%w %h' '$src'")
if [[ -z "${width:-}" || -z "${height:-}" ]]; then
  echo "Could not read north-star dimensions." >&2
  exit 3
fi

crop_scaled() {
  local name="$1" x_num="$2" y_num="$3" w_num="$4" h_num="$5" tw="$6" th="$7"
  local x=$(( width * x_num / 10000 ))
  local y=$(( height * y_num / 10000 ))
  local cw=$(( width * w_num / 10000 ))
  local ch=$(( height * h_num / 10000 ))

  "${IM[@]}" "$src"     -crop "${cw}x${ch}+${x}+${y}" +repage     -resize "${tw}x${th}^" -gravity center -extent "${tw}x${th}"     -strip -interlace Plane -sampling-factor 4:2:0 -quality 88     "$out/$name"
}

# The source remains the approved north-star composition. Each runtime asset is
# extracted independently so the application uses dedicated artwork rather than
# displaying, slicing, or overlaying the full prototype screen at runtime.
crop_scaled hero-sunset.jpg       4306  935 4007 1807 1200 280
crop_scaled overview-mountain.jpg 2165 4336 1220 1775  360 280
crop_scaled date-city.jpg         4904 6557 1286 1158  420 180
crop_scaled region-sydney.jpg     8134 6546 1465  978  420 180
crop_scaled network-globe.jpg     8254 8077 1376 1541  440 240
crop_scaled theme-light.jpg       2255 8587  736  659  180  92
crop_scaled theme-dark.jpg        3200 8587  837  659  180  92
crop_scaled theme-follow.jpg      4234 8587  825  659  180  92
crop_scaled theme-mercedes.jpg    5221 8587  843  659  180  92

echo "Generated dedicated runtime UI art assets from $src"
