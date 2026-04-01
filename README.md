# AVM — Android Virtual Machine

A gaming-focused Android emulator for PC, built from scratch on top of QEMU with hardware virtualization (KVM/AEHD/WHPX) and GPU acceleration via gfxstream.

> ⚠️ This project is in early development. Contributions and ideas are welcome.

---

## Goals

- Near-native (90–100%) performance for Android games on PC
- Full hardware-accelerated GPU rendering via Vulkan/OpenGL ES forwarding
- Low-latency keyboard/mouse/gamepad input remapping
- Configurable game profiles and keymapper UI
- Multi-instance support for running multiple game sessions simultaneously

---

## Architecture Overview

```
┌─────────────────────────────────────────────────┐
│                  HOST (Windows/Linux/macOS)      │
│                                                 │
│   ┌────────────────────────────────────────┐   │
│   │           AVM Host Daemon              │   │
│   │  (Input bridge, GPU decoder, IPC)      │   │
│   └──────────────┬─────────────────────────┘   │
│                  │ virtio / gfxstream           │
│   ┌──────────────▼─────────────────────────┐   │
│   │         QEMU Virtual Machine           │   │
│   │   Android x86 (goldfish kernel)        │   │
│   │   GPU: virtio-gpu / gfxstream guest    │   │
│   └────────────────────────────────────────┘   │
│                                                 │
│   Hypervisor: KVM (Linux) / AEHD / WHPX (Win)  │
└─────────────────────────────────────────────────┘
```

### Key Layers

| Layer | Responsibility |
|---|---|
| **Hypervisor** | Near-native CPU execution via hardware virtualization |
| **QEMU + goldfish kernel** | Android VM base — virtual hardware, sensors, networking |
| **gfxstream / virtio-gpu** | Serializes GPU API calls from guest → host |
| **Host GPU renderer** | Decodes GPU commands, renders via host Vulkan/DirectX/Metal |
| **Input bridge** | Maps keyboard/mouse/gamepad to Android touch events |
| **AVM Overlay UI** | Keymapper, FPS display, game profiles, multi-instance manager |

---

## Project Structure

```
AVM/
├── core/               # QEMU integration, VM lifecycle management
│   ├── vm/             # VM create/start/stop/snapshot
│   ├── hypervisor/     # KVM, AEHD, WHPX backend abstractions
│   └── kernel/         # Goldfish kernel configs and patches
├── gpu/                # GPU forwarding pipeline
│   ├── gfxstream/      # Guest-side gfxstream encoder (vendored/patched)
│   ├── host_renderer/  # Host-side Vulkan/OpenGL decoder
│   └── angle/          # ANGLE OpenGL→Vulkan translation layer
├── input/              # Input bridge and event translation
│   ├── bridge/         # Host input capture → ADB input events
│   └── keymapper/      # Keymapping profiles and engine
├── overlay/            # AVM Overlay UI (keymapper editor, FPS, settings)
├── tools/              # CLI tools, build helpers, image downloader
├── docs/               # Architecture docs, setup guides
│   ├── architecture.md
│   ├── setup-linux.md
│   └── setup-windows.md
├── scripts/            # Build and bootstrap scripts
│   ├── bootstrap.sh
│   └── build.sh
└── CMakeLists.txt      # Top-level CMake build
```

---

## Tech Stack

- **Base VM**: [QEMU](https://www.qemu.org/) with Android goldfish kernel
- **GPU**: [gfxstream](https://android.googlesource.com/platform/hardware/google/gfxstream/) (AOSP) + [ANGLE](https://chromium.googlesource.com/angle/angle)
- **Hypervisors**: KVM (Linux), Android Emulator Hypervisor Driver / WHPX (Windows)
- **Host UI**: C++ with Dear ImGui (overlay) or Qt (main launcher)
- **Build system**: CMake + Ninja
- **Language**: C/C++ (core, GPU, input), Python (tooling/scripts)

---

## Roadmap

### Phase 1 — Foundation
- [ ] QEMU-based Android VM boots to home screen
- [ ] Hardware virtualization (KVM / AEHD) enabled
- [ ] x86 Android system image integration
- [ ] Basic ADB connectivity

### Phase 2 — GPU Acceleration
- [ ] gfxstream guest driver integration
- [ ] Host-side Vulkan renderer
- [ ] ANGLE OpenGL ES → Vulkan translation
- [ ] 60fps stable rendering in a test game

### Phase 3 — Gaming UX
- [ ] Keyboard/mouse → touch input bridge
- [ ] Keymapper UI with game profile support
- [ ] FPS counter and performance overlay
- [ ] Multi-instance support

### Phase 4 — Polish
- [ ] FPS unlock (bypass Android vsync cap)
- [ ] Game-specific performance profiles
- [ ] One-click game download/install helper
- [ ] Windows installer / Linux AppImage packaging

---

## Getting Started (WIP)

> Full setup guides are in `docs/setup-linux.md` and `docs/setup-windows.md`.

### Prerequisites

- Linux (recommended): KVM-capable kernel, `libvirt`, `qemu-system-x86_64`
- Windows: Hyper-V or AEHD enabled, Visual Studio 2022
- CMake 3.20+, Ninja, Python 3.10+

```bash
git clone https://github.com/wisnuub/AVM.git
cd AVM
./scripts/bootstrap.sh   # fetch submodules, install deps
./scripts/build.sh       # configure + build
```

---

## References & Prior Art

- [Android Emulator (AOSP)](https://android.googlesource.com/platform/external/qemu/)
- [gfxstream](https://android.googlesource.com/platform/hardware/google/gfxstream/)
- [ANGLE](https://chromium.googlesource.com/angle/angle)
- [QEMU](https://www.qemu.org/)
- [BlueStacks HyperG Engine](https://www.bluestacks.com/blog/bluestacks-exclusives/what-is-bluestacks-en.html)
- Trinity: Near-Native Android Emulation (OSDI 2022)

---

## License

MIT License — see [LICENSE](LICENSE) for details.
