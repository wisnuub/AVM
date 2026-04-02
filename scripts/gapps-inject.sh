#!/usr/bin/env bash
# gapps-inject.sh — Mount a system.img, inject OpenGApps (pico), unmount.
#
# Usage: sudo bash scripts/gapps-inject.sh <system.img> <gapps.zip> <android_api>
#
# Requirements (macOS): hdiutil (built-in), e2fsprogs (brew), resize2fs
# Requirements (Linux): mount, e2fsck, resize2fs, unzip

set -euo pipefail

IMAGE="${1:-}"
GAPPS_ZIP="${2:-}"
ANDROID_API="${3:-34}"

if [[ -z "$IMAGE" || -z "$GAPPS_ZIP" ]]; then
    echo "Usage: $0 <system.img> <gapps.zip> <android_api>"
    exit 1
fi

if [[ ! -f "$IMAGE" ]]; then
    echo "Error: image not found: $IMAGE"
    exit 1
fi

if [[ ! -f "$GAPPS_ZIP" ]]; then
    echo "Error: GApps zip not found: $GAPPS_ZIP"
    exit 1
fi

PLATFORM=$(uname -s)
MOUNT_DIR=$(mktemp -d /tmp/avm-gapps-XXXXXX)
GAPPS_EXTRACT=$(mktemp -d /tmp/avm-gapps-src-XXXXXX)

cleanup() {
    echo "  Cleaning up …"
    if [[ "$PLATFORM" == "Darwin" ]]; then
        hdiutil detach "$MOUNT_DIR" 2>/dev/null || true
    else
        umount "$MOUNT_DIR" 2>/dev/null || true
    fi
    rm -rf "$GAPPS_EXTRACT"
    rmdir "$MOUNT_DIR" 2>/dev/null || true
}
trap cleanup EXIT

echo "  [1/5] Extracting GApps zip …"
unzip -q "$GAPPS_ZIP" -d "$GAPPS_EXTRACT"

# Determine pico package directory inside the OpenGApps zip
GAPPS_SRC=$(find "$GAPPS_EXTRACT" -type d -name "pico" | head -1)
if [[ -z "$GAPPS_SRC" ]]; then
    # Fallback: look for Core folder (older OpenGApps layout)
    GAPPS_SRC=$(find "$GAPPS_EXTRACT" -type d -name "Core" | head -1)
fi
if [[ -z "$GAPPS_SRC" ]]; then
    echo "  Warning: Could not locate pico/Core in GApps zip — injecting all APKs found."
    GAPPS_SRC="$GAPPS_EXTRACT"
fi

echo "  [2/5] Resizing image to make room for GApps …"
# Add 512 MB headroom
dd if=/dev/zero bs=1m count=512 >> "$IMAGE" 2>/dev/null || dd if=/dev/zero bs=1M count=512 >> "$IMAGE" 2>/dev/null
e2fsck -yf "$IMAGE" || true
resize2fs "$IMAGE" 2>/dev/null || true

echo "  [3/5] Mounting system image …"
if [[ "$PLATFORM" == "Darwin" ]]; then
    # macOS: use hdiutil to attach ext4 image via FUSE-ext2 if available,
    # otherwise fall back to a Linux VM approach.
    if command -v fuse-ext2 &>/dev/null; then
        fuse-ext2 "$IMAGE" "$MOUNT_DIR" -o rw+
    else
        echo "  ⚠ fuse-ext2 not found. Install via: brew install --cask fuse-ext2"
        echo "    Or run this script inside a Linux container/VM."
        exit 1
    fi
else
    mount -t ext4 -o loop,rw "$IMAGE" "$MOUNT_DIR"
fi

echo "  [4/5] Copying GApps APKs into /system/priv-app …"
PRIV_APP="$MOUNT_DIR/priv-app"
mkdir -p "$PRIV_APP"

# Core GApps components for Play Store functionality
GAPPS_PACKAGES=(
    "PrebuiltGmsCore"     # GMS Core — required for all Google services
    "Phonesky"            # Google Play Store
    "GoogleServicesFramework"  # GSF
    "GoogleLoginService"  # Account login
    "GooglePartnerSetup"  # Setup wizard
)

for pkg in "${GAPPS_PACKAGES[@]}"; do
    APK_PATH=$(find "$GAPPS_EXTRACT" -type f -name "${pkg}.apk" 2>/dev/null | head -1)
    if [[ -n "$APK_PATH" ]]; then
        TARGET_DIR="$PRIV_APP/$pkg"
        mkdir -p "$TARGET_DIR"
        cp "$APK_PATH" "$TARGET_DIR/${pkg}.apk"
        echo "    + $pkg"
    else
        echo "    ✗ $pkg not found in GApps zip (may be named differently)"
    fi
done

# Also copy any additional APKs from the pico set
while IFS= read -r -d '' apk; do
    name=$(basename "$apk" .apk)
    # Skip if already copied
    if [[ ! -d "$PRIV_APP/$name" ]]; then
        mkdir -p "$PRIV_APP/$name"
        cp "$apk" "$PRIV_APP/$name/${name}.apk"
        echo "    + $name (additional)"
    fi
done < <(find "$GAPPS_SRC" -type f -name "*.apk" -print0 2>/dev/null)

# Write build.prop additions for Play Store compatibility
BUILD_PROP="$MOUNT_DIR/build.prop"
if [[ -f "$BUILD_PROP" ]]; then
    echo "  [4b] Patching build.prop for GApps compatibility …"
    # Remove any existing conflicting entries
    sed -i.bak '/^ro.build.tags/d' "$BUILD_PROP" 2>/dev/null || true
    sed -i.bak '/^ro.build.type/d' "$BUILD_PROP" 2>/dev/null || true
    sed -i.bak '/^ro.debuggable/d' "$BUILD_PROP" 2>/dev/null || true
    # Write Play-compatible values
    cat >> "$BUILD_PROP" << 'PROPS'
ro.build.tags=release-keys
ro.build.type=user
ro.debuggable=0
PROPS
fi

echo "  [5/5] Unmounting …"
if [[ "$PLATFORM" == "Darwin" ]]; then
    umount "$MOUNT_DIR" 2>/dev/null || diskutil unmount "$MOUNT_DIR" 2>/dev/null || true
else
    umount "$MOUNT_DIR"
fi

echo
echo "  ✓ GApps injection complete."
echo "  ✓ Image: $IMAGE"
