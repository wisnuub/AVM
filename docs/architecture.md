# AVM Architecture

## Overview

AVM is a gaming-focused Android emulator. The goal is near-native (90–100%) performance on PC by combining:

1. **Hardware virtualization** — CPU instructions run directly on the host hardware via a hypervisor, eliminating instruction-by-instruction software interpretation.
2. **x86 Android system image** — avoids ARM→x86 architecture translation overhead entirely.
3. **GPU forwarding (gfxstream)** — OpenGL ES and Vulkan draw calls are serialized in the guest, forwarded to the host, and decoded + dispatched to the real GPU.

---

## Component Diagram

```
┌──────────────────────────────────────────────────────────────┐
│                    HOST PROCESS (avm)                        │
│                                                              │
│  ┌───────────────┐  ┌──────────────────┐  ┌──────────────┐  │
│  │  VmManager    │  │   GpuRenderer    │  │ InputBridge  │  │
│  │  (QEMU mgmt)  │  │ (Vulkan/OpenGL)  │  │ (ADB/virtio) │  │
│  └──────┬────────┘  └────────┬─────────┘  └──────┬───────┘  │
│         │                    │ gfxstream          │ events   │
│  ┌──────▼────────────────────▼────────────────────▼───────┐  │
│  │               QEMU Virtual Machine                     │  │
│  │                                                        │  │
│  │  ┌─────────────────────────────────────────────────┐  │  │
│  │  │          Android x86 (goldfish kernel)          │  │  │
│  │  │                                                 │  │  │
│  │  │   SurfaceFlinger → virtio-gpu → gfxstream enc  │  │  │
│  │  │   Android Input → virtio-input                 │  │  │
│  │  └─────────────────────────────────────────────────┘  │  │
│  │                                                        │  │
│  │  Hypervisor: KVM / AEHD / WHPX                        │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## CPU Virtualization

When hardware acceleration is enabled, the VM runs an **x86 Android image** on an **x86 host** — meaning the guest CPU instructions are passed through directly to the host CPU by the hypervisor. There is no instruction translation.

- **Linux**: KVM (`/dev/kvm`)
- **Windows**: AEHD (Android Emulator Hypervisor Driver) or WHPX (Windows Hypervisor Platform)
- **macOS**: HVF (Hypervisor.framework) — to be implemented

---

## GPU Forwarding

Android games issue OpenGL ES or Vulkan draw calls to the Android graphics stack. These are intercepted by the **gfxstream** guest driver, serialized into a compact binary command buffer, and forwarded over the `virtio-gpu` PCI device to the host.

On the host side, the command buffer is decoded by the **gfxstream host library** and dispatched to the real GPU via Vulkan (or OpenGL as a fallback). The decoded frames are composited and displayed in the host window.

For games using OpenGL ES, **ANGLE** (Google's OpenGL ES → Vulkan translator) can be used to run GLES commands entirely on the Vulkan path, improving compatibility and performance on modern GPUs.

---

## Input Pipeline

```
Host OS input event (keyboard / mouse / gamepad)
        ↓
Keymapper (applies active game profile)
        ↓
Android InputEvent (touch tap, swipe, key)
        ↓
Injection via ADB (short-term) or virtio-input (planned)
        ↓
Android InputDispatcher → Game receives touch event
```

The keymapper stores profiles per game as JSON files. Each profile maps host inputs to Android gesture actions (tap, swipe, hold, drag, pinch).

---

## Performance Targets

| Scenario | Target |
|---|---|
| CPU-bound workload | ≥ 95% of native device |
| GPU rendering (Vulkan path) | ≥ 90% of native device |
| Input latency (virtio-input) | < 5ms |
| Boot time to home screen | < 10 seconds |
