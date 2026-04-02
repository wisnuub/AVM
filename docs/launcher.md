# AVM Launcher

The AVM Launcher is a PyQt6 desktop GUI that provides a full game-centre experience:
first-run setup wizard → game library → Play Store search → per-game profiles → one-click launch.

---

## Install & Run

```bash
# Install Python dependencies
pip install PyQt6

# Run the launcher
python3 launcher/main.py
```

---

## First-Run Wizard

On first launch (no Android image or no Google account stored), the wizard runs:

```
Step 1: Welcome
Step 2: Download Android Image
        • Android 14 / 13 / 12
        • Flavor: GApps (Play Store) or Vanilla
        • Calls avm-image-pull in the background
Step 3: Google Account
        • Enter your Gmail address
        • Stored locally in ~/.config/avm/account.json
        • Password is never stored — typed inside the Android VM
Step 4: Global Performance Profile
        • Choose device spoof (ROG Phone 9 Pro recommended)
        • Set RAM, FPS cap, GPU backend
Step 5: Done → opens Game Library
```

---

## Game Library

- Shows installed games + featured games as quick-launch cards
- Click **Play** to launch (uses global profile)
- Right-click a card → **Edit Profile** to set per-game overrides
- Right-click → **Remove** to uninstall

---

## Search Play Store

Click **Search** in the sidebar or use the search bar at the top.
Type a game name, genre ("MOBA", "RPG"), or developer name.
Click a result card to queue it for installation via ADB.

---

## Profile System (like PPSSPP)

### Global Profile

Set once, applies to every game unless overridden:

```json
{
  "spoof_profile": "rog9pro",
  "android":       14,
  "memory_mb":     6144,
  "cores":         4,
  "target_fps":    60,
  "gpu_backend":   "vulkan",
  "gapps":         true,
  "show_hud":      true
}
```

Edit via **Settings** in the sidebar or the **Global Profile** button.

### Per-Game Profile

Right-click any game card → **Edit Profile**.
Only set what you want to override — everything else falls through to global:

```json
{
  "spoof_profile": "rog9pro",
  "target_fps":    120,
  "memory_mb":     8192
}
```

Merge order: **global defaults ← per-game overrides ← CLI flags**

### Bundled profiles

The `profiles/` directory ships ready-to-use profiles:

| File | Game | Profile | FPS |
|---|---|---|---|
| `pubg.json` | PUBG Mobile | rog9pro | 90 |
| `codm.json` | CoD Mobile | rog9pro | 120 |
| `mlbb.json` | Mobile Legends | rog9pro | 120 |
| `genshin.json` | Genshin Impact | rog9pro | 60 |
| `honkai-sr.json` | Honkai: Star Rail | s25ultra | 60 |
| `wuthering-waves.json` | Wuthering Waves | rog9pro | 60 |

---

## Config file locations

| File | Purpose |
|---|---|
| `~/.config/avm/global.json` | Global profile |
| `~/.config/avm/account.json` | Google account email |
| `~/.config/avm/games.json` | Installed game registry + per-game profiles |
| `~/.config/avm/profiles/` | Per-game profile JSON files |
| `~/.config/avm/images/` | Downloaded Android images |
