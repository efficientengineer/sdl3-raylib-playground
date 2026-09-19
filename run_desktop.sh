#!/bin/bash
# Build (incrementally) and run the game on the Mac. Same code as the phone: keyboard = arrows/WASD to
# walk, Space/Enter/Z to interact, Shift to run; click to advance dialogue. Art and maps are read
# straight from story/, so a re-cut sheet shows up on the next launch with no push step.
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT" || exit 1
export PATH="/opt/homebrew/bin:/usr/local/bin:$PATH"
LOG="$ROOT/build_desktop/run_desktop.log"
mkdir -p "$ROOT/build_desktop"
{
  ./story_prompt.py portraits >/dev/null 2>&1
  ./story_prompt.py export   >/dev/null 2>&1
  cmake -B build_desktop -S . -DCMAKE_BUILD_TYPE=Debug >/dev/null \
    && cmake --build build_desktop --target quest_glory game_logic -j"$(sysctl -n hw.ncpu)" | tail -2
} >"$LOG" 2>&1
if [ ! -x build_desktop/quest_glory ]; then
  osascript -e 'display alert "The game has not been built yet" message "See build_desktop/run_desktop.log"' >/dev/null 2>&1
  exit 1
fi
# A failed build (an agent may be mid-edit) still launches the last good binary.
exec build_desktop/quest_glory "$@" >>"$LOG" 2>&1
