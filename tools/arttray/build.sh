#!/usr/bin/env bash
# Build tools/arttray/ArtTray.app with swiftc. No Xcode project, no signing.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
APP="$DIR/ArtTray.app"
BIN="$APP/Contents/MacOS/ArtTray"

if ! command -v swiftc >/dev/null 2>&1; then
    echo "swiftc not found. Install the Xcode command line tools: xcode-select --install" >&2
    exit 1
fi

ARCH="$(uname -m)"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key>            <string>ArtTray</string>
    <key>CFBundleDisplayName</key>     <string>Art Tray</string>
    <key>CFBundleExecutable</key>      <string>ArtTray</string>
    <key>CFBundleIdentifier</key>      <string>com.playground.arttray</string>
    <key>CFBundleInfoDictionaryVersion</key> <string>6.0</string>
    <key>CFBundlePackageType</key>     <string>APPL</string>
    <key>CFBundleShortVersionString</key> <string>1.0</string>
    <key>CFBundleVersion</key>         <string>1</string>
    <key>LSMinimumSystemVersion</key>  <string>12.0</string>
    <key>NSHighResolutionCapable</key> <true/>
    <key>NSPrincipalClass</key>        <string>NSApplication</string>
</dict>
</plist>
PLIST

echo "compiling ArtTray.swift ($ARCH)…"
swiftc -O \
    -parse-as-library \
    -target "$ARCH-apple-macosx12.0" \
    -framework AppKit \
    -o "$BIN" \
    "$DIR/ArtTray.swift"

# A locally built binary is not quarantined, so it opens without a Gatekeeper prompt.
touch "$APP"
echo "built $APP"
echo "run it with: $DIR/run.sh   (or: open '$APP')"
