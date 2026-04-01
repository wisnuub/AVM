# Phase 3 — Input Bridge, Keymapper & FPS Overlay

## Overview

Phase 3 adds the full **gaming UX layer** on top of the Phase 2 GPU pipeline:

| Component | Location | Responsibility |
|---|---|---|
| `InputBridge` | `include/avm/input/input_bridge.h` | Abstract interface for host → guest event translation |
| `SdlInputBridge` | `input/input_bridge_sdl.cpp` | SDL2 implementation (mouse, keyboard, multitouch) |
| `Keymapper` | `input/keymapper/keymapper.cpp` | Named keymap profiles, JSON persistence, package activation |
| `FpsOverlay` | `overlay/fps_overlay.cpp` | In-window HUD: FPS, frame time, CPU, RAM, sparkline |

---

## Input Bridge

### Event Flow

```
SDL2 event loop
     │
     ▼
 SdlInputBridge::handle_sdl_event()
     │
     ├─ Mouse/Touch ──► normalize coords ──► TouchEvent ──► ADB inject
     │
     └─ Keyboard ─────► Keymapper::lookup() ─► tap match? ──► TouchEvent
                                              └─ no match ──► KeyEvent ──► ADB inject
```

### Multitouch Slots

Android's evdev multitouch protocol assigns events to **slots 0–9**.
- Native SDL finger events get unique slot IDs via `finger_slots_` map.
- Keyboard-mapped taps rotate through slots 1–9 so simultaneous key presses
  (e.g. W + Space + Shift in a battle royale) each get their own slot.

---

## Keymapper

### Profile Format (`.json`)

```json
{
  "name": "PUBG Mobile Default",
  "package": "com.tencent.ig",
  "mappings": [
    { "sc": 26, "label": "W",     "x": 0.5,  "y": 0.4,  "r": 0.04 },
    { "sc": 22, "label": "S",     "x": 0.5,  "y": 0.65, "r": 0.04 },
    { "sc": 4,  "label": "A",     "x": 0.35, "y": 0.55, "r": 0.04 },
    { "sc": 7,  "label": "D",     "x": 0.65, "y": 0.55, "r": 0.04 },
    { "sc": 44, "label": "Space", "x": 0.85, "y": 0.75, "r": 0.06 }
  ]
}
```

- `sc` = SDL_Scancode integer value.
- `x`, `y` = normalized screen position [0..1].
- `r` = tap radius as fraction of screen width (default 0.04).

Profiles live in `~/.avm/keymaps/` by default.
The emulator core calls `activate_for_package()` whenever the foreground app changes.

---

## FPS Overlay

Toggle with **F3**. Four display corners selectable at runtime.

### Metrics displayed

| Field | Source |
|---|---|
| FPS | `FramePresenter::measured_fps()` |
| Frame ms | Last frame duration |
| CPU % | `/proc/stat` (Linux) / `GetSystemTimes` (Windows) |
| RAM MB | Guest QEMU memory balloon stats |
| Renderer | `"Vulkan"` / `"OpenGL"` / `"Software"` |
| Sparkline | Rolling 60-sample FPS bar chart |

### FPS colour coding

| Range | Colour | Meaning |
|---|---|---|
| ≥ 55 FPS | 🟢 Green | Smooth |
| 30–54 FPS | 🟡 Yellow | Acceptable |
| < 30 FPS | 🔴 Red | Laggy |

---

## What's Next — Phase 4

- **Keymapper UI editor** — drag-and-drop overlay to visually place tap targets on the screen.
- **Gamepad support** — SDL2 joystick/controller → Android gamepad events via `AKEYCODE_BUTTON_*`.
- **ADB transport** — replace the placeholder inject calls with real `adb shell input tap/swipe` commands over a local socket.
- **virtio-input** — faster than ADB for production; inject events directly into the guest kernel.
