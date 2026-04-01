# Phase 4 — Gamepad, ADB Transport, virtio-input & Keymapper UI Editor

## Overview

| Component | File | What it does |
|---|---|---|
| `GamepadBridge` | `input/gamepad_bridge_sdl.cpp` | SDL2 controller → Android AKEYCODE / stick-as-touch |
| `AdbInjector` | `input/adb_injector.cpp` | Touch/key injection over ADB TCP socket |
| `VirtioInputTransport` | `input/virtio_input.cpp` | Low-latency event injection via virtio-input Unix socket |
| `KeymapperEditor` | `overlay/keymapper_editor.cpp` | Drag-and-drop in-window keymap editor |

---

## Gamepad Bridge

### Up to 4 simultaneous controllers

SDL2 `SDL_GameController` API handles hot-plug automatically (CONTROLLERDEVICEADDED / REMOVED events).
Each controller occupies a slot 0–3; the `instance_to_slot_` map keeps them aligned across reconnects.

### Stick-as-Touch mode

The majority of mobile games don’t expose gamepad APIs — they only respond to touch.
When `set_stick_as_touch(true, center_x, center_y, radius)` is called:
- Left stick at rest → no touch event
- Left stick moves past dead zone → DOWN on slot 9, then continuous MOVE
- Left stick returns to center → UP on slot 9

This maps naturally to virtual joystick UIs common in mobile shooters (PUBG, COD Mobile, etc.).

### Dead zone

A 12% dead zone is applied on all axes to prevent drift from worn-out sticks.

---

## ADB Transport

### Wire protocol

Communicates with the local ADB server (default port 5037) using the official ADB wire protocol:
```
[4-byte hex length][payload]
```
Transport selector: `host:transport:emulator-<serial_port>` targets a specific emulator instance
so multiple AVM instances can run concurrently on ports 5554, 5556, etc.

### Touch injection

| TouchEvent type | ADB command |
|---|---|
| DOWN | `input touchscreen swipe x y x y 1` (1ms = instant tap-start) |
| MOVE | `input touchscreen swipe oldX oldY newX newY 16` |
| UP | `input touchscreen tap x y` |

### Foreground package detection

`foreground_package()` runs `dumpsys activity activities` and parses `mResumedActivity`.
The emulator core polls this every 500ms to auto-activate the correct keymap profile.

### Latency note

ADB shell commands incur ~20–50ms round-trip. For most games this is acceptable,
but for frame-precise rhythm games, prefer VirtioInputTransport.

---

## virtio-input Transport

### Architecture

```
Host process                        QEMU process
┌─────────────────┐             ┌─────────────────────────────┐
│ VirtioInputTransport│             │ virtio-input device              │
│                     │  Unix sock  │                                  │
│ enqueue(event)  ───┼────────────►│ guest evdev driver               │
│ flush_thread    ───┘             │ Android InputReader              │
└─────────────────┘             └─────────────────────────────┘
```

### Event format

Raw `struct input_event` (16 bytes: 8-byte timeval + type + code + value).
Uses Linux Multitouch Protocol B (slot-based) for touch:
- `ABS_MT_SLOT` → `ABS_MT_TRACKING_ID` → `ABS_MT_POSITION_X/Y` → `SYN_REPORT`

### QEMU launch flags

```bash
qemu-system-x86_64 \
  -device virtio-tablet-pci \
  -device virtio-keyboard-pci \
  ...
```

Or with a custom virtio-input socket:
```bash
  -device virtio-input-host-pci,evdev=/dev/input/eventX
```

### Latency

~1–3ms host-to-guest, vs ~20–50ms for ADB.

---

## Keymapper UI Editor

### Activation

Press **F4** to enter edit mode. The screen dims and all mapped keys appear as labelled circles.

### Controls

| Action | Result |
|---|---|
| Left-click empty area | Place new handle (then press a key to assign) |
| Drag handle | Reposition tap target |
| Right-click handle | Delete mapping |
| Del key | Delete selected handle |
| Scroll wheel on handle | Resize tap radius |
| F4 | Save profile and exit edit mode |
| Escape | Cancel pending key assignment |

### Persistence

On F4, `commit_to_profile()` writes the modified positions back into the `KeymapProfile`
and calls `Keymapper::save_profile()` — serialized to `~/.avm/keymaps/<name>.json`.

---

## Transport Selection Guide

| Use case | Recommended transport |
|---|---|
| Quick setup / debugging | ADB (`AdbInjector`) |
| Gaming (most titles) | ADB (sufficient at 20-50ms) |
| Rhythm games / frame-precise input | virtio-input (`VirtioInputTransport`) |
| Windows host | ADB only (virtio Unix socket not yet supported) |
| Linux host + custom QEMU build | virtio-input |

---

## Phase 5 Preview

- **Multi-instance manager** — launch and manage multiple AVM instances from one host UI
- **Screen recording** — capture frames from the gfxstream pipeline to MP4 via FFmpeg
- **APK installer UI** — drag-and-drop `.apk` install via `adb install`
- **Macro recorder** — record and replay touch/key sequences
