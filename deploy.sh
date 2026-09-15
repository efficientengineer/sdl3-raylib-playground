#!/bin/bash
set -e
PHONE=192.168.1.217:5555
PKG=com.playground.sdlraylib

adb connect "$PHONE" 2>/dev/null || true

export JAVA_HOME="/Applications/Android Studio.app/Contents/jbr/Contents/Home"
cd android
./gradlew assembleDebug "$@"
cd ..

adb -s "$PHONE" install -r android/app/build/outputs/apk/debug/app-debug.apk
adb -s "$PHONE" shell am start -n "$PKG/.MainActivity"
