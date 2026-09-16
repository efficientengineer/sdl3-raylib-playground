#!/bin/bash
# Fast hot reload: compile game_logic.so directly with NDK, skip gradle
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"
NDK="$HOME/Library/Android/sdk/ndk/27.0.12077973"
CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang++"
CXX_DIR="$SCRIPT_DIR/android/app/.cxx/Debug/931y536w/arm64-v8a"
OBJ_DIR="$CXX_DIR/obj_fast"
mkdir -p "$OBJ_DIR"

IMGUI_SO="$SCRIPT_DIR/android/app/build/intermediates/cxx/Debug/931y536w/obj/arm64-v8a/libimgui_shared.so"
SDL3_SO="$SCRIPT_DIR/android/app/build/intermediates/cxx/Debug/931y536w/obj/arm64-v8a/libSDL3.so"
SDL3_INC="$CXX_DIR/_deps/sdl3-src/include"
SDL3_BUILD_INC="$CXX_DIR/_deps/sdl3-build/include-revision"

echo "=== Compiling game_logic.cpp ==="
$CC -std=c++17 -O0 -g -DANDROID -DIMGUI_IMPL_OPENGL_ES3 -Dgame_logic_EXPORTS \
    -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong \
    -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -fPIC -fvisibility=hidden \
    -I"$SCRIPT_DIR/src" \
    -I"$SCRIPT_DIR/third_party" \
    -I"$SCRIPT_DIR/third_party/imgui" \
    -I"$SCRIPT_DIR/third_party/imgui/backends" \
    -I"$SDL3_BUILD_INC" \
    -I"$SDL3_INC" \
    -c "$SCRIPT_DIR/src/game_logic.cpp" -o "$OBJ_DIR/game_logic.o"

echo "=== Linking libgame_logic.so ==="
$CC -shared -static-libstdc++ \
    -Wl,--build-id=sha1 -Wl,--no-rosegment -Wl,--no-undefined-version \
    -Wl,--fatal-warnings -Wl,--no-undefined -Wl,-z,max-page-size=16384 \
    "$OBJ_DIR/game_logic.o" \
    "$IMGUI_SO" "$SDL3_SO" \
    -lEGL -lGLESv3 -landroid -llog -latomic -lm \
    -o "$OBJ_DIR/libgame_logic.so"

echo "=== Pushing to device ==="
$ADB -s "$DEVICE" push "$OBJ_DIR/libgame_logic.so" /data/local/tmp/libgame_logic.so
$ADB -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/libgame_logic.so files/libgame_logic.so"

echo "=== Setting reload flag ==="
echo "1" > /tmp/reload.flag
$ADB -s "$DEVICE" push /tmp/reload.flag /data/local/tmp/reload.flag > /dev/null 2>&1
$ADB -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/reload.flag files/reload.flag"

echo "=== Done ==="
