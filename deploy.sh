#!/bin/bash
set -e
PHONE=192.168.1.217:5555
PKG=com.playground.sdlraylib
ADB=~/Library/Android/sdk/platform-tools/adb

"$ADB" connect "$PHONE" 2>/dev/null || true

# Bundle scene data, panel art, and speaker portraits into the APK (the copies are generated;
# story/ is the source). Portraits run first: the export records which ones exist.
./story_prompt.py portraits
./story_prompt.py export
mkdir -p android/app/src/main/assets/cutscenes
# portrait_*.png is excluded so --delete leaves the portraits copied in below alone.
rsync -a --delete --exclude='portrait_*.png' --include='*.png' --exclude='*' story/panels/ android/app/src/main/assets/cutscenes/
for f in story/portraits/*.png; do
    [ -f "$f" ] || continue
    d="android/app/src/main/assets/cutscenes/portrait_$(basename "$f")"   # the name cutscene_data.h uses
    cmp -s "$f" "$d" || cp "$f" "$d"
done

# Field maps and art (FIELD.md): bundled under assets/field/<kind>/, which is where the game looks
# after the phone's files/ copy that fast_reload.sh pushes.
if [ -d story/field ]; then
    mkdir -p android/app/src/main/assets/field
    rsync -a --delete story/field/ android/app/src/main/assets/field/
fi

export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
cd android
./gradlew assembleDebug "$@"
cd ..

"$ADB" -s "$PHONE" install -r android/app/build/outputs/apk/debug/app-debug.apk
# A stale hot-reloaded lib in files/ would override the freshly installed APK lib.
"$ADB" -s "$PHONE" shell "run-as $PKG sh -c 'rm -f files/libgame_logic.so files/libgame_logic.so.* files/reload.flag; rm -rf files/cutscenes files/field'" || true
"$ADB" -s "$PHONE" shell am start -n "$PKG/.MainActivity"
