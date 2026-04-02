# AVM — Android Virtual Machine

A gaming-focused Android emulator for PC, built from scratch on top of QEMU with hardware virtualization (KVM / HVF / AEHD / WHPX) and GPU acceleration via gfxstream + Vulkan.

> ⚠️ This project is in active early development. Contributions and ideas are welcome.

---

## Goals

- Near-native (90–100%) performance for Android games on PC
- Full hardware-accelerated GPU rendering via Vulkan/OpenGL ES forwarding (gfxstream)
- Low-latency keyboard, mouse, and gamepad input remapping
- Per-game profiles with keymapper, FPS cap, and resource overrides
- Android version selector (Android 10–15, API 29–35)
- Multi-instance support for running multiple game sessions simultaneously

---

## Architecture Overview

```
┌───────────────────────────────────────────────────┐
│        HOST (Windows / Linux / macOS)         │
│                                               │
│  ┌─────────────────────────────────────┐  │
│  │        AVM Host Process                │  │
│  │  CLI / Profile loader / ImageManager  │  │
│  │  Input bridge (SDL2 → ADB/virtio)    │  │
│  │  FPS limiter + Overlay UI             │  │
│  └─────────────┬──────────────────────┘  │
│                 │ virtio / gfxstream          │
│  ┌─────────────┴──────────────────────┐  │
│  │         QEMU Virtual Machine           │  │
│  │   Android (goldfish kernel)            │  │
│  │   GPU: virtio-gpu / gfxstream guest    │  │
│  └─────────────────────────────────────┘  │
│                                               │
│  Hypervisor: KVM • HVF • AEHD • WHPX        │
└───────────────────────────────────────────────┐
```

### Key Layers

| Layer | Responsibility |
|---|---|
| **Hypervisor** | Near-native CPU execution (KVM / HVF / AEHD / WHPX) |
| **QEMU + goldfish kernel** | Android VM base — virtual hardware, sensors, networking |
| **gfxstream / virtio-gpu** | Serializes GPU API calls from guest → host |
| **Host GPU renderer** | Decodes GPU commands, renders via host Vulkan / Metal / DX12 |
| **Input bridge** | Maps keyboard / mouse / gamepad → Android touch events |
| **Profile system** | Per-game resource + keymapper + FPS config |
| **AVM Overlay UI** | Keymapper editor, FPS display, multi-instance manager |

---

## Project Structure

```
AVM/
├── core/                    # VM lifecycle, hypervisor, version selector
│   ├── vm_manager.cpp         # QEMU launch, QMP, ADB wait
│   ├── android_version.cpp    # Version metadata table + parser
│   ├── image_manager.cpp      # Image validation + arch compat
│   ├── profile.cpp            # Per-game profile load/save/apply
│   ├── fps_limiter.cpp        # Precise sleep+spin FPS limiter
│   └── hypervisor/            # Platform hypervisor helpers
├── gpu/                     # GPU forwarding pipeline
│   ├── gfxstream/             # Guest-side gfxstream encoder
│   ├── host_renderer/         # Host Vulkan / OpenGL decoder
│   └── angle/                 # ANGLE OpenGL ES → Vulkan
├── input/                   # Input bridge and event translation
│   ├── input_bridge_sdl.cpp   # SDL2 → touch/key events
│   ├── input_bridge_adb.cpp   # ADB sendevent transport
│   ├── adb_injector.cpp       # ADB command runner
│   ├── gamepad_bridge_sdl.cpp # SDL2 gamepad → Android input
│   └── virtio_input.cpp       # virtio-input socket (Linux/macOS)
├── overlay/                 # AVM Overlay UI
│   ├── overlay.cpp
│   ├── fps_overlay.cpp
│   └── keymapper_editor.cpp
├── include/avm/             # Public headers
├── src/
│   └── main.cpp               # CLI entry point
├── docs/                    # Phase docs, setup guides
├── scripts/
│   ├── bootstrap-macos.sh     # One-shot macOS setup + codesign
│   └── bootstrap-windows.ps1  # One-shot Windows setup
└── CMakeLists.txt
```

---

## Quick Start

### macOS (Apple Silicon or Intel)

```bash
git clone https://github.com/wisnuub/AVM.git && cd AVM
chmod +x scripts/bootstrap-macos.sh
./scripts/bootstrap-macos.sh
# Binary is at ./build/avm, already codesigned with Hypervisor entitlement
```

> ⚠️ **Apple Silicon only**: You must use an **ARM64** Android image.
> x86\_64 images will not run under `Hypervisor.framework` on M1/M2/M3.

### Windows

```powershell
# Run as Administrator
git clone https://github.com/wisnuub/AVM.git; cd AVM
.\scripts\bootstrap-windows.ps1
# Binary: .\build\RelWithDebInfo\avm.exe
```

### Linux

```bash
sudo apt install cmake ninja-build libsdl2-dev libsdl2-ttf-dev qemu-system-x86
git clone https://github.com/wisnuub/AVM.git && cd AVM
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

---

## Usage

```bash
# Basic boot
avm --android 14 --image android14-arm64.img

# Specify resources explicitly
avm --android 13 --image android13.img --memory 6144 --cores 6

# Unlock FPS (bypass Android vsync cap)
avm --android 14 --image android14.img --fps-unlock

# Load a per-game profile
avm --profile genshin --image android14.img

# See all supported Android versions
avm --list-versions

# See saved profiles
avm --list-profiles
```

### Android Version Selector

All of the following select Android 13:
```bash
avm --android 13
avm --android android13
avm --android API33
avm --android tiramisu
```

| Version    | Code Name         | API | Min Kernel | Min RAM |
|------------|-------------------|-----|------------|---------|
| Android 10 | Q                 | 29  | 4.14       | 2 GB    |
| Android 11 | R                 | 30  | 4.19       | 3 GB    |
| Android 12 | S                 | 31  | 5.4        | 4 GB    |
| Android 12L| Sv2               | 32  | 5.10       | 4 GB    |
| Android 13 | Tiramisu          | 33  | 5.15       | 4 GB    |
| Android 14 | Upside Down Cake  | 34  | 6.1        | 6 GB    |
| Android 15 | Vanilla Ice Cream | 35  | 6.6        | 6 GB    |

---

## Per-Game Profiles

Profiles are JSON files in `~/.config/avm/profiles/` (Linux/macOS) or `%APPDATA%\AVM\profiles\` (Windows).

```json
{
  "name": "genshin",
  "package_name": "com.miHoYo.GenshinImpact",
  "display_name": "Genshin Impact",
  "keymapper": "~/.config/avm/keymaps/genshin.json",
  "android_version": 34,
  "memory_mb": 8192,
  "vcpu_cores": 6,
  "target_fps": 60,
  "fps_unlock": false
}
```

CLI flags always override profile values.

---

## Tech Stack

| Component | Technology |
|---|---|
| Base VM | [QEMU](https://www.qemu.org/) + Android goldfish kernel |
| GPU | [gfxstream](https://android.googlesource.com/platform/hardware/google/gfxstream/) + [ANGLE](https://chromium.googlesource.com/angle/angle) |
| Hypervisors | KVM (Linux) • HVF (macOS) • AEHD / WHPX (Windows) |
| Input | SDL2, ADB `sendevent`, virtio-input |
| Overlay UI | Dear ImGui |
| Build | CMake 3.20+ + Ninja |
| Language | C++17 (core, GPU, input) |

---

## Roadmap

### ✅ Phase 1 — Foundation
- [x] QEMU-based Android VM boots to home screen
- [x] Hardware virtualization (KVM / HVF / AEHD / WHPX)
- [x] Android system image integration + validation
- [x] ADB connectivity

### ✅ Phase 2 — GPU Acceleration
- [x] virtio-gpu device in QEMU args
- [x] Host-side Vulkan / OpenGL renderer selection
- [x] ANGLE OpenGL ES → Vulkan translation layer
- [ ] gfxstream guest driver (in progress)
- [ ] 60 fps stable rendering in test game

### ✅ Phase 3 — Input Bridge
- [x] SDL2 keyboard/mouse → touch event bridge
- [x] Keymapper UI with game profile support
- [x] FPS counter overlay
- [x] Multi-instance foundation

### ✅ Phase 4 — Gamepad & virtio-input
- [x] SDL2 gamepad → Android input
- [x] ADB sendevent injector
- [x] virtio-input socket transport (Linux/macOS)
- [x] Platform build guards (Windows/macOS/Linux)

### ✅ Phase 5 — Version Selector & Profiles
- [x] Android version selector (`--android 10`–`15`, API levels, code names)
- [x] Image validation + arch compatibility check
- [x] RAM/vCPU auto-tuning from version metadata
- [x] Per-game JSON profile system
- [x] Precise FPS limiter (sleep+spin hybrid)
- [x] `--fps-unlock` flag
- [x] macOS bootstrap script with auto codesign
- [x] Windows bootstrap PowerShell script

### 🟡 Phase 6 — Packaging & Distribution *(next)*
- [ ] `avm-image-pull` CLI tool to fetch AOSP images automatically
- [ ] Linux AppImage / Flatpak packaging
- [ ] Windows NSIS/WiX installer
- [ ] macOS `.app` bundle with Info.plist + entitlements
- [ ] Auto-updater channel

---

## Prerequisites by Platform

| Platform | Requirement |
|---|---|
| **Linux** | KVM-capable kernel, `libvirt`, `qemu-system-x86_64` or `qemu-system-aarch64` |
| **macOS** | macOS 11+, Xcode CLI tools, `brew install qemu cmake ninja sdl2` |
| **Windows** | Windows 10 1803+, WHPX or AEHD enabled, Visual Studio 2022, vcpkg |
| **All** | CMake 3.20+, Ninja, C++17 compiler |

---

## References

- [Android Emulator (AOSP)](https://android.googlesource.com/platform/external/qemu/)
- [gfxstream](https://android.googlesource.com/platform/hardware/google/gfxstream/)
- [ANGLE](https://chromium.googlesource.com/angle/angle)
- [QEMU](https://www.qemu.org/)
- [Apple Hypervisor.framework](https://developer.apple.com/documentation/hypervisor)
- [Android Emulator Hypervisor Driver (AEHD)](https://github.com/google/android-emulator-hypervisor-driver)
- Trinity: Near-Native Android Emulation (OSDI 2022)

---

## License

MIT License — see [LICENSE](LICENSE) for details.
