#!/bin/bash
# Cross-compile every file in fast_reload.sh's GAME_SRCS list with fast_reload.sh's exact command.
# Objects go to /tmp; the device is never touched and fast_reload.sh is never run.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
NDK="$HOME/Library/Android/sdk/ndk/27.0.12077973"
CC="$NDK/toolchains/llvm/prebuilt/darwin-x86_64/bin/aarch64-linux-android24-clang++"
CXX_DIR="$(ls -d "$SCRIPT_DIR"/android/app/.cxx/Debug/*/arm64-v8a 2>/dev/null | head -1)"
OBJ_DIR=/tmp/ndkcheck_obj; mkdir -p "$OBJ_DIR"
SDL3_INC="$CXX_DIR/_deps/sdl3-src/include"
SDL3_BUILD_INC="$CXX_DIR/_deps/sdl3-build/include-revision"
# Read the list out of fast_reload.sh itself so the two can never drift.
SRCS=$(awk '/^for F in /,/; do$/' "$SCRIPT_DIR/fast_reload.sh" | tr ' ;' '\n\n' | grep '\.cpp$')
echo "sources: $(echo $SRCS | wc -w | tr -d ' ')"
for F in $SRCS; do
    printf '  %-24s' "$F"
    $CC -std=c++17 -O0 -g -DANDROID -DIMGUI_IMPL_OPENGL_ES3 -Dgame_logic_EXPORTS \
        -fdata-sections -ffunction-sections -funwind-tables -fstack-protector-strong \
        -no-canonical-prefixes -D_FORTIFY_SOURCE=2 -fPIC -fvisibility=hidden \
        -I"$SCRIPT_DIR/src" -I"$SCRIPT_DIR/third_party" \
        -I"$SCRIPT_DIR/third_party/imgui" -I"$SCRIPT_DIR/third_party/imgui/backends" \
        -I"$SDL3_BUILD_INC" -I"$SDL3_INC" \
        -c "$SCRIPT_DIR/src/$F" -o "$OBJ_DIR/${F%.cpp}.o" && echo ok
done
echo "NDK cross-compile: all ok"
