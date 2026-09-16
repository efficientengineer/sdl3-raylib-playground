#!/bin/bash
set -e
PHONE=192.168.1.217:5555
PKG=com.playground.sdlraylib
ADB=~/Library/Android/sdk/platform-tools/adb

"$ADB" connect "$PHONE" 2>/dev/null || true

export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
cd android
./gradlew assembleDebug "$@"
cd ..

"$ADB" -s "$PHONE" install -r android/app/build/outputs/apk/debug/app-debug.apk
# A stale hot-reloaded lib in files/ would override the freshly installed APK lib.
"$ADB" -s "$PHONE" shell "run-as $PKG sh -c 'rm -f files/libgame_logic.so files/libgame_logic.so.* files/reload.flag'" || true
"$ADB" -s "$PHONE" shell am start -n "$PKG/.MainActivity"
