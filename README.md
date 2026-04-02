# AVM — Android Virtual Machine

> Near-native Android on PC via hardware virtualization (HVF/KVM/WHPX) + GPU forwarding.

[![License: MIT](https://img.shields.io/badge/License-MIT-teal.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux%20%7C%20Windows-lightgrey)](#prerequisites)
[![Phase](https://img.shields.io/badge/phase-6-blue)](#roadmap)

---

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     AVM Host Process                    │
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │  SDL2    │  │  Overlay │  │  Profile / GApps     │  │
│  │  Window  │  │  (HUD)   │  │  Config Engine       │  │
│  └────┬─────┘  └────┬─────┘  └──────────┬───────────┘  │
│       └─────────────┴──────────────────┬┘              │
│                                        ▼                │
│  ┌─────────────────────────────────────────────────┐   │
│  │              QEMU / Virtual Machine             │   │
│  │   CPU: HVF (macOS) / KVM (Linux) / WHPX (Win)  │   │
│  │   GPU: MoltenVK → gfxstream → virtio-gpu        │   │
│  │   RAM: configurable  │  vCPUs: configurable      │   │
│  └─────────────────────────────────────────────────┘   │
│                         │                               │
│  ┌──────────────────────▼──────────────────────────┐   │
│  │              Android Guest (ARM64)              │   │
│  │   GApps (Phonesky/GMS Core) + SafetyNet spoof   │   │
│  │   virtio-input │ virtio-net │ virtio-gpu         │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

---

## Prerequisites

| Platform | Requirements |
|---|---|
| **macOS (Apple Silicon / Intel)** | macOS 11+, Xcode CLI tools, `brew install cmake ninja sdl2 sdl2_ttf qemu molten-vk` |
| **Linux** | KVM-capable kernel, `apt install cmake ninja-build libsdl2-dev qemu-system-aarch64` |
| **Windows** | Windows 10 1903+, Hyper-V or AEHD, Visual Studio 2022, vcpkg |

---

## Quick Start

### macOS (Apple Silicon recommended)

```bash
git clone https://github.com/wisnuub/AVM.git && cd AVM
bash scripts/bootstrap-macos.sh

# Pull Android 14 + GApps (Play Store)
python3 tools/avm-image-pull.py --android 14 --flavor gapps

# Boot
./build/avm --android 14 \
  --image ~/.config/avm/images/android14-arm64-gapps/system.img \
  --gapps
```

### Linux

```bash
git clone https://github.com/wisnuub/AVM.git && cd AVM
bash scripts/bootstrap.sh
python3 tools/avm-image-pull.py --android 14 --flavor gapps
./build/avm --android 14 \
  --image ~/.config/avm/images/android14-arm64-gapps/system.img \
  --gapps
```

### Windows (PowerShell)

```powershell
git clone https://github.com/wisnuub/AVM.git; cd AVM
.\scripts\bootstrap-windows.ps1
python tools\avm-image-pull.py --android 14 --flavor gapps
.\build\avm.exe --android 14 `
  --image "$env:APPDATA\AVM\images\android14-arm64-gapps\system.img" `
  --gapps
```

---

## Usage

```
avm [OPTIONS]

Core:
  --android <ver>       Android version: 12, 13, 14 (default: 14)
  --image <path>        Path to system.img
  --memory <MB>         RAM in MB (default: auto per version)
  --cores <n>           vCPU count (default: 4)
  --gpu <backend>       vulkan | opengl | software (default: vulkan)

GApps & Play Integrity:
  --gapps               Enable GApps mode (Play Store, GMS Core)
  --spoof-profile <id>  Fingerprint profile: pixel8pro | pixel7pro | pixelfold
  --fingerprint <file>  Custom fingerprint JSON file

Performance:
  --fps <n>             Target FPS cap (0 = unlimited, default: 60)
  --fps-unlock          Unlimited FPS + vsync override

Profiles:
  --profile <name>      Load per-game profile (name or path to .json)
  --list-profiles       List saved profiles

Info:
  --list-versions       Show supported Android versions
  --version             Show AVM version
```

### Android Versions

| Version | API | Min RAM | Notes |
|---|---|---|---|
| Android 14 | 34 | 4 GB (6 GB with GApps) | Recommended |
| Android 13 | 33 | 3 GB (4 GB with GApps) | Stable |
| Android 12 | 32 | 2 GB | Legacy |

---

## GApps & Google Play Store

AVM supports Google Play Store via **OpenGApps pico** injection.

```bash
# Download image with GApps pre-injected
python3 tools/avm-image-pull.py --android 14 --flavor gapps

# List downloaded images
python3 tools/avm-image-pull.py --check

# Boot with GApps enabled
avm --android 14 --image <img> --gapps
```

Sign in with your Google account after first boot to access the Play Store.

---

## SafetyNet / Play Integrity

AVM spoofs the device fingerprint at boot time so Play Services sees a real certified device (Pixel 8 Pro by default).

| Check | Status |
|---|---|
| MEETS_BASIC_INTEGRITY | ✅ Passes |
| MEETS_DEVICE_INTEGRITY | ✅ Passes |
| MEETS_STRONG_INTEGRITY | ❌ Not possible (requires real TEE) |

Most games check BASIC + DEVICE only. For apps that require STRONG, download **PlayIntegrityFix**:

```bash
# Download PlayIntegrityFix.zip → scripts/
bash scripts/safetynet-patch.sh
```

See [`docs/phase6-gapps-safetynet.md`](docs/phase6-gapps-safetynet.md) for full details.

---

## Per-Game Profiles

Profiles live in `~/.config/avm/profiles/` (Linux/macOS) or `%APPDATA%\AVM\profiles\` (Windows).

```json
{
  "name": "genshin",
  "android": 14,
  "memory_mb": 8192,
  "cores": 6,
  "target_fps": 60,
  "gpu_backend": "vulkan",
  "gapps": true,
  "spoof_profile": "pixel8pro",
  "description": "Genshin Impact — high-res, 60fps"
}
```

```bash
avm --profile genshin --image ~/.config/avm/images/android14-arm64-gapps/system.img
```

---

## macOS App Bundle

```bash
cmake --build build -j$(sysctl -n hw.logicalcpu)
bash packaging/macos/build-app-bundle.sh
cp -r dist/AVM.app /Applications/
```

---

## Building from Source

```bash
# macOS (one command)
bash scripts/bootstrap-macos.sh

# Manual
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DAVM_ENABLE_HVF=ON \
    -DAVM_ENABLE_MOLTENVK=ON
cmake --build build -j$(sysctl -n hw.logicalcpu)
codesign --entitlements avm.entitlements --force -s - ./build/avm
```

---

## Roadmap

- [x] Phase 1 — QEMU base + HVF/KVM/WHPX virtualization
- [x] Phase 2 — SDL2 window + virtio-input (keyboard, mouse, gamepad)
- [x] Phase 3 — GPU forwarding (MoltenVK/gfxstream host side)
- [x] Phase 4 — Overlay HUD, keymapper, ADB bridge
- [x] Phase 5 — Per-game profiles, FPS limiter, Android version selector
- [x] Phase 6 — GApps (Play Store), SafetyNet spoof, image-pull CLI, macOS .app bundle
- [ ] Phase 7 — Windows/Linux installers, auto-update, gfxstream guest driver

---

## License

MIT — see [LICENSE](LICENSE).
