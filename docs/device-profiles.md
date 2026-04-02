# Device Spoof Profiles — Which to Use and Why

## Quick Decision Guide

| Goal | Best Profile | Why |
|---|---|---|
| **Max high-FPS unlock** (120/144 Hz) | `rog9pro` | Most games explicitly whitelist ROG Phone for highest FPS tiers |
| **Samsung 120fps** (prefer Samsung ecosystem) | `s25ultra` | Samsung Game Booster hints, on PUBG/MLBB 120fps whitelist |
| **Play Integrity / banking apps** | `pixel8pro` | Google's own device, highest CTS pass rate, default |
| **Android 15 + newest features** | `pixel9pro` or `rog9pro` | Both ship Android 15 with API 35 |
| **Widest Android 14 game compat** | `rog8pro` or `s24ultra` | Battle-tested fingerprints, huge game whitelist coverage |

---

## Why ROG Phone Wins for FPS Unlock

Game developers maintain **server-side device allowlists** for high-FPS modes. A game running on an "unknown" device defaults to the lowest FPS/graphics tier. ROG phones are in more of these allowlists than any other Android brand because:

- ASUS specifically partners with PUBG Mobile, CoD Mobile, MLBB, and Genshin Impact for ROG-specific FPS modes
- ROG Phone 8/9 Pro are explicitly named in PUBG Mobile's 120fps device list
- The `ro.vendor.asus.gaming_mode=1` prop triggers game-engine-level detection in Unity and Unreal-based titles
- ROG Phone 9 Pro uses the same Snapdragon 8 Elite as the S25 Ultra — same raw performance tier, but better game-specific hints

---

## Why Samsung is Still Good

- Samsung Galaxy S23/S24/S25 series are on PUBG Mobile's 120fps whitelist
- `ro.vendor.samsung.freecess.enable=1` triggers Samsung Game Booster optimisation paths in Unity games
- Samsung models have broader recognition in older games that haven't updated their ROG allowlists
- S25 Ultra fingerprint passes Play Integrity cleanly

---

## Profile Reference

### `rog9pro` — ASUS ROG Phone 9 Pro

```
Model:        ASUS_AI2501
Chipset:      Snapdragon 8 Elite
Android:      15 (API 35)
Display:      165 Hz AMOLED
FP:           asus/WW_AI2501/ASUS_AI2501:15/.../release-keys
Extra props:  ro.vendor.asus.gaming_mode=1
              ro.asus.gamekey.support=1
              vendor.perf.fps_switch=1
```

**Games unlocked at highest tier:** PUBG Mobile (120fps), CoD Mobile (120fps Max), MLBB (120fps Ultra), Genshin Impact (60fps High), Wuthering Waves (60fps High/Very High), Honkai Star Rail (60fps High)

---

### `rog8pro` — ASUS ROG Phone 8 Pro

```
Model:        ASUS_AI2401
Chipset:      Snapdragon 8 Gen 3
Android:      14 (API 34)
FP:           asus/WW_AI2401/ASUS_AI2401:14/.../release-keys
Extra props:  ro.vendor.asus.gaming_mode=1
```

Better choice if you're using an Android 14 image and want maximum compatibility with games that haven't updated for Android 15.

---

### `s25ultra` — Samsung Galaxy S25 Ultra

```
Model:        SM-S938B (international)
Chipset:      Snapdragon 8 Elite
Android:      15 (API 35)
FP:           samsung/dm3qxxx/dm3q:15/.../release-keys
Extra props:  ro.vendor.samsung.freecess.enable=1
              debug.sf.hw=1
```

**Games unlocked:** PUBG Mobile (120fps), MLBB (120fps), CoD Mobile (120fps), most titles that check for Samsung flagship.

---

### `s24ultra` — Samsung Galaxy S24 Ultra

```
Model:        SM-S928B (international)
Chipset:      Snapdragon 8 Gen 3
Android:      14 (API 34)
FP:           samsung/e3qxxx/e3q:14/.../release-keys
```

---

### `pixel8pro` / `pixel9pro` — Google Pixel

Best for Play Integrity. Not optimised for high-FPS unlock (Pixel is not on most game FPS allowlists), but passes SafetyNet cleanly for every app.

---

## Usage

```bash
# Gaming: ROG profile (recommended)
avm --android 14 --image <img> --gapps --spoof-profile rog9pro

# Samsung alternative
avm --android 14 --image <img> --gapps --spoof-profile s25ultra

# Via per-game profile (easiest)
avm --profile pubg   --image <img>   # rog9pro, 90fps, 8GB RAM
avm --profile codm   --image <img>   # rog9pro, 120fps, 8GB RAM
avm --profile mlbb   --image <img>   # rog9pro, 120fps, 6GB RAM
avm --profile genshin --image <img>  # rog9pro, 60fps, 8GB RAM
avm --profile honkai-sr --image <img> # s25ultra, 60fps, 10GB RAM
avm --profile wuthering-waves --image <img> # rog9pro, 60fps, 10GB RAM
```
