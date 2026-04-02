"""Per-game profile override dialog."""

from PyQt6.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QComboBox,
    QPushButton, QLabel, QDialogButtonBox,
    QCheckBox, QGroupBox, QHBoxLayout,
)
from PyQt6.QtCore import Qt
from launcher.config import LauncherConfig

DEVICE_PROFILES = [
    ("global",   "Use Global Profile (default)"),
    ("rog9pro",  "ROG Phone 9 Pro  —  120fps gaming"),
    ("rog8pro",  "ROG Phone 8 Pro  —  Android 14"),
    ("s25ultra", "Samsung Galaxy S25 Ultra"),
    ("s24ultra", "Samsung Galaxy S24 Ultra"),
    ("pixel9pro","Google Pixel 9 Pro"),
    ("pixel8pro","Google Pixel 8 Pro"),
]


class GameProfileDialog(QDialog):
    def __init__(self, package: str, config: LauncherConfig, parent=None):
        super().__init__(parent)
        self.package = package
        self.config  = config
        self.setWindowTitle(f"Profile — {package}")
        self.setMinimumWidth(480)
        self.setStyleSheet(
            "QDialog { background:#1a1a2e; color:#e0e0e0; }"
            "QLabel  { color:#e0e0e0; }"
            "QGroupBox { background:#12122a; border:1px solid #2a2a40; border-radius:8px; padding:12px; }"
        )
        self._build_ui()
        self._load()

    def _build_ui(self):
        layout = QVBoxLayout(self)

        header = QLabel(f"⚙️  Per-game profile for:\n{self.package}")
        header.setStyleSheet("font-size:13px; color:#8a8a9a; margin-bottom:12px;")
        layout.addWidget(header)

        note = QLabel(
            "Override specific settings for this game. "
            "Leave as 'Use Global Profile' to inherit the global default."
        )
        note.setWordWrap(True)
        note.setStyleSheet("color:#8a8a9a; font-size:11px; margin-bottom:8px;")
        layout.addWidget(note)

        box = QGroupBox()
        form = QFormLayout(box)
        form.setSpacing(10)

        self.device_combo = QComboBox()
        for key, label in DEVICE_PROFILES:
            self.device_combo.addItem(label, key)
        form.addRow("Device profile:", self.device_combo)

        self.fps_combo = QComboBox()
        for label, val in [
            ("Use global", None),("30 fps",30),("60 fps",60),
            ("90 fps",90),("120 fps",120),("Unlimited",0)
        ]:
            self.fps_combo.addItem(label, val)
        form.addRow("Target FPS:", self.fps_combo)

        self.ram_combo = QComboBox()
        for label, val in [
            ("Use global",None),("4 GB",4096),("6 GB",6144),
            ("8 GB",8192),("10 GB",10240),("12 GB",12288)
        ]:
            self.ram_combo.addItem(label, val)
        form.addRow("RAM allocation:", self.ram_combo)

        self.gpu_combo = QComboBox()
        for label, val in [
            ("Use global",None),("Vulkan","vulkan"),("OpenGL","opengl"),("Software","software")
        ]:
            self.gpu_combo.addItem(label, val)
        form.addRow("GPU backend:", self.gpu_combo)

        layout.addWidget(box)

        # Buttons
        btns = QDialogButtonBox(
            QDialogButtonBox.StandardButton.Save |
            QDialogButtonBox.StandardButton.Cancel
        )
        btns.setStyleSheet(
            "QPushButton { background:#00d4aa; color:#000; border-radius:6px; padding:6px 18px; font-weight:bold; }"
            "QPushButton[text='Cancel'] { background:#1e1e30; color:#e0e0e0; }"
        )
        btns.accepted.connect(self._save)
        btns.rejected.connect(self.reject)
        layout.addWidget(btns)

    def _load(self):
        game = self.config.get_game(self.package)
        if not game or "profile" not in game:
            return
        p = game["profile"]
        profile_key = p.get("spoof_profile", "global")
        for i, (k, _) in enumerate(DEVICE_PROFILES):
            if k == profile_key:
                self.device_combo.setCurrentIndex(i)
                break
        fps = p.get("target_fps")
        fps_map = {None:0, 30:1, 60:2, 90:3, 120:4, 0:5}
        self.fps_combo.setCurrentIndex(fps_map.get(fps, 0))
        ram = p.get("memory_mb")
        ram_map = {None:0, 4096:1, 6144:2, 8192:3, 10240:4, 12288:5}
        self.ram_combo.setCurrentIndex(ram_map.get(ram, 0))
        gpu = p.get("gpu_backend")
        gpu_map = {None:0, "vulkan":1, "opengl":2, "software":3}
        self.gpu_combo.setCurrentIndex(gpu_map.get(gpu, 0))

    def _save(self):
        profile = {}
        dev = self.device_combo.currentData()
        if dev and dev != "global":
            profile["spoof_profile"] = dev
        fps = self.fps_combo.currentData()
        if fps is not None:
            profile["target_fps"] = fps
        ram = self.ram_combo.currentData()
        if ram is not None:
            profile["memory_mb"] = ram
        gpu = self.gpu_combo.currentData()
        if gpu is not None:
            profile["gpu_backend"] = gpu
        self.config.save_game_profile(self.package, profile)
        self.accept()
