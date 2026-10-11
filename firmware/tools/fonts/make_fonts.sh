#!/bin/sh
# Generates Adafruit GFX fonts with Latin-1 characters (ä, ö, ü, ß, é, ...) from
# GNU FreeFont, into lib/EpaperDisplay/src/fonts/. The fonts bundled with Adafruit
# GFX (FreeSans18pt7b, ...) only contain ASCII.
#
# Needs gcc, FreeType (freetype2) and GNU FreeFont (Arch: gnu-free-fonts,
# Debian/Ubuntu: fonts-freefont-otf), plus one `pio run` for the fontconvert source.
set -e

here=$(cd "$(dirname "$0")" && pwd)
firmware="$here/../.."
fontconvert_src="$firmware/.pio/libdeps/esp32dev/Adafruit GFX Library/fontconvert"
font_dir=${FONT_DIR:-/usr/share/fonts/gnu-free}
out="$firmware/lib/EpaperDisplay/src/fonts"
build="$here/build"

mkdir -p "$build" "$out"
gcc -w $(pkg-config --cflags freetype2) "$fontconvert_src/fontconvert.c" \
    $(pkg-config --libs freetype2) -o "$build/fontconvert"

# <font file> <size in pt>; 0x20-0xFF = ASCII + Latin-1, file name gets "8b" for 8 bit
for spec in "FreeSans 12" "FreeSans 18" "FreeSansBold 24"; do
    set -- $spec
    "$build/fontconvert" "$font_dir/$1.otf" "$2" 32 255 > "$out/$1$2pt8b.h"
    echo "$out/$1$2pt8b.h"
done
