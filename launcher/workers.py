"""Background QThread workers for the launcher."""

import subprocess
import sys
from pathlib import Path
from typing import List, Dict

from PyQt6.QtCore import QThread, pyqtSignal


# ---------------------------------------------------------------------------
class ImagePullWorker(QThread):
    """Runs avm-image-pull in a background thread."""
    progress = pyqtSignal(int)
    log      = pyqtSignal(str)
    finished = pyqtSignal(bool, str)

    def __init__(self, android: str, arch: str, flavor: str):
        super().__init__()
        self.android = android
        self.arch    = arch
        self.flavor  = flavor

    def run(self):
        script = Path(__file__).parent.parent / "tools" / "avm-image-pull.py"
        cmd = [
            sys.executable, str(script),
            "--android", self.android,
            "--arch",    self.arch,
            "--flavor",  self.flavor,
        ]
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            lines_seen = 0
            for line in proc.stdout:
                line = line.rstrip()
                self.log.emit(line)
                lines_seen += 1
                # Rough progress: first 50 lines = download, 50-80 = extract, 80+ = inject
                if lines_seen < 50:
                    self.progress.emit(int(lines_seen / 50 * 60))
                elif lines_seen < 80:
                    self.progress.emit(60 + int((lines_seen - 50) / 30 * 30))
                else:
                    self.progress.emit(95)
            proc.wait()
            if proc.returncode == 0:
                self.progress.emit(100)
                self.finished.emit(True, "✓ Image downloaded and ready.")
            else:
                self.finished.emit(False, "✗ Download failed. Check log above.")
        except Exception as e:
            self.finished.emit(False, f"✗ Error: {e}")


# ---------------------------------------------------------------------------
# Curated game database for Play Store search
# In a real build this would hit the Play Store internal search API via ADB
# or a self-hosted game metadata service. For now it's a local fuzzy-search
# over a curated catalog.
GAME_CATALOG: List[Dict] = [
    {"name": "PUBG Mobile",             "package": "com.tencent.ig",                        "icon_emoji": "🔫", "genre": "Battle Royale", "developer": "Krafton"},
    {"name": "PUBG: New State",          "package": "com.pubg.newstate",                     "icon_emoji": "🔫", "genre": "Battle Royale", "developer": "Krafton"},
    {"name": "Call of Duty Mobile",      "package": "com.activision.callofduty.shooter",     "icon_emoji": "🎖️", "genre": "FPS",           "developer": "Activision"},
    {"name": "Mobile Legends: Bang Bang","package": "com.mobile.legends",                   "icon_emoji": "⚔️", "genre": "MOBA",          "developer": "Moonton"},
    {"name": "Genshin Impact",            "package": "com.miHoYo.GenshinImpact",             "icon_emoji": "🗺️", "genre": "RPG",           "developer": "HoYoverse"},
    {"name": "Honkai: Star Rail",         "package": "com.HoYoverse.hkrpgoversea",          "icon_emoji": "⭐", "genre": "RPG",           "developer": "HoYoverse"},
    {"name": "Wuthering Waves",           "package": "com.kurogame.wutheringwaves.global",  "icon_emoji": "🌊", "genre": "RPG",           "developer": "Kuro Games"},
    {"name": "Zenless Zone Zero",         "package": "com.HoYoverse.Nap",                   "icon_emoji": "⚡", "genre": "RPG",           "developer": "HoYoverse"},
    {"name": "Free Fire",                 "package": "com.dts.freefireth",                  "icon_emoji": "🔥", "genre": "Battle Royale", "developer": "Garena"},
    {"name": "Free Fire MAX",             "package": "com.dts.freefiremax",                 "icon_emoji": "🔥", "genre": "Battle Royale", "developer": "Garena"},
    {"name": "Clash of Clans",            "package": "com.supercell.clashofclans",          "icon_emoji": "🏰", "genre": "Strategy",     "developer": "Supercell"},
    {"name": "Clash Royale",              "package": "com.supercell.clashroyale",           "icon_emoji": "🃏", "genre": "Strategy",     "developer": "Supercell"},
    {"name": "Brawl Stars",               "package": "com.supercell.brawlstars",            "icon_emoji": "💥", "genre": "Action",       "developer": "Supercell"},
    {"name": "Arena of Valor",            "package": "com.ngame.allstar.eu",                "icon_emoji": "🗡️", "genre": "MOBA",          "developer": "TiMi Studio"},
    {"name": "League of Legends: Wild Rift","package":"com.riotgames.league.wildrift",     "icon_emoji": "🏆", "genre": "MOBA",          "developer": "Riot Games"},
    {"name": "Minecraft",                 "package": "com.mojang.minecraftpe",              "icon_emoji": "⛰️", "genre": "Sandbox",      "developer": "Mojang"},
    {"name": "Roblox",                    "package": "com.roblox.client",                   "icon_emoji": "🎲", "genre": "Sandbox",      "developer": "Roblox"},
    {"name": "Among Us",                  "package": "com.innersloth.spacemafia",           "icon_emoji": "👾", "genre": "Party",        "developer": "InnerSloth"},
    {"name": "Stumble Guys",              "package": "com.scopely.stumbleguys",             "icon_emoji": "🏃", "genre": "Party",        "developer": "Scopely"},
    {"name": "Candy Crush Saga",          "package": "com.king.candycrushsaga",            "icon_emoji": "🍬", "genre": "Puzzle",       "developer": "King"},
]


class PlayStoreSearchWorker(QThread):
    """
    Searches the local game catalog for matching titles.
    In a future build: install the APK via ADB after user confirms.
    """
    results = pyqtSignal(list)

    def __init__(self, query: str):
        super().__init__()
        self.query = query.lower()

    def run(self):
        import time
        time.sleep(0.3)  # Simulate network latency
        matches = [
            g for g in GAME_CATALOG
            if self.query in g["name"].lower()
            or self.query in g.get("genre", "").lower()
            or self.query in g.get("developer", "").lower()
        ]
        self.results.emit(matches)
