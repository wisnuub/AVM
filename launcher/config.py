import json
import os
import platform
from pathlib import Path
from typing import Optional, List, Dict, Any


def _config_root() -> Path:
    if platform.system() == "Windows":
        return Path(os.environ.get("APPDATA", Path.home() / "AppData" / "Roaming")) / "AVM"
    elif platform.system() == "Darwin":
        return Path.home() / "Library" / "Application Support" / "AVM"
    else:
        return Path.home() / ".config" / "avm"


CONFIG_ROOT   = _config_root()
IMAGES_DIR    = CONFIG_ROOT / "images"
PROFILES_DIR  = CONFIG_ROOT / "profiles"
GAMES_DB      = CONFIG_ROOT / "games.json"
GLOBAL_CFG    = CONFIG_ROOT / "global.json"
ACCOUNT_FILE  = CONFIG_ROOT / "account.json"


DEFAULT_GLOBAL = {
    "spoof_profile":  "rog9pro",
    "android":        14,
    "memory_mb":      6144,
    "cores":          4,
    "target_fps":     60,
    "gpu_backend":    "vulkan",
    "gapps":          True,
    "fps_cap_mode":   "precise",
    "show_hud":       True,
    "adb_port":       5555,
}


class LauncherConfig:
    def __init__(self):
        CONFIG_ROOT.mkdir(parents=True, exist_ok=True)
        IMAGES_DIR.mkdir(parents=True, exist_ok=True)
        PROFILES_DIR.mkdir(parents=True, exist_ok=True)

        self.global_cfg: Dict[str, Any] = self._load_json(GLOBAL_CFG, DEFAULT_GLOBAL)
        self.games:      List[Dict]     = self._load_json(GAMES_DB,   [])
        self.account:    Dict[str, Any] = self._load_json(ACCOUNT_FILE, {})

        # Seed with bundled profiles from the repo profiles/ folder
        self._seed_bundled_profiles()

    # ------------------------------------------------------------------
    def needs_first_run(self) -> bool:
        images = list(IMAGES_DIR.glob("*/avm-image.json"))
        return len(images) == 0 or not self.account.get("google_email")

    def get_default_image(self) -> Optional[Path]:
        for meta in sorted(IMAGES_DIR.glob("*/avm-image.json")):
            with open(meta) as f:
                d = json.load(f)
            img = Path(d.get("image_file", ""))
            if img.exists():
                return img
        return None

    def list_images(self) -> List[Dict]:
        result = []
        for meta in sorted(IMAGES_DIR.glob("*/avm-image.json")):
            with open(meta) as f:
                result.append(json.load(f))
        return result

    # ------------------------------------------------------------------
    def save_global(self):
        self._save_json(GLOBAL_CFG, self.global_cfg)

    def save_account(self):
        self._save_json(ACCOUNT_FILE, self.account)

    def save_games(self):
        self._save_json(GAMES_DB, self.games)

    # ------------------------------------------------------------------
    def get_game(self, package: str) -> Optional[Dict]:
        for g in self.games:
            if g.get("package") == package:
                return g
        return None

    def upsert_game(self, game: Dict):
        for i, g in enumerate(self.games):
            if g.get("package") == game["package"]:
                self.games[i] = game
                self.save_games()
                return
        self.games.append(game)
        self.save_games()

    def remove_game(self, package: str):
        self.games = [g for g in self.games if g.get("package") != package]
        self.save_games()

    # ------------------------------------------------------------------
    def load_profile(self, package: str) -> Dict[str, Any]:
        """Return merged profile: global defaults ← per-game overrides."""
        base = dict(self.global_cfg)
        game = self.get_game(package)
        if game and "profile" in game:
            base.update(game["profile"])
        return base

    def save_game_profile(self, package: str, profile: Dict[str, Any]):
        game = self.get_game(package) or {"package": package}
        game["profile"] = profile
        self.upsert_game(game)

    # ------------------------------------------------------------------
    def list_profiles(self) -> List[Dict]:
        """List all per-game profile JSON files from the user profiles dir."""
        result = []
        for p in sorted(PROFILES_DIR.glob("*.json")):
            with open(p) as f:
                result.append(json.load(f))
        return result

    # ------------------------------------------------------------------
    def _seed_bundled_profiles(self):
        """Copy bundled profiles/ from the repo into the user config dir."""
        repo_profiles = Path(__file__).parent.parent / "profiles"
        if not repo_profiles.exists():
            return
        for src in repo_profiles.glob("*.json"):
            dst = PROFILES_DIR / src.name
            if not dst.exists():
                import shutil
                shutil.copy(src, dst)

    # ------------------------------------------------------------------
    @staticmethod
    def _load_json(path: Path, default):
        if path.exists():
            with open(path) as f:
                return json.load(f)
        return default if not isinstance(default, dict) else dict(default)

    @staticmethod
    def _save_json(path: Path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            json.dump(data, f, indent=2)
