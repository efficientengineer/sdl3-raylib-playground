#!/bin/bash
# perf.sh — drive the voxel field's A/B benchmark and bring the report back to the Mac.
# src/VOXFIELD_NOTES.md, "Performance", is the contract; src/vxperf.h is the instrumentation.
#
#   ./perf.sh [map]        run it on the phone (default map: halm), pull the report, print the table
#   ./perf.sh --desktop [map]   run the same benchmark in the desktop build
#   ./perf.sh --watch      tail the periodic `voxfield perf:` frame lines from the phone
#   ./perf.sh --hud        just tail everything the field logs (perf lines and PERF rows)
#
# The benchmark parks the party on a fixed view, runs every configuration for 30 warm-up + 240
# measured frames at two views, and writes perf_report.md / perf_report.csv into the app's pref dir.
# It takes a few minutes and moves the camera, so it only ever runs when it is asked for.
set -e

# Every Mac run of the game is a TEST run, so it is silent by default: STAR_MUTE forces mute at
# startup whatever settings.ini says, and the forced state is never written back to the file.
# The Settings window shows "muted by STAR_MUTE" and the owner can still untick it for the session.
export STAR_MUTE=1

ROOT="$(cd "$(dirname "$0")" && pwd)"
ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"
OUT_DIR="$ROOT/build_desktop/perf"
DATE="$(date +%Y%m%d_%H%M)"
mkdir -p "$OUT_DIR"

adb_ready() {
    [ -x "$ADB" ] || { echo "perf.sh: no adb at $ADB"; exit 1; }
    "$ADB" connect "$DEVICE" > /dev/null 2>&1 || true
    "$ADB" -s "$DEVICE" shell true > /dev/null 2>&1 \
        || { echo "perf.sh: device $DEVICE is not reachable"; exit 1; }
}

# ───────────────────────── --watch / --hud ─────────────────────────
if [ "$1" = "--watch" ] || [ "$1" = "--hud" ]; then
    adb_ready
    echo "perf.sh: tailing the field's frame lines (ctrl-C to stop)"
    exec "$ADB" -s "$DEVICE" logcat -v time | grep --line-buffered -E "voxfield perf:|^.*PERF "
fi

# ───────────────────────── --desktop ─────────────────────────
if [ "$1" = "--desktop" ]; then
    MAP="${2:-halm}"
    PREF="$HOME/Library/Application Support/com.playground/questglory"
    mkdir -p "$PREF"
    printf '%s' "$MAP" > "$PREF/perf.flag"
    echo "perf.sh: desktop benchmark on $MAP — the window will run for a few minutes"
    LOG="$OUT_DIR/${DATE}_desktop.log"
    cmake -B "$ROOT/build_desktop" -S "$ROOT" -DCMAKE_BUILD_TYPE=Debug > /dev/null
    cmake --build "$ROOT/build_desktop" --target quest_glory game_logic \
        -j"$(sysctl -n hw.ncpu)" | tail -1
    # The field has to be open for the flag to be polled: map.flag is what opens it.
    printf '%s' "$MAP" > "$PREF/map.flag"
    ( "$ROOT/build_desktop/quest_glory" > "$LOG" 2>&1 & echo $! > "$OUT_DIR/.pid" ) || true
    PID="$(cat "$OUT_DIR/.pid")"
    for _ in $(seq 1 900); do
        grep -q "PERF done" "$LOG" 2>/dev/null && break
        kill -0 "$PID" 2>/dev/null || break
        sleep 1
    done
    kill "$PID" 2>/dev/null || true
    REPORT="$OUT_DIR/${DATE}_desktop.md"
    if [ -f "$PREF/perf_report.md" ]; then
        cp "$PREF/perf_report.md" "$REPORT"
        cp "$PREF/perf_report.csv" "$OUT_DIR/${DATE}_desktop.csv" 2>/dev/null || true
        echo; cat "$REPORT"
        echo; echo "perf.sh: $REPORT"
    else
        echo "perf.sh: no report was written — see $LOG"
        grep -E "PERF|voxfield perf" "$LOG" | tail -30 || true
        exit 1
    fi
    exit 0
fi

# ───────────────────────── the phone ─────────────────────────
MAP="${1:-halm}"
adb_ready

# Always bring it to the front, not just when pidof fails: a process Android has moved to its cached
# state still HAS a pid, and a frozen process cannot poll the flag. That is what a silent run looks
# like — the flag sits there unconsumed and perf.sh waits out its whole timeout for nothing.
echo "perf.sh: bringing the app to the front"
"$ADB" -s "$DEVICE" shell monkey -p "$PKG" 1 > /dev/null 2>&1
sleep 5

# The field has to be open for the flag to be polled, and map.flag is what opens it.
"$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'printf %s \"$MAP\" > files/map.flag'"
"$ADB" -s "$DEVICE" shell "run-as $PKG sh -c 'printf %s \"$MAP\" > files/perf.flag'"
echo "perf.sh: benchmark requested on $MAP — this takes a few minutes, leave the phone alone"

LOG="$OUT_DIR/${DATE}_phone.log"
"$ADB" -s "$DEVICE" logcat -c > /dev/null 2>&1 || true
"$ADB" -s "$DEVICE" logcat -v time > "$LOG" 2>&1 &
TAIL_PID=$!

# Keep the screen awake for the length of the run. Without this the phone sleeps a couple of minutes
# in, Android freezes the process, and the benchmark stops halfway through with no error anywhere —
# which is exactly how the first attempt at this was lost. WAKEUP plus a monkey launch brings the
# existing task back to the front; it does not restart the game, so the run continues where it was.
keep_awake() {
    while true; do
        "$ADB" -s "$DEVICE" shell input keyevent KEYCODE_WAKEUP > /dev/null 2>&1 || true
        "$ADB" -s "$DEVICE" shell monkey -p "$PKG" 1 > /dev/null 2>&1 || true
        sleep 20
    done
}
keep_awake &
WAKE_PID=$!
trap 'kill $TAIL_PID $WAKE_PID 2>/dev/null || true' EXIT

for _ in $(seq 1 1200); do
    grep -q "PERF done" "$LOG" 2>/dev/null && break
    sleep 1
done
sleep 2
kill $TAIL_PID $WAKE_PID 2>/dev/null || true

REPORT="$OUT_DIR/${DATE}_phone.md"
if "$ADB" -s "$DEVICE" shell "run-as $PKG cat files/perf_report.md" > "$REPORT" 2>/dev/null \
   && [ -s "$REPORT" ]; then
    "$ADB" -s "$DEVICE" shell "run-as $PKG cat files/perf_report.csv" \
        > "$OUT_DIR/${DATE}_phone.csv" 2>/dev/null || true
    echo; cat "$REPORT"
    echo; echo "perf.sh: $REPORT"
else
    echo "perf.sh: no report came back. The PERF lines that did arrive:"
    grep -E "PERF|voxfield perf" "$LOG" | tail -40 || true
    exit 1
fi
