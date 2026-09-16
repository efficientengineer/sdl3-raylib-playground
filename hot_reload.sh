#!/bin/bash
# Hot reload: rebuild game_logic and push to phone (or signal desktop)
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"

MODE="${1:-desktop}"

if [ "$MODE" = "android" ]; then
    echo "=== Building game_logic for Android ==="
    cd "$SCRIPT_DIR/android"
    JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home" \
        ./gradlew assembleDebug 2>&1 | tail -5

    # Extract the .so from the APK build
    SO_PATH="$SCRIPT_DIR/android/app/build/intermediates/merged_native_libs/debug/mergeDebugNativeLibs/out/lib/arm64-v8a/libgame_logic.so"
    if [ ! -f "$SO_PATH" ]; then
        echo "ERROR: libgame_logic.so not found at $SO_PATH"
        echo "Searching..."
        find "$SCRIPT_DIR/android/app/build" -name "libgame_logic.so" 2>/dev/null
        exit 1
    fi

    # Push the .so to the app's files directory (SDL_GetPrefPath = /data/data/$PKG/files/)
    echo "=== Pushing libgame_logic.so to device ==="
    $ADB -s "$DEVICE" push "$SO_PATH" /data/local/tmp/libgame_logic.so
    $ADB -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/libgame_logic.so files/libgame_logic.so"

    # Set the reload flag
    echo "=== Setting reload flag ==="
    echo "1" > /tmp/reload.flag
    $ADB -s "$DEVICE" push /tmp/reload.flag /data/local/tmp/reload.flag > /dev/null 2>&1
    $ADB -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/reload.flag files/reload.flag"

    echo "=== Hot reload pushed! Game will reload within ~1 second ==="

elif [ "$MODE" = "desktop" ]; then
    echo "=== Building game_logic for desktop ==="
    cd "$BUILD_DIR"
    make -j8 game_logic 2>&1 | tail -5
    echo "=== Desktop hot reload: library updated, game will detect within ~1 second ==="
else
    echo "Usage: $0 [desktop|android]"
    exit 1
fi
