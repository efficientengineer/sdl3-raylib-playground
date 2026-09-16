#!/bin/bash
# Watch for new bugs on the device and print them
set -e

ADB="$HOME/Library/Android/sdk/platform-tools/adb"
DEVICE="192.168.1.217:5555"
PKG="com.playground.sdlraylib"

LAST_COUNT=0

echo "=== Watching for bug reports on device ==="
echo "Press Ctrl+C to stop"

while true; do
    # Read bug count from binary file
    DATA=$($ADB -s "$DEVICE" shell "run-as $PKG cat files/com.playground/questglory/bugs.dat 2>/dev/null" | xxd -p 2>/dev/null || echo "")
    if [ -n "$DATA" ]; then
        # First 4 bytes after version (4 bytes) = count (little-endian int32)
        COUNT_HEX=$(echo "$DATA" | head -c 16 | tail -c 8)
        if [ -n "$COUNT_HEX" ]; then
            # Convert little-endian hex to int
            B0=$(echo "$COUNT_HEX" | cut -c1-2)
            B1=$(echo "$COUNT_HEX" | cut -c3-4)
            B2=$(echo "$COUNT_HEX" | cut -c5-6)
            B3=$(echo "$COUNT_HEX" | cut -c7-8)
            COUNT=$((16#$B3$B2$B1$B0))

            if [ "$COUNT" -gt "$LAST_COUNT" ] 2>/dev/null; then
                echo ""
                echo "=== NEW BUG(S) DETECTED: $COUNT total (was $LAST_COUNT) ==="
                $ADB -s "$DEVICE" shell "run-as $PKG cat files/com.playground/questglory/bugs.dat" | xxd | head -80
                echo "==="
                LAST_COUNT=$COUNT
            fi
        fi
    fi
    sleep 5
done
