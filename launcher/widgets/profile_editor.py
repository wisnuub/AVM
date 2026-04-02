"""Global profile editor widget (embedded in main window)."""

from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QFormLayout, QComboBox,
    QPushButton, QLabel, QSpinBox, QGroupBox,
    QHBoxLayout, QCheckBox,
)
from PyQt6.QtCore import pyqtSignal
from launcher.config import LauncherConfig


DEVICE_PROFILES = [
    ("rog9pro",   "ROG Phone 9 Pro  —  Best for gaming (120fps)"),
    ("rog8pro",   "ROG Phone 8 Pro  —  Android 14, wide compat"),
    ("s25ultra",  "Samsung Galaxy S25 Ultra"),
    ("s24ultra",  "Samsung Galaxy S24 Ultra"),
    ("pixel9pro", "Google Pixel 9 Pro  —  Best Play Integrity"),
    ("pixel8pro", "Google Pixel 8 Pro  —  Default / safe"),
]


class GlobalProfileEditor(QWidget):
    saved = pyqtSignal()

    def __init__(self, config: LauncherConfig, parent=None):
        super().__init__(parent)
        self.config = config
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        title = QLabel("Global Profile")
        title.setStyleSheet("font-size:16px; font-weight:700; color:#e0e0e0; margin-bottom:8px;")
        layout.addWidget(title)

        desc = QLabel(
            "These settings apply to all games unless overridden by a per-game profile."
        )
        desc.setWordWrap(True)
        desc.setStyleSheet("color:#8a8a9a; font-size:12px; margin-bottom:16px;")
        layout.addWidget(desc)

        box = QGroupBox()
        box.setStyleSheet(
            "QGroupBox { background:#1a1a2e; border:1px solid #2a2a40; border-radius:8px; padding:16px; }"
        )
        form = QFormLayout(box)
        form.setSpacing(12)

        # Device profile
        self.device_combo = QComboBox()
        for key, label in DEVICE_PROFILES:
            self.device_combo.addItem(label, key)
        form.addRow("Device profile:", self.device_combo)

        # FPS
        self.fps_combo = QComboBox()
        for label, val in [("30 fps",30),("60 fps",60),("90 fps",90),("120 fps",120),("Unlimited",0)]:
            self.fps_combo.addItem(label, val)
        form.addRow("Target FPS:", self.fps_combo)

        # RAM
        self.ram_combo = QComboBox()
        for label, val in [("4 GB",4096),("6 GB",6144),("8 GB",8192),("10 GB",10240),("12 GB",12288)]:
            self.ram_combo.addItem(label, val)
        form.addRow("RAM allocation:", self.ram_combo)

        # GPU
        self.gpu_combo = QComboBox()
        for label, val in [("Vulkan (Recommended)","vulkan"),("OpenGL","opengl"),("Software (slow)","software")]:
            self.gpu_combo.addItem(label, val)
        form.addRow("GPU backend:", self.gpu_combo)

        # GApps toggle
        self.gapps_cb = QCheckBox("Enable GApps (Google Play Store)")
        self.gapps_cb.setStyleSheet("color:#e0e0e0;")
        form.addRow("", self.gapps_cb)

        # HUD toggle
        self.hud_cb = QCheckBox("Show performance overlay (HUD)")
        self.hud_cb.setStyleSheet("color:#e0e0e0;")
        form.addRow("", self.hud_cb)

        layout.addWidget(box)

        # Save button
        save_btn = QPushButton("  Save Global Profile")
        save_btn.setFixedHeight(40)
        save_btn.setStyleSheet(
            "background:#00d4aa; color:#000; font-weight:bold; border-radius:6px; "
            "font-size:13px; margin-top:12px;"
        )
        save_btn.clicked.connect(self._save)
        layout.addWidget(save_btn)
        layout.addStretch()

    def load(self):
        cfg = self.config.global_cfg
        profile = cfg.get("spoof_profile", "rog9pro")
        for i, (key, _) in enumerate(DEVICE_PROFILES):
            if key == profile:
                self.device_combo.setCurrentIndex(i)
                break

        fps = cfg.get("target_fps", 60)
        fps_map = {30:0, 60:1, 90:2, 120:3, 0:4}
        self.fps_combo.setCurrentIndex(fps_map.get(fps, 1))

        ram = cfg.get("memory_mb", 6144)
        ram_map = {4096:0, 6144:1, 8192:2, 10240:3, 12288:4}
        self.ram_combo.setCurrentIndex(ram_map.get(ram, 1))

        gpu = cfg.get("gpu_backend", "vulkan")
        gpu_map = {"vulkan":0, "opengl":1, "software":2}
        self.gpu_combo.setCurrentIndex(gpu_map.get(gpu, 0))

        self.gapps_cb.setChecked(cfg.get("gapps", True))
        self.hud_cb.setChecked(cfg.get("show_hud", True))

    def _save(self):
        self.config.global_cfg.update({
            "spoof_profile": self.device_combo.currentData(),
            "target_fps":    self.fps_combo.currentData(),
            "memory_mb":     self.ram_combo.currentData(),
            "gpu_backend":   self.gpu_combo.currentData(),
            "gapps":         self.gapps_cb.isChecked(),
            "show_hud":      self.hud_cb.isChecked(),
        })
        self.config.save_global()
        self.saved.emit()
