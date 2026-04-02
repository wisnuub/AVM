# Phase 5 — Android Version Selector & Profile System

## Overview

Phase 5 adds three capabilities on top of the Phase 4 input/virtio foundation:

1. **Android version selector** — `--android <version>` CLI flag with a full metadata table
2. **Image validation** — arch detection, RAM/vCPU auto-tuning, Apple Silicon compat check
3. **Per-game profile system** — JSON profiles that override config per game/app

---

## New CLI Flags

```
--android <ver>       Select Android version (13, android14, API33, tiramisu …)
--list-versions       Print the version table and exit
--fps <n>             FPS cap (default 60, use 0 for unlimited)
--fps-unlock          Alias for --fps 0
--profile <name>      Load a game profile from ~/.config/avm/profiles/<name>.json
--list-profiles       List all saved profiles
```

### Supported Android Versions

| Version    | Code Name         | API | Min Kernel | Min RAM |
|------------|-------------------|-----|------------|---------|
| Android 10 | Q                 | 29  | 4.14       | 2 GB    |
| Android 11 | R                 | 30  | 4.19       | 3 GB    |
| Android 12 | S                 | 31  | 5.4        | 4 GB    |
| Android 12L| Sv2               | 32  | 5.10       | 4 GB    |
| Android 13 | Tiramisu          | 33  | 5.15       | 4 GB    |
| Android 14 | Upside Down Cake  | 34  | 6.1        | 6 GB    |
| Android 15 | Vanilla Ice Cream | 35  | 6.6        | 6 GB    |

All of the following are equivalent for Android 13:
```bash
avm --android 13
avm --android android13
avm --android API33
avm --android tiramisu
```

---

## Profile System

Profiles are JSON files stored in:
- **Linux/macOS**: `~/.config/avm/profiles/<name>.json`
- **Windows**: `%APPDATA%\AVM\profiles\<name>.json`

### Example Profile (`genshin.json`)

```json
{
  "name": "genshin",
  "package_name": "com.miHoYo.GenshinImpact",
  "display_name": "Genshin Impact",
  "keymapper": "/home/user/.config/avm/keymaps/genshin.json",
  "android_version": 34,
  "memory_mb": 8192,
  "vcpu_cores": 6,
  "target_fps": 60,
  "fps_unlock": false
}
```

### Loading a Profile

```bash
# By name (looks in profiles_dir automatically)
avm --profile genshin --image android14.img

# By full path
avm --profile /path/to/my-profile.json --image android14.img
```

Profile fields merge with CLI args — explicit CLI args always win. So:
```bash
# Profile sets memory=8192, but --memory 4096 overrides it
avm --profile genshin --memory 4096 --image android14.img
```

---

## FPS Limiter

`FpsLimiter` (`core/fps_limiter.cpp`) uses a **sleep + spin hybrid** for sub-millisecond precision:

- Sleeps until ~1ms before the next frame deadline
- Busy-spins for the final millisecond
- Computes a rolling 60-frame average for the FPS display

The host render loop calls `limiter.wait()` once per frame, then can read `limiter.measured_fps()` to feed the overlay.

---

## Files Added / Modified

| File | Change |
|------|--------|
| `include/avm/core/android_version.h` | New — version enum + metadata table |
| `core/android_version.cpp` | New — table, parser, printer |
| `include/avm/core/image_manager.h` | New — image validation + arch compat |
| `core/image_manager.cpp` | New — sparse/raw detection, arch hints |
| `include/avm/core/profile.h` | New — Profile struct + API |
| `core/profile.cpp` | New — load/save JSON, apply_profile |
| `include/avm/core/fps_limiter.h` | New — precise FPS limiter |
| `core/fps_limiter.cpp` | New — sleep+spin, rolling average |
| `include/avm/core/config.h` | Updated — android_version, HVF enum |
| `src/main.cpp` | Updated — all new flags, profile loading |
| `core/CMakeLists.txt` | Updated — new sources wired in |
