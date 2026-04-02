#!/usr/bin/env bash
# build-app-bundle.sh — Package AVM into a macOS .app bundle.
#
# Run from the repo root after building:
#   cmake --build build -j$(sysctl -n hw.logicalcpu)
#   bash packaging/macos/build-app-bundle.sh
#
# Output: dist/AVM.app  (ready to drag to /Applications)

set -euo pipefail

BINARY="build/avm"
if [[ ! -f "$BINARY" ]]; then
    echo "Error: $BINARY not found. Build the project first."
    exit 1
fi

APP_NAME="AVM"
BUNDLE_ID="com.wisnuub.avm"
VERSION="0.6.0"

DIST_DIR="dist"
APP_DIR="$DIST_DIR/${APP_NAME}.app"
CONTENTS="$APP_DIR/Contents"
MACOS="$CONTENTS/MacOS"
RESOURCES="$CONTENTS/Resources"

echo "Building ${APP_NAME}.app …"
rm -rf "$APP_DIR"
mkdir -p "$MACOS" "$RESOURCES"

# Copy binary
cp "$BINARY" "$MACOS/avm"
chmod +x "$MACOS/avm"

# Launcher wrapper — sets up env and calls the real binary
cat > "$MACOS/AVM" << 'LAUNCHER'
#!/usr/bin/env bash
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Ensure MoltenVK is discoverable
export DYLD_LIBRARY_PATH="$DIR/../Frameworks:$DYLD_LIBRARY_PATH"
# Forward all args to the real binary
exec "$DIR/avm" "$@"
LAUNCHER
chmod +x "$MACOS/AVM"

# Info.plist
cat > "$CONTENTS/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>AVM</string>
    <key>CFBundleIdentifier</key>
    <string>${BUNDLE_ID}</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key>
    <string>AVM — Android Virtual Machine</string>
    <key>CFBundleVersion</key>
    <string>${VERSION}</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>LSMinimumSystemVersion</key>
    <string>11.0</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSHumanReadableCopyright</key>
    <string>Copyright © 2024 Wisnu A. Kurniawan</string>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
    <!-- Required for Hypervisor.framework -->
    <key>com.apple.security.hypervisor</key>
    <true/>
    <key>com.apple.security.virtualization</key>
    <true/>
</dict>
</plist>
PLIST

# Entitlements
cp "avm.entitlements" "$CONTENTS/avm.entitlements"

# Generate simple SVG icon → icns (requires Inkscape or rsvg-convert + iconutil)
if command -v rsvg-convert &>/dev/null && command -v iconutil &>/dev/null; then
    echo "  Generating app icon …"
    ICONSET_DIR=$(mktemp -d)
    mkdir -p "$ICONSET_DIR/AppIcon.iconset"
    for size in 16 32 64 128 256 512; do
        rsvg-convert -w $size -h $size packaging/macos/AppIcon.svg \
            -o "$ICONSET_DIR/AppIcon.iconset/icon_${size}x${size}.png" 2>/dev/null || true
        rsvg-convert -w $((size*2)) -h $((size*2)) packaging/macos/AppIcon.svg \
            -o "$ICONSET_DIR/AppIcon.iconset/icon_${size}x${size}@2x.png" 2>/dev/null || true
    done
    iconutil -c icns "$ICONSET_DIR/AppIcon.iconset" -o "$RESOURCES/AppIcon.icns" 2>/dev/null || true
    rm -rf "$ICONSET_DIR"
    if [[ -f "$RESOURCES/AppIcon.icns" ]]; then
        # Reference icon in plist
        /usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon" "$CONTENTS/Info.plist" 2>/dev/null || true
        echo "  ✓ App icon generated."
    fi
fi

# Code-sign with Hypervisor entitlement
echo "  Code-signing with Hypervisor entitlement …"
codesign \
    --entitlements "$CONTENTS/avm.entitlements" \
    --force \
    --deep \
    -s - \
    "$APP_DIR"

echo
echo "✓ Built: $APP_DIR"
echo
echo "To install:"
echo "  cp -r $APP_DIR /Applications/"
echo "  open /Applications/AVM.app --args --android 14 --image ~/AVM/images/android14-arm64-gapps/system.img"
echo
echo "Or run directly:"
echo "  open $APP_DIR --args --android 14 --image <path>"
