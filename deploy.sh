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
# THE SHIP LIST, and the APK holds nothing else. It used to rsync all of story/field/, which put the
# parked 3D art and the ingest's debug images into the APK; the assets are rebuilt from scratch each
# time so a file that leaves the list leaves the APK.
ASSETS=android/app/src/main/assets
rm -rf "$ASSETS/cutscenes" "$ASSETS/field"
mkdir -p "$ASSETS/cutscenes"
# Only the panels the current src/cutscene_data.h names — a retired scene's art is not shipped.
for n in $(grep -o '"[a-z0-9_]*\.png"' src/cutscene_data.h | tr -d '"' | sort -u); do
    [ -f "story/panels/$n" ] && cp "story/panels/$n" "$ASSETS/cutscenes/$n"
done
for f in story/portraits/*.png; do
    [ -f "$f" ] || continue
    cp "$f" "$ASSETS/cutscenes/portrait_$(basename "$f")"                 # the name cutscene_data.h uses
done

# Field (TILES.md): the tile field reads tmaps, walkers, one tileset folder and the palette. The
# parked 3D streams (screens, views, maps, props, tiles, buildings, edges) are not bundled; the old
# field says so in the log and falls back to its built-in map.
for d in tmaps walkers; do
    [ -d "story/field/$d" ] || continue
    mkdir -p "$ASSETS/field/$d"
    for f in story/field/"$d"/*; do
        case "$f" in *_debug.png|*.raw.png) continue;; esac
        case "$f" in *.png|*.tmap|*.json) cp "$f" "$ASSETS/field/$d/";; esac
    done
done
for d in story/field/tilesets/*/; do
    [ -d "$d" ] || continue
    mkdir -p "$ASSETS/field/tilesets/$(basename "$d")"
    for n in atlas.png atlas.json tiles.md masks.png masks.json; do
        [ -f "$d$n" ] && cp "$d$n" "$ASSETS/field/tilesets/$(basename "$d")/$n"
    done
    if [ -d "$d/decals" ]; then
        mkdir -p "$ASSETS/field/tilesets/$(basename "$d")/decals"
        cp "$d"decals/*.png "$ASSETS/field/tilesets/$(basename "$d")/decals/" 2>/dev/null || true
    fi
    if [ -d "$d/swatches" ]; then
        mkdir -p "$ASSETS/field/tilesets/$(basename "$d")/swatches"
        cp "$d"swatches/*.png "$ASSETS/field/tilesets/$(basename "$d")/swatches/" 2>/dev/null || true
    fi
done
if [ -d story/palette ]; then                        # PALETTE.md: the colours and the light tables
    mkdir -p "$ASSETS/palette"
    for n in master.hex master.json master.pal.png colormap.png colormap.json cycles.md; do
        [ -f "story/palette/$n" ] && cp "story/palette/$n" "$ASSETS/palette/$n"
    done
fi

export JAVA_HOME="/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home"
cd android
./gradlew assembleDebug "$@"
cd ..

"$ADB" -s "$PHONE" install -r android/app/build/outputs/apk/debug/app-debug.apk
# A stale hot-reloaded lib in files/ would override the freshly installed APK lib.
"$ADB" -s "$PHONE" shell "run-as $PKG sh -c 'rm -f files/libgame_logic.so files/libgame_logic.so.* files/reload.flag; rm -rf files/cutscenes files/field files/palette files/com.playground'" || true
"$ADB" -s "$PHONE" shell am start -n "$PKG/.MainActivity"

APK=android/app/build/outputs/apk/debug/app-debug.apk
echo "=== APK $(du -h "$APK" | awk '{print $1}')   assets $(du -sh "$ASSETS" | awk '{print $1}') ==="
du -sh "$ASSETS"/* 2>/dev/null | sed 's/^/    /'
