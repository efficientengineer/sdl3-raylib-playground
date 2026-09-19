#!/usr/bin/env bash
# Build ArtTray.app if the source is newer than the binary, then open it.
# Any arguments are passed to the app (e.g. ./run.sh --repo /path/to/repo).
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$DIR/ArtTray.app"
BIN="$APP/Contents/MacOS/ArtTray"

if [ ! -x "$BIN" ] || [ "$DIR/ArtTray.swift" -nt "$BIN" ]; then
    "$DIR/build.sh"
fi

if [ "$#" -gt 0 ]; then
    open -a "$APP" --args "$@"
else
    open "$APP"
fi
