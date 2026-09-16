#!/bin/bash
# Send a dev message to the game's message log
# Usage: ./dev_msg.sh "Your message here"
set -e

ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"
PREF_DIR="files"

MSG="$1"
if [ -z "$MSG" ]; then
    echo "Usage: $0 \"message text\""
    exit 1
fi

# Read current log, append message, write back atomically
EXISTING=$($ADB -s "$DEVICE" shell "run-as $PKG cat $PREF_DIR/dev_log.txt 2>/dev/null" || true)
TMPFILE=$(mktemp)
if [ -n "$EXISTING" ]; then
    printf '%s\n' "$EXISTING" > "$TMPFILE"
fi
TS=$(date +%H:%M:%S)
printf '[DEV %s] %s\n' "$TS" "$MSG" >> "$TMPFILE"
$ADB -s "$DEVICE" push "$TMPFILE" /data/local/tmp/dev_log.txt > /dev/null 2>&1
$ADB -s "$DEVICE" shell "run-as $PKG cp /data/local/tmp/dev_log.txt $PREF_DIR/dev_log.txt"
rm "$TMPFILE"

echo "Sent: $MSG"
