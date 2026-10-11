#!/bin/sh
# Renders all activity icons with their sport type names to icon_overview.png,
# without flashing. Needs g++ and the libraries PlatformIO downloads on the first
# `pio run`. Converting to PNG needs python3 with Pillow; otherwise the .pbm stays.
set -e

here=$(cd "$(dirname "$0")" && pwd)
firmware="$here/../../.."
gfx="$firmware/.pio/libdeps/esp32dev/Adafruit GFX Library"
lib="$firmware/lib/EpaperDisplay/src"
out="$here/build"

if [ ! -d "$gfx" ]; then
    echo "Adafruit GFX not found, run 'pio run' in firmware/ once first." >&2
    exit 1
fi

mkdir -p "$out"
g++ -std=gnu++17 -DARDUINO=100 -w -I"$here/stubs" -I"$gfx" -I"$lib" \
    "$here/main.cpp" "$lib/ActivityIcons.cpp" "$lib/IconOverview.cpp" "$gfx/Adafruit_GFX.cpp" \
    -o "$out/icon_overview"
"$out/icon_overview" "$out/icon_overview.pbm"

if python3 -c "import PIL" 2>/dev/null; then
    python3 -I -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" \
        "$out/icon_overview.pbm" "$out/icon_overview.png"
    echo "$out/icon_overview.png"
else
    echo "$out/icon_overview.pbm"
fi
