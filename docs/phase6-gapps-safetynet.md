# Phase 6 — GApps, Play Integrity & Distribution

## Overview

Phase 6 adds three major capabilities:

1. **`avm-image-pull`** — download & register Android system images (vanilla or GApps)
2. **GApps / Google Play Store support** — inject OpenGApps (pico) into system images
3. **SafetyNet / Play Integrity spoofing** — pass CTS + Basic/Device integrity checks
4. **macOS `.app` bundle** — proper signed, double-clickable application

---

## GApps Quick Start

```bash
# Pull Android 14 + GApps (default)
python3 tools/avm-image-pull.py --android 14

# Check what was downloaded
python3 tools/avm-image-pull.py --check

# Boot with GApps
avm --android 14 --image ~/.config/avm/images/android14-arm64-gapps/system.img --gapps
```

---

## SafetyNet / Play Integrity — What's Achievable in a VM

| Level | Full name | Achievable? | Method |
|---|---|---|---|
| ✅ | MEETS_BASIC_INTEGRITY | Yes | Fingerprint spoof + release-keys |
| ✅ | MEETS_DEVICE_INTEGRITY | Yes | Fingerprint spoof + Magisk DenyList |
| ❌ | MEETS_STRONG_INTEGRITY | No | Requires real hardware TEE/Keystore |

**STRONG integrity** relies on hardware-backed key attestation inside a real Trusted Execution Environment (TEE) — a physical chip on the device SoC. A VM cannot emulate this. Most games only require BASIC + DEVICE integrity, which AVM can pass.

### Automatic (boot-time spoofing)

When you pass `--gapps`, AVM injects the Pixel 8 Pro fingerprint via QEMU `-prop` at boot. No ADB needed:

```bash
avm --android 14 --image <img> --gapps
avm --android 14 --image <img> --gapps --spoof-profile pixel7pro
```

### Manual (post-boot, ADB)

For stronger spoofing with PlayIntegrityFix Magisk module:

```bash
# 1. Download PlayIntegrityFix.zip from:
#    https://github.com/chiteroman/PlayIntegrityFix/releases
cp ~/Downloads/PlayIntegrityFix.zip scripts/

# 2. Run patch script (requires Magisk in the image)
bash scripts/safetynet-patch.sh
```

---

## GApps Injection Architecture

```
avm-image-pull
    │
    ├─ Download base system.img (AOSP Google APIs ARM64)
    ├─ Download OpenGApps pico ZIP (arm64/14.0)
    └─ Call scripts/gapps-inject.sh
           │
           ├─ Resize system.img (+512 MB headroom)
           ├─ Mount image (fuse-ext2 on macOS, loop device on Linux)
           ├─ Copy GApps APKs → /system/priv-app:
           │     PrebuiltGmsCore    ← GMS Core (Google Play Services)
           │     Phonesky           ← Google Play Store
           │     GoogleServicesFramework
           │     GoogleLoginService
           │     GooglePartnerSetup
           ├─ Patch build.prop → ro.build.tags=release-keys
           └─ Unmount
```

---

## Fingerprint Spoof Profiles

Built-in profiles (passed via `--spoof-profile`):

| Profile | Device | Android | Fingerprint |
|---|---|---|---|
| `pixel8pro` | Pixel 8 Pro | 14 | `google/husky/husky:14/AD1A.240905.004/...` |
| `pixel7pro` | Pixel 7 Pro | 14 | `google/cheetah/cheetah:14/AP1A.240905.005/...` |
| `pixelfold` | Pixel Fold | 14 | `google/felix/felix:14/AD1A.240905.004/...` |

Custom fingerprint (JSON file):

```json
{
  "brand": "google",
  "device": "husky",
  "manufacturer": "Google",
  "model": "Pixel 8 Pro",
  "name": "husky",
  "fingerprint": "google/husky/husky:14/AD1A.240905.004/12117344:user/release-keys",
  "description": "husky-user 14 AD1A.240905.004 12117344 release-keys",
  "version_release": "14",
  "version_sdk": 34
}
```

```bash
avm --android 14 --image <img> --gapps --fingerprint ~/my-device.json
```

---

## macOS App Bundle

```bash
# Build the project first
cmake --build build -j$(sysctl -n hw.logicalcpu)

# Package
bash packaging/macos/build-app-bundle.sh

# Install
cp -r dist/AVM.app /Applications/

# Run from Spotlight or Finder
open /Applications/AVM.app --args --android 14 --image <img>
```

---

## Phase 6 Roadmap Status

- [x] `avm-image-pull` CLI
- [x] OpenGApps injection (`gapps-inject.sh`)
- [x] Boot-time SafetyNet spoof (`core/gapps.cpp` + QEMU `-prop`)
- [x] ADB post-boot spoof (`safetynet-patch.sh` + PlayIntegrityFix support)
- [x] macOS `.app` bundle + codesign (`packaging/macos/`)
- [ ] Windows NSIS installer
- [ ] Linux AppImage
- [ ] Auto-update manifest (hosted image registry)
- [ ] gfxstream guest driver (in progress)
