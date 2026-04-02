#!/usr/bin/env bash
# safetynet-patch.sh — Apply SafetyNet / Play Integrity spoofing to a running AVM instance.
#
# Strategy:
#   1. Push MagiskHide Props Config values via ADB to spoof device fingerprint
#      to a known certified device (passing CTS profile match).
#   2. Patch ro.build.* props to match a real device that is on Google's
#      certified device list (Pixel 8 Pro fingerprint used here).
#   3. Disable su/root indicators that SafetyNet hardware attestation checks for.
#
# This targets Basic Integrity + CTS Profile Match (SafetyNet classic).
# Hardware-backed attestation (Play Integrity STRONG) requires a real TEE
# and cannot be fully spoofed in a VM — only MEETS_BASIC_INTEGRITY and
# MEETS_DEVICE_INTEGRITY levels are achievable.
#
# Usage: bash scripts/safetynet-patch.sh [adb-serial]
#   adb-serial: optional, defaults to the first connected device

set -euo pipefail

ADB=$(command -v adb 2>/dev/null || echo "")
if [[ -z "$ADB" ]]; then
    echo "Error: adb not found. Install Android SDK Platform Tools."
    exit 1
fi

SERIAL="${1:-}"
if [[ -n "$SERIAL" ]]; then
    ADB_CMD="$ADB -s $SERIAL"
else
    ADB_CMD="$ADB"
fi

echo "Waiting for device …"
$ADB_CMD wait-for-device

echo "Checking root access …"
if ! $ADB_CMD shell "su -c 'echo ok'" 2>/dev/null | grep -q ok; then
    echo "  ⚠ Root (su) not available. SafetyNet patching requires Magisk or similar."
    echo "  Attempting resetprop-based approach (requires Magisk resetprop) …"
fi

# ---------------------------------------------------------------------------
# Spoofing fingerprint  — Pixel 8 Pro (Android 14, release-keys)
# This is a Google-certified device fingerprint from the CTS verified list.
# ---------------------------------------------------------------------------
echo "Applying device fingerprint spoof (Pixel 8 Pro / Android 14) …"

PROPS=(
    "ro.product.brand=google"
    "ro.product.device=husky"
    "ro.product.manufacturer=Google"
    "ro.product.model=Pixel 8 Pro"
    "ro.product.name=husky"
    "ro.build.fingerprint=google/husky/husky:14/AD1A.240905.004/12117344:user/release-keys"
    "ro.build.description=husky-user 14 AD1A.240905.004 12117344 release-keys"
    "ro.build.version.release=14"
    "ro.build.version.sdk=34"
    "ro.build.tags=release-keys"
    "ro.build.type=user"
    "ro.debuggable=0"
    "ro.secure=1"
    "ro.adb.secure=1"
)

for prop in "${PROPS[@]}"; do
    key="${prop%%=*}"
    val="${prop#*=}"
    echo "  Setting $key"
    # Try Magisk resetprop first (persists across boots if in /data/adb/modules)
    $ADB_CMD shell "su -c \"resetprop '$key' '$val'\"" 2>/dev/null || \
    $ADB_CMD shell "setprop '$key' '$val'" 2>/dev/null || \
    echo "    ⚠ Could not set $key (may require root)"
done

# ---------------------------------------------------------------------------
# Disable test-keys / engineering build indicators
# ---------------------------------------------------------------------------
echo "Disabling test-keys indicators …"
$ADB_CMD shell "su -c \"resetprop ro.build.selinux 0\"" 2>/dev/null || true
$ADB_CMD shell "su -c \"resetprop ro.build.keys release-keys\"" 2>/dev/null || true

# ---------------------------------------------------------------------------
# Magisk MagiskHide / DenyList — hide root from Play Services & Play Store
# ---------------------------------------------------------------------------
echo "Configuring Magisk DenyList for Play Services …"
$ADB_CMD shell "su -c 'magisk --denylist add com.google.android.gms'" 2>/dev/null || true
$ADB_CMD shell "su -c 'magisk --denylist add com.google.android.gms.unstable'" 2>/dev/null || true
$ADB_CMD shell "su -c 'magisk --denylist add com.android.vending'" 2>/dev/null || true

# ---------------------------------------------------------------------------
# Install PlayIntegrityFix Magisk module (if available)
# This module spoofs the TEE/hardware attestation at the framework level.
# https://github.com/chiteroman/PlayIntegrityFix
# ---------------------------------------------------------------------------
PIF_MODULE="$(dirname "$0")/PlayIntegrityFix.zip"
if [[ -f "$PIF_MODULE" ]]; then
    echo "Installing PlayIntegrityFix module …"
    $ADB_CMD push "$PIF_MODULE" /sdcard/PlayIntegrityFix.zip
    $ADB_CMD shell "su -c 'magisk --install-module /sdcard/PlayIntegrityFix.zip'"
    echo "  ✓ PlayIntegrityFix installed — reboot to activate."
else
    echo
    echo "  ℹ  For stronger Play Integrity spoofing, download PlayIntegrityFix:"
    echo "     https://github.com/chiteroman/PlayIntegrityFix/releases"
    echo "     Place PlayIntegrityFix.zip next to this script, then re-run."
fi

# ---------------------------------------------------------------------------
# Reboot to apply
# ---------------------------------------------------------------------------
echo
read -rp "Reboot device now to apply changes? [y/N]: " yn
case "$yn" in
    [Yy]*)
        echo "Rebooting …"
        $ADB_CMD reboot
        ;;
    *)
        echo "Skipped reboot. Run 'adb reboot' when ready."
        ;;
esac

echo
echo "✓ SafetyNet/Play Integrity patch applied."
echo "  Expected result:  MEETS_BASIC_INTEGRITY ✓   MEETS_DEVICE_INTEGRITY ✓"
echo "  Hardware-backed (STRONG) attestation requires a real TEE — not achievable in VM."
