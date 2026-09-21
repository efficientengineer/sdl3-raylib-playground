#!/bin/bash
# Capture a view of the game on the Mac, with no phone involved: builds the desktop host + game lib
# and runs the real renderer once. See the mode headers below (--vox, --dialog, --chapter-selftest,
# --battle-selftest, --vox-selftest, --vox-walktest).
#
# The window flashes up for a moment; that is the renderer doing its one frame.
#
# Every Mac run of the game is a TEST run, so it is silent by default: STAR_MUTE forces mute at
# startup whatever settings.ini says, and the forced state is never written back to the file.
# The Settings window shows "muted by STAR_MUTE" and the owner can still untick it for the session.
export STAR_MUTE=1

# One dialogue box, drawn by the real player: ./capture.sh --dialog 0110_the_board 4 [page] [n|x|s|t]
#   n = draw it as a Narrator line (no name, no portrait, centred), x = no portrait,
#   s = the short box (pagination and the double marker), t = TEXTLESS (name and portrait, no words).
# DIALOG_EXPR=<expr> forces one expression portrait on every line (neutral, smile, laugh, biglaugh,
# concern, sorrow, annoyed, angry, shock, resolve), falling back to the plain portrait when the file
# has not been drawn:  DIALOG_EXPR=sorrow ./capture.sh --dialog 0110_the_board 1
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
#   ./capture.sh --vox halm --nav 1               -> the navmesh overlay drawn into the shot
# The movement bot: the real movement code driven against walls, path-walked to every exit, door and
# NPC, and jumped 200 times, on one map or all three. Non-zero exit means a real failure.
if [ "$1" = "--vox-walktest" ]; then
    MAP=${2:-all}
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -1
    cd "$ROOT"
    OUT=$(VOX_WALKTEST="$MAP" "$ROOT/build_desktop/quest_glory" 2>&1 | grep -E "WALKTEST")
    echo "$OUT"
    echo "$OUT" | grep -q "WALKTEST verdict: ok" || { echo "ERROR: voxel field walk test FAILED" >&2; exit 1; }
    exit 0
fi

# The chapter self-test: the whole of chapter one played through the chapter script's own entry
# points, plus the battle self-test, with no window and no input. It proves the STORY wiring — every
# step's gate opens, every clip exists in cutscene_data.h, all ten mandatory interactions fire, the
# end card is reached, the swing is unwinnable by attacking and winnable by the parry. It does NOT
# prove the player can walk to each trigger — that is `--vox-walktest`. Run both.
if [ "$1" = "--chapter-selftest" ] || [ "$1" = "--battle-selftest" ]; then
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -1
    cd "$ROOT"
    if [ "$1" = "--battle-selftest" ]; then VAR=BATTLE_SELFTEST; PAT="SELFCHECK battle"; else VAR=CHAPTER_SELFTEST; PAT="CHAPTER|SELFCHECK (chapter|battle)|^ "; fi
    OUT=$(env "$VAR=1" "$ROOT/build_desktop/quest_glory" 2>&1 | grep -E "$PAT")
    echo "$OUT"
    echo "$OUT" | grep -qE "SELFCHECK (chapter|battle) ok" || { echo "ERROR: $1 FAILED" >&2; exit 1; }
    exit 0
fi

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

# The battle UI test: the REAL battle screen, in a real window, driven by injected TAPS. The
# selftests call the battle API directly and never touch input, which is why they could not see the
# owner's "stuck after attacking". This one taps every command at every effort, picks targets, drags
# the effort slider, and asserts the battle accepts input again within 5 s after EVERY action. It
# also prints the party-per-chapter-step table and checks that each CLIP hands back to gameplay.
if [ "$1" = "--battle-ui-test" ]; then
    ROOT="$(cd "$(dirname "$0")" && pwd)"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -1
    cd "$ROOT"
    OUT=$(BATTLE_UI_TEST=1 "$ROOT/build_desktop/quest_glory" 2>&1 | grep -E "UITEST|SELFCHECK battle-ui|battle: WATCHDOG")
    echo "$OUT"
    echo "$OUT" | grep -q "SELFCHECK battle-ui ok" || { echo "ERROR: --battle-ui-test FAILED" >&2; exit 1; }
    exit 0
fi

if [ "$1" = "--vox" ]; then
    MAP=${2:-halm}
    shift 2 || true
    AT=""; ORTHO=""; HD2D=""; LIGHT=""; PITCH=""; FOV=""; VIEWH=""; FACE=""; SIZE="1920x1080"; NAME=""; SUFFIX=""; CUT=""; FOG=""; TILT=""; NAV=""; RADIUS=""
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
            --nav) NAV="$2"; [ "$2" = "1" ] && SUFFIX="${SUFFIX}_nav"; shift ;;
            --radius) RADIUS="$2"; shift ;;
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
    VOX_NAV="$NAV" VOX_RADIUS="$RADIUS" \
        VOX_CAPTURE="$MAP:$W:$H:$OUT" "$ROOT/build_desktop/quest_glory" 2>&1 | grep -E "SELFCHECK|OVERDRAW|voxfield: (capture|walker|[0-9]+ cells)" || true
    if [ -f "$OUT" ]; then
        echo "wrote $OUT"
        exit 0
    fi
    echo "ERROR: no capture written to $OUT" >&2
    exit 1
fi

echo "usage: capture.sh --vox|--vox-selftest|--vox-walktest|--dialog|--chapter-selftest|--battle-selftest|--battle-ui-test ..." >&2
exit 2
