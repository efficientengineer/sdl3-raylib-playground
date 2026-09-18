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
GAME_SRCS="$SCRIPT_DIR/src/star_logic.cpp $SCRIPT_DIR/src/field.cpp"

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
for f in "$SCRIPT_DIR"/story/panels/*.png; do
    [ -f "$f" ] || continue
    push_art "$f" "$(basename "$f")"
done
for f in "$SCRIPT_DIR"/story/portraits/*.png; do   # cutscene_data.h names these portrait_<name>.png
    [ -f "$f" ] || continue
    push_art "$f" "portrait_$(basename "$f")"
done

# Field maps and art (FIELD.md): same push-only-if-changed rule, keeping the story/field/<kind>/ layout.
# The game looks in files/field/<kind>/<id>.png first, then the APK assets, then its placeholder.
for d in maps tiles props walkers edges buildings views; do
    [ -d "$SCRIPT_DIR/story/field/$d" ] || continue
    "$ADB" -s "$DEVICE" shell "run-as $PKG mkdir -p files/field/$d"
    HAVE_F="$("$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'cd files/field/$d && stat -c \"%n %s\" * 2>/dev/null'" | tr -d '\r')"
    for f in "$SCRIPT_DIR"/story/field/"$d"/*; do
        [ -f "$f" ] || continue
        case "$f" in *.png|*.map) ;; *) continue;; esac   # authoring scripts stay off the phone
        n="$(basename "$f")"
        sz="$(stat -f %z "$f")"
        if ! echo "$HAVE_F" | grep -qx "$n $sz"; then
            echo "  field $d/$n"
            "$ADB" -s "$DEVICE" push "$f" "/data/local/tmp/$n" >/dev/null
            "$ADB" -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/$n files/field/$d/$n && rm /data/local/tmp/$n"
        fi
    done
done

# --pull-views: bring the in-game "Capture view" output back into the repo for the painter.
if [ "$1" = "--pull-views" ]; then
    mkdir -p "$SCRIPT_DIR/story/field/views"
    NAMES="$("$ADB" -s "$DEVICE" shell "run-as $PKG ls files/field/views 2>/dev/null" | tr -d '\r')"
    for n in $NAMES; do
        "$ADB" -s "$DEVICE" shell "run-as $PKG cat files/field/views/$n" > "$SCRIPT_DIR/story/field/views/$n"
        echo "  pulled views/$n"
    done
fi

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
        case "$LINE" in *SUCCESS*|*"layout changed"*|*"failed validation"*) echo "=== Done ==="; exit 0;; esac
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
