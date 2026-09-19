#!/bin/bash
# Capture a field view on the Mac, with no phone involved: builds the desktop host + game lib and
# runs the real renderer once with FIELD_CAPTURE set. Writes story/field/views/<map>_<zone>.png.
#
#   ./capture.sh halm square 2
#
# The window flashes up for a moment; that is the renderer doing its one frame.
#
# The tile field (TILES.md) has its own mode: the player's own 640x360 view rendered at the phone's
# resolution (scale 3 = 1920x1080), party included, written to build_desktop/tiles_<map>.png.
#
#   ./capture.sh --tiles hart_yard              -> one view, 1920x1080, with Falke and Ottilie
#   ./capture.sh --tiles halm --whole           -> the WHOLE map, how a .tmap is read as a town
#   ./capture.sh --tiles halm --no-walkers      -> nobody in it
#   ./capture.sh --tiles halm 2                 -> a different scale (1..4)
#
# Light (PALETTE.md / D19) — the same colormap the phone uses, so a night shot on the Mac is the
# night the owner will see:
#
#   ./capture.sh --tiles halm --light night:0.35            -> build_desktop/tiles_halm_night.png
#   ./capture.sh --tiles halm --light night:0.35 --lantern 5  -> ..._night_lantern.png
#
set -e

# One dialogue box, drawn by the real player: ./capture.sh --dialog 0110_the_board 4 [page] [n|x]
#   n = draw it as a Narrator line (no name, no portrait, centred), x = no portrait.
if [ "$1" = "--dialog" ]; then
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -1
    cd "$ROOT"
    SPEC="$2:${3:-0}:${4:-0}${5:+:$5}"
    DIALOG_CAPTURE="$SPEC" "$ROOT/build_desktop/quest_glory" 2>&1 | grep "dialog capture" || true
    exit 0
fi

# The voxel field (VOXFIELD_NOTES.md):
#   ./capture.sh --vox halm                       -> build_desktop/vox_halm.png, 1920x1080
#   ./capture.sh --vox halm --at 21,21            -> the party stood on that cell
#   ./capture.sh --vox halm --ortho 1             -> orthographic instead of the low perspective
#   ./capture.sh --vox halm --light night:0.35    -> the same colormap the phone uses
#   ./capture.sh --vox halm --hd2d 0              -> the post pass off, for a side-by-side
#   ./capture.sh --vox halm --pitch 40 --fov 32 --viewh 9 --face N --size 1920x1080 --name spawn
# The self-test: every map loaded once, meshed and checked. Non-zero exit means a real failure.
if [ "$1" = "--vox-selftest" ]; then
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -1
    cd "$ROOT"
    OUT=$(VOX_SELFTEST=1 "$ROOT/build_desktop/quest_glory" 2>&1 | grep "SELFCHECK vox")
    echo "$OUT"
    echo "$OUT" | grep -q "selftest ok" || { echo "ERROR: voxel field self-test FAILED" >&2; exit 1; }
    exit 0
fi

if [ "$1" = "--vox" ]; then
    MAP=${2:-halm}
    shift 2 || true
    AT=""; ORTHO=""; HD2D=""; LIGHT=""; PITCH=""; FOV=""; VIEWH=""; FACE=""; SIZE="1920x1080"; NAME=""; SUFFIX=""; CUT=""; FOG=""; TILT=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --at) AT="$2"; SUFFIX="${SUFFIX}_at${2//,/-}"; shift ;;
            --ortho) ORTHO="$2"; [ "$2" = "1" ] && SUFFIX="${SUFFIX}_ortho"; shift ;;
            --hd2d) HD2D="$2"; SUFFIX="${SUFFIX}_hd$2"; shift ;;
            --light) LIGHT="$2"; SUFFIX="${SUFFIX}_${2%%:*}"; shift ;;
            --pitch) PITCH="$2"; shift ;;
            --fov) FOV="$2"; shift ;;
            --viewh) VIEWH="$2"; shift ;;
            --face) FACE="$2"; shift ;;
            --size) SIZE="$2"; shift ;;
            --name) NAME="$2"; shift ;;
            --cut) CUT="$2"; shift ;;
            --fog) FOG="$2"; shift ;;
            --tilt) TILT="$2"; shift ;;
        esac
        shift
    done
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    W="${SIZE%%x*}"; H="${SIZE##*x}"
    [ -n "$NAME" ] && SUFFIX="_$NAME"
    OUT="$ROOT/build_desktop/vox_${MAP}${SUFFIX}.png"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -2
    cd "$ROOT"
    VOX_AT="$AT" VOX_ORTHO="$ORTHO" VOX_HD2D="$HD2D" VOX_LIGHT="$LIGHT" VOX_PITCH="$PITCH" \
    VOX_FOV="$FOV" VOX_VIEWH="$VIEWH" VOX_FACE="$FACE" VOX_CUT="$CUT" VOX_FOG="$FOG" VOX_TILT="$TILT" \
        VOX_CAPTURE="$MAP:$W:$H:$OUT" "$ROOT/build_desktop/quest_glory" 2>&1 | grep -E "SELFCHECK|voxfield: (capture|walker|[0-9]+ cells)" || true
    if [ -f "$OUT" ]; then
        echo "wrote $OUT"
        exit 0
    fi
    echo "ERROR: no capture written to $OUT" >&2
    exit 1
fi

if [ "$1" = "--tiles" ]; then
    MAP=${2:-halm}
    shift 2 || true
    WHOLE=0; NOWALK=0; SCALE=3; LIGHT=""; LANTERN=""; WEATHER=""; GROUND=""; DECALS=""; FACE=""; SUFFIX=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --whole) WHOLE=1 ;;
            --no-walkers) NOWALK=1 ;;
            --light) LIGHT="$2"; SUFFIX="${SUFFIX}_${2%%:*}"; shift ;;
            --lantern) LANTERN="$2"; SUFFIX="${SUFFIX}_lantern"; shift ;;
            --weather) WEATHER="$2"; SUFFIX="${SUFFIX}_w${2//:/-}"; shift ;;
            --ground) GROUND="$2"; SUFFIX="${SUFFIX}_$2"; shift ;;
            --decals) DECALS="$2"; shift ;;
            --face) FACE="$2"; SUFFIX="${SUFFIX}_face$2"; shift ;;
            [0-9]*) SCALE="$1" ;;
        esac
        shift
    done
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    OUT="$ROOT/build_desktop/tiles_${MAP}${SUFFIX}.png"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -2
    cd "$ROOT"
    TILE_LIGHT="$LIGHT" TILE_LANTERN="$LANTERN" TILE_WEATHER="$WEATHER" TILE_GROUND="$GROUND" TILE_DECALS="$DECALS" TILE_FACE="$FACE" \
    TILE_CAPTURE_WHOLE="$WHOLE" TILE_CAPTURE_NO_WALKERS="$NOWALK" \
        TILE_CAPTURE="$MAP:$SCALE:$OUT" "$ROOT/build_desktop/quest_glory" || true
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
