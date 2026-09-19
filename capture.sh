#!/bin/bash
# Capture a field view on the Mac, with no phone involved: builds the desktop host + game lib and
# runs the real renderer once with FIELD_CAPTURE set. Writes story/field/views/<map>_<zone>.png.
#
#   ./capture.sh halm square 2
#
# The window flashes up for a moment; that is the renderer doing its one frame.
#
# The tile field (TILES.md) has its own mode, and it captures the WHOLE map rather than one view,
# which is how a .tmap is read as a town without a phone screenshot:
#
#   ./capture.sh --tiles halm        -> build_desktop/tiles_halm.png
#
set -e

if [ "$1" = "--tiles" ]; then
    MAP=${2:-halm}
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    OUT="$ROOT/build_desktop/tiles_${MAP}.png"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -2
    cd "$ROOT"
    TILE_CAPTURE="$MAP:1:$OUT" "$ROOT/build_desktop/quest_glory" || true
    if [ -f "$OUT" ]; then
        echo "wrote $OUT"
        /usr/bin/sips -g pixelWidth -g pixelHeight "$OUT" 2>/dev/null | tail -2
        exit 0
    fi
    echo "ERROR: no capture written to $OUT" >&2
    exit 1
fi

MAP=${1:-halm}
ZONE=${2:-base}
SCALE=${3:-2}
ROOT="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="$ROOT/story/field/views"
OUT="$OUT_DIR/${MAP}_${ZONE}.png"

mkdir -p "$OUT_DIR"
cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -3

# The lib is dlopen'd next to the executable; the map and art are read from story/ relative to the
# working directory, which is why this runs from the repo root.
cd "$ROOT"
FIELD_CAPTURE="$MAP:$ZONE:$SCALE:$OUT" "$ROOT/build_desktop/quest_glory" || true

# The depth reference is debug output, not art: keep it out of the repo.
if [ -f "${OUT%.png}_depth.png" ]; then
    mkdir -p "$ROOT/build_desktop/views_depth"
    mv "${OUT%.png}_depth.png" "$ROOT/build_desktop/views_depth/"
fi

if [ -f "$OUT" ]; then
    echo "wrote $OUT"
    /usr/bin/sips -g pixelWidth -g pixelHeight "$OUT" 2>/dev/null | tail -2
else
    echo "ERROR: no capture written to $OUT" >&2
    exit 1
fi
