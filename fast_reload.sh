#!/bin/bash
# Fast hot reload: compile game_logic.so directly with NDK, skip gradle
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"
NDK="$HOME/Library/Android/sdk/ndk/27.0.12077973"
CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang++"
# Gradle names the CMake dir with a per-checkout hash, so find it rather than hard-coding it.
CXX_DIR="$(ls -d "$SCRIPT_DIR"/android/app/.cxx/Debug/*/arm64-v8a 2>/dev/null | head -1)"
if [ -z "$CXX_DIR" ]; then
    echo "ERROR: no Android build found in this checkout. Run ./deploy.sh once first." >&2
    exit 1
fi
HASH="$(basename "$(dirname "$CXX_DIR")")"
OBJ_DIR="$CXX_DIR/obj_fast"
mkdir -p "$OBJ_DIR"
# Every translation unit of libgame_logic.so. CMakeLists.txt's game_logic target must match.
GAME_SRCS="$SCRIPT_DIR/src/star_logic.cpp $SCRIPT_DIR/src/field.cpp $SCRIPT_DIR/src/tilefield.cpp $SCRIPT_DIR/src/voxfield.cpp"

IMGUI_SO="$SCRIPT_DIR/android/app/build/intermediates/cxx/Debug/$HASH/obj/arm64-v8a/libimgui_shared.so"
SDL3_SO="$SCRIPT_DIR/android/app/build/intermediates/cxx/Debug/$HASH/obj/arm64-v8a/libSDL3.so"

# Speaker portraits are cut from the reference sheets; scene text and panel layout are compiled in.
# Regenerate both from story/ on every reload (portraits first: export records which ones exist).
"$SCRIPT_DIR/story_prompt.py" portraits
"$SCRIPT_DIR/story_prompt.py" export
SDL3_INC="$CXX_DIR/_deps/sdl3-src/include"
SDL3_BUILD_INC="$CXX_DIR/_deps/sdl3-build/include-revision"

OBJS=""
for SRC in $GAME_SRCS; do
    OBJ="$OBJ_DIR/$(basename "${SRC%.cpp}").o"
    OBJS="$OBJS $OBJ"
    echo "=== Compiling $(basename "$SRC") ==="
    $CC -std=c++17 -O0 -g -DANDROID -DIMGUI_IMPL_OPENGL_ES3 -Dgame_logic_EXPORTS \
        -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong \
        -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -fPIC -fvisibility=hidden \
        -I"$SCRIPT_DIR/src" \
        -I"$SCRIPT_DIR/third_party" \
        -I"$SCRIPT_DIR/third_party/imgui" \
        -I"$SCRIPT_DIR/third_party/imgui/backends" \
        -I"$SDL3_BUILD_INC" \
        -I"$SDL3_INC" \
        -c "$SRC" -o "$OBJ"
done

echo "=== Linking libgame_logic.so ==="
$CC -shared -static-libstdc++ \
    -Wl,--build-id=sha1 -Wl,--no-rosegment -Wl,--no-undefined-version \
    -Wl,--fatal-warnings -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
    $OBJS \
    "$IMGUI_SO" "$SDL3_SO" \
    -lEGL -lGLESv3 -landroid -llog -latomic -lm \
    -o "$OBJ_DIR/libgame_logic.so"

echo "=== Pushing to device ==="
"$ADB" connect "$DEVICE" >/dev/null 2>&1 || true
if ! "$ADB" -s "$DEVICE" get-state >/dev/null 2>&1; then
    echo "ERROR: $DEVICE not connected. Run: $ADB connect $DEVICE" >&2
    exit 1
fi

# THE SHIP LIST. Everything this run puts on the phone is recorded here, and afterwards the device is
# made to match it: anything in files/cutscenes or files/field that this run would not ship is deleted.
# Without that the phone only ever accumulates — 69 MB of app data against an 8.7 MB APK, most of it
# art from retired scenes and the parked 3D field.
SHIP_CUT="$(mktemp)"; SHIP_FIELD="$(mktemp)"
trap 'rm -f "$SHIP_CUT" "$SHIP_FIELD"' EXIT
BEFORE_KB="$("$ADB" -s "$DEVICE" shell "run-as $PKG du -sk files" | awk '{print $1}' | tr -d '\r')"

# The parked streams (the old 3D field: FIELD.md's screens, views, maps, props, tiles, buildings,
# edges) are not pushed any more — nothing the tile field draws reads them, and they were 11 MB.
# `--with-parked` puts them back for a session on the old field; without them its Dev button finds no
# map and says so in the log rather than crashing.
WITH_PARKED=0
for a in "$@"; do [ "$a" = "--with-parked" ] && WITH_PARKED=1; done
FIELD_KINDS="tmaps walkers"
[ "$WITH_PARKED" = 1 ] && FIELD_KINDS="tmaps walkers maps screens views props tiles buildings edges"

# Panel images and speaker portraits: push only the ones the device doesn't already have at the same size.
"$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/cutscenes"
HAVE="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/cutscenes && stat -c \"%n %s\" *.png 2>/dev/null'" | tr -d '\r')"
push_art() {                                   # $1 = local file, $2 = name on the device
    local f="$1" n="$2" sz
    sz="$(stat -f %z "$f")"
    if ! echo "$HAVE" | grep -qx "$n $sz"; then
        echo "  art $n"
        "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
        "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/cutscenes/$n && rm /data/local/tmp/$n"
    fi
}
# Only the panels the CURRENT src/cutscene_data.h names: a retired scene's art is not shipped.
WANTED_PANELS="$(grep -o '"[a-z0-9_]*\.png"' "$SCRIPT_DIR/src/cutscene_data.h" | tr -d '"' | sort -u)"
for n in $WANTED_PANELS; do
    f="$SCRIPT_DIR/story/panels/$n"
    echo "$n" >> "$SHIP_CUT"
    [ -f "$f" ] || continue
    push_art "$f" "$n"
done
for f in "$SCRIPT_DIR"/story/portraits/*.png; do   # cutscene_data.h names these portrait_<name>.png
    [ -f "$f" ] || continue
    echo "portrait_$(basename "$f")" >> "$SHIP_CUT"
    push_art "$f" "portrait_$(basename "$f")"
done

# Field maps and art (FIELD.md): same push-only-if-changed rule, keeping the story/field/<kind>/ layout.
# The game looks in files/field/<kind>/<id>.png first, then the APK assets, then its placeholder.
for d in $FIELD_KINDS; do
    [ -d "$SCRIPT_DIR/story/field/$d" ] || continue
    "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/field/$d"
    HAVE_F="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/$d && stat -c \"%n %s\" * 2>/dev/null'" | tr -d '\r')"
    for f in "$SCRIPT_DIR"/story/field/"$d"/*; do
        [ -f "$f" ] || continue
        # authoring scripts stay off the phone; screens carry .screen and .triggers text files
        case "$f" in *_debug.png) continue;; esac          # the ingest's own check image, not art
        case "$f" in *.png|*.map|*.tmap|*.screen|*.triggers|*.json) ;; *) continue;; esac
        n="$(basename "$f")"
        echo "$d/$n" >> "$SHIP_FIELD"
        sz="$(stat -f %z "$f")"
        # Text files are tiny and are regenerated in place: a `.screen` can change completely and
        # keep its byte count, which the size check below cannot see. Always push those.
        always=""
        case "$f" in *.screen|*.triggers|*.map|*.tmap|*.json) always=1;; esac
        if [ -n "$always" ] || ! echo "$HAVE_F" | grep -qx "$n $sz"; then
            echo "  field $d/$n"
            "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
            "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/field/$d/$n && rm /data/local/tmp/$n"
        fi
    done
done

# Tilesets (TILES.md) are a folder each, not a flat dir: atlas.png and tiles.md per set. tiles.md is
# small and is rewritten in place, so it always goes; the atlas only when its size changed.
for d in "$SCRIPT_DIR"/story/field/tilesets/*/; do
    [ -d "$d" ] || continue
    set_name="$(basename "$d")"
    "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/field/tilesets/$set_name"
    HAVE_T="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/tilesets/$set_name && stat -c \"%n %s\" * 2>/dev/null'" | tr -d '\r')"
    for f in "$d"atlas.png "$d"atlas.json "$d"tiles.md "$d"masks.png "$d"masks.json; do
        [ -f "$f" ] || continue
        n="$(basename "$f")"
        echo "tilesets/$set_name/$n" >> "$SHIP_FIELD"
        sz="$(stat -f %z "$f")"
        if [ "$n" = "tiles.md" ] || [ "$n" = "atlas.json" ] || [ "$n" = "masks.json" ] || ! echo "$HAVE_T" | grep -qx "$n $sz"; then
            echo "  tileset $set_name/$n"
            "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
            "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/field/tilesets/$set_name/$n && rm /data/local/tmp/$n"
        fi
    done
    # The voxel field's detail billboards (VOXFIELD_NOTES.md) come from the tileset's decals/.
    if [ -d "$d/decals" ]; then
        "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/field/tilesets/$set_name/decals"
        HAVE_D="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/tilesets/$set_name/decals && stat -c \"%n %s\" * 2>/dev/null'" | tr -d '\r')"
        for f in "$d"decals/*.png; do
            [ -f "$f" ] || continue
            n="$(basename "$f")"
            echo "tilesets/$set_name/decals/$n" >> "$SHIP_FIELD"
            sz="$(stat -f %z "$f")"
            if ! echo "$HAVE_D" | grep -qx "$n $sz"; then
                echo "  decal $set_name/$n"
                "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
                "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/field/tilesets/$set_name/decals/$n && rm /data/local/tmp/$n"
            fi
        done
    fi
    # TILES2: the terrain swatches live in their own folder beside the atlas.
    if [ -d "$d/swatches" ]; then
        "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/field/tilesets/$set_name/swatches"
        HAVE_S="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/tilesets/$set_name/swatches && stat -c \"%n %s\" * 2>/dev/null'" | tr -d '\r')"
        for f in "$d"swatches/*.png; do
            [ -f "$f" ] || continue
            n="$(basename "$f")"
            echo "tilesets/$set_name/swatches/$n" >> "$SHIP_FIELD"
            sz="$(stat -f %z "$f")"
            if ! echo "$HAVE_S" | grep -qx "$n $sz"; then
                echo "  swatch $set_name/$n"
                "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
                "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/field/tilesets/$set_name/swatches/$n && rm /data/local/tmp/$n"
            fi
        done
    fi
done

# The palette (PALETTE.md / D19): the master colours, the colormap the shader reads its light from and
# the cycle list. Small files that are rewritten in place, so they always go.
if [ -d "$SCRIPT_DIR/story/palette" ]; then
    "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/palette"
    for n in master.hex master.json master.pal.png colormap.png colormap.json cycles.md; do
        f="$SCRIPT_DIR/story/palette/$n"
        [ -f "$f" ] || continue
        echo "  palette $n"
        "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
        "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/palette/$n && rm /data/local/tmp/$n"
    done
fi

# --pull-views: bring the in-game "Capture view" output back into the repo for the painter.
if [ "$1" = "--pull-views" ]; then
    mkdir -p "$SCRIPT_DIR/story/field/views"
    NAMES="$("$ADB" -s "$DEVICE" shell "run-as $PKG ls files/field/views 2>/dev/null" | tr -d '\r')"
    for n in $NAMES; do
        case "$n" in *_depth.png) continue;; esac     # depth stays on the phone: reference, not art
        "$ADB" -s "$DEVICE" shell "run-as $PKG cat files/field/views/$n" > "$SCRIPT_DIR/story/field/views/$n"
        echo "  pulled views/$n"
    done
fi

# ── PRUNE: make the device mirror the ship list ────────────────────────────────────────────────
# Anything in files/cutscenes or files/field this run would not ship is deleted. Panels belong to
# retired scenes, field art belongs to the parked 3D streams; the phone had 69 MB of it.
echo "$SHIP_FIELD" > /dev/null
PRUNED=0
DEV_CUT="$("$ADB" -s "$DEVICE" shell "run-as $PKG ls files/cutscenes 2>/dev/null" | tr -d '\r')"
for n in $DEV_CUT; do
    grep -qx "$n" "$SHIP_CUT" && continue
    echo "  prune cutscenes/$n"
    "$ADB" -s "$DEVICE" shell "run-as $PKG rm -f files/cutscenes/$n"
    PRUNED=$((PRUNED + 1))
done
DEV_FIELD="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field 2>/dev/null && ls -d */ 2>/dev/null'" | tr -d '\r/')"
for d in $DEV_FIELD; do
    if [ "$d" = "tilesets" ]; then
        for sub in $("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/tilesets && ls -d */ 2>/dev/null'" | tr -d '\r/'); do
            for n in $("$ADB" -s "$DEVICE" shell "run-as $PKG ls files/field/tilesets/$sub 2>/dev/null" | tr -d '\r'); do
                { [ "$n" = "swatches" ] || [ "$n" = "decals" ]; } && continue        # a folder, and its files are listed by path
                grep -qx "tilesets/$sub/$n" "$SHIP_FIELD" && continue
                echo "  prune field/tilesets/$sub/$n"
                "$ADB" -s "$DEVICE" shell "run-as $PKG rm -f files/field/tilesets/$sub/$n"
                PRUNED=$((PRUNED + 1))
            done
        done
        continue
    fi
    case " $FIELD_KINDS " in
        *" $d "*) ;;                                  # a kind this run ships: check it file by file
        *) echo "  prune field/$d (parked)"           # a kind it does not: the whole folder goes
           "$ADB" -s "$DEVICE" shell "run-as $PKG rm -rf files/field/$d"
           PRUNED=$((PRUNED + 1)); continue;;
    esac
    for n in $("$ADB" -s "$DEVICE" shell "run-as $PKG ls files/field/$d 2>/dev/null" | tr -d '\r'); do
        grep -qx "$d/$n" "$SHIP_FIELD" && continue
        echo "  prune field/$d/$n"
        "$ADB" -s "$DEVICE" shell "run-as $PKG rm -f files/field/$d/$n"
        PRUNED=$((PRUNED + 1))
    done
done

# Stale hot-reload copies. A copy belonging to a DEAD pid is unreferenced; for the live pid every
# copy but the newest gen is already mapped, so unlinking it is safe (the mapping outlives the name).
# The host unlinks its own copy the moment it dlopens it now — this is for builds from before that.
LIVE_PID="$("$ADB" -s "$DEVICE" shell pidof $PKG | tr -d '\r' | awk '{print $1}')"
COPIES="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'ls files/libgame_logic.so.*.* 2>/dev/null'" | tr -d '\r')"
NEWEST_GEN=-1
for c in $COPIES; do
    p="$(echo "$c" | awk -F. '{print $(NF-1)}')"; g="$(echo "$c" | awk -F. '{print $NF}')"
    [ "$p" = "$LIVE_PID" ] && [ "$g" -gt "$NEWEST_GEN" ] && NEWEST_GEN="$g"
done
for c in $COPIES; do
    p="$(echo "$c" | awk -F. '{print $(NF-1)}')"; g="$(echo "$c" | awk -F. '{print $NF}')"
    if [ "$p" = "$LIVE_PID" ] && [ "$g" = "$NEWEST_GEN" ]; then continue; fi
    "$ADB" -s "$DEVICE" shell "run-as $PKG rm -f $c"
    PRUNED=$((PRUNED + 1))
done

# Left over from a run whose pref path was relative: nothing reads it.
"$ADB" -s "$DEVICE" shell "run-as $PKG rm -rf files/com.playground"

AFTER_KB="$("$ADB" -s "$DEVICE" shell "run-as $PKG du -sk files" | awk '{print $1}' | tr -d '\r')"
echo "=== Pruned $PRUNED item(s); files/ ${BEFORE_KB} KB -> ${AFTER_KB} KB ==="

LOCAL_MD5=$(md5 -q "$OBJ_DIR/libgame_logic.so")
"$ADB" -s "$DEVICE" push "$OBJ_DIR/libgame_logic.so" /data/local/tmp/libgame_logic.so
# Copy to a temp name then mv so the host never dlopens a half-written file.
"$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cp /data/local/tmp/libgame_logic.so files/libgame_logic.so.tmp && mv files/libgame_logic.so.tmp files/libgame_logic.so'"
REMOTE_MD5=$("$ADB" -s "$DEVICE" shell "run-as $PKG md5sum files/libgame_logic.so" | cut -d' ' -f1)
if [ "$LOCAL_MD5" != "$REMOTE_MD5" ]; then
    echo "ERROR: checksum mismatch after push (local $LOCAL_MD5, device $REMOTE_MD5)" >&2
    exit 1
fi

if [ -z "$("$ADB" -s "$DEVICE" shell pidof $PKG)" ]; then
    echo "=== App not running; launching (loads new lib at startup) ==="
    "$ADB" -s "$DEVICE" logcat -c
    "$ADB" -s "$DEVICE" shell am start -n "$PKG/.MainActivity" >/dev/null
    sleep 5
    if [ -z "$("$ADB" -s "$DEVICE" shell pidof $PKG)" ]; then
        echo "ERROR: app died on launch. Backtrace:" >&2
        "$ADB" -s "$DEVICE" logcat -d | grep -E "F DEBUG|SDL/APP|QuestGlory" | tail -30 >&2
        exit 1
    fi
    echo "=== Done (launched fresh) ==="
    exit 0
fi

echo "=== Setting reload flag ==="
"$ADB" -s "$DEVICE" logcat -c
echo "1" > /tmp/reload.flag
"$ADB" -s "$DEVICE" push /tmp/reload.flag /data/local/tmp/reload.flag > /dev/null 2>&1
"$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/reload.flag files/reload.flag"

echo "=== Waiting for host to apply reload ==="
for i in $(seq 1 12); do
    sleep 0.5
    LINE=$("$ADB" -s "$DEVICE" logcat -d -s QuestGlory 2>/dev/null | grep -E "Hot reload (SUCCESS|FAILED|SKIPPED)|layout changed|failed validation" | tail -1)
    if [ -n "$LINE" ]; then
        echo "$LINE"
        case "$LINE" in *SUCCESS*|*"layout changed"*|*"failed validation"*)
            # The device's own summary of what it actually loaded. The Mac reads the repo directly, so
            # this line is the only place a missing mask or swatch on the PHONE shows up.
            sleep 2
            CHECK="$("$ADB" -s "$DEVICE" logcat -d -s SDL/APP 2>/dev/null | grep "SELFCHECK" | tail -1)"
            if [ -n "$CHECK" ]; then
                echo "${CHECK#*SDL/APP : }"
                case "$CHECK" in *"ground quads 0,"*)
                    echo "ERROR: the phone built NO ground quads — masks or swatches are missing on the device" >&2
                    exit 1;; esac
                case "$CHECK" in *"masks MISSING"*)
                    echo "WARNING: masks.json/png did not load on the device; terrains are square cells" >&2;; esac
            fi
            echo "=== Done ==="; exit 0;; esac
        exit 1
    fi
    if [ -z "$("$ADB" -s "$DEVICE" shell pidof $PKG)" ]; then
        echo "ERROR: app crashed during reload. Backtrace:" >&2
        "$ADB" -s "$DEVICE" logcat -d | grep -E "F DEBUG|SDL/APP|QuestGlory" | tail -30 >&2
        exit 1
    fi
done
TOP="$("$ADB" -s "$DEVICE" shell dumpsys activity activities | grep -m1 topResumedActivity)"
if ! echo "$TOP" | grep -q "$PKG"; then
    # Android freezes apps that sit in the background for a while; a frozen process cannot poll.
    echo "=== Queued: the game is in the background and Android has frozen it. ==="
    echo "    The new build and reload flag are on the device; it reloads the moment the game is opened."
    exit 0
fi
echo "ERROR: no reload confirmation in 6s. Check: $ADB shell pidof $PKG; $ADB logcat -d -s QuestGlory" >&2
exit 1
