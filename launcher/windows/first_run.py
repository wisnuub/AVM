"""First-run setup wizard: download image → Google account → done."""

import subprocess
import sys
from pathlib import Path

from PyQt6.QtWidgets import (
    QWizard, QWizardPage, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QComboBox,
    QProgressBar, QTextEdit, QCheckBox, QGroupBox,
    QFormLayout, QSizePolicy,
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QSize
from PyQt6.QtGui import QFont, QPixmap, QIcon

from launcher.config import LauncherConfig
from launcher.workers import ImagePullWorker


# ---------------------------------------------------------------------------
class WelcomePage(QWizardPage):
    def __init__(self):
        super().__init__()
        self.setTitle("Welcome to AVM")
        self.setSubTitle("Android Virtual Machine  —  Gaming Edition")
        layout = QVBoxLayout(self)

        desc = QLabel(
            "AVM runs Android games on your PC at near-native performance "
            "using hardware virtualization.\n\n"
            "This wizard will:\n"
            "  •  Download a ready-to-run Android image\n"
            "  •  Help you sign in with your Google Account\n"
            "  •  Set your global performance profile\n\n"
            "Setup takes about 5 minutes."
        )
        desc.setWordWrap(True)
        desc.setStyleSheet("font-size: 13px; line-height: 1.6; color: #e0e0e0;")
        layout.addWidget(desc)
        layout.addStretch()


# ---------------------------------------------------------------------------
class DownloadPage(QWizardPage):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.setTitle("Download Android Image")
        self.setSubTitle("Choose the Android version and flavor to download.")
        self._complete = False
        self._worker = None

        layout = QVBoxLayout(self)

        form = QFormLayout()
        self.android_combo = QComboBox()
        self.android_combo.addItems(["Android 14 (Recommended)", "Android 13", "Android 12"])
        self.flavor_combo = QComboBox()
        self.flavor_combo.addItems(["GApps (Google Play Store)", "Vanilla (AOSP)"])
        form.addRow("Android version:", self.android_combo)
        form.addRow("Image flavor:",    self.flavor_combo)
        layout.addLayout(form)

        self.download_btn = QPushButton("  Download")
        self.download_btn.setFixedHeight(40)
        self.download_btn.setStyleSheet(
            "background:#00d4aa; color:#000; font-weight:bold; border-radius:6px;"
        )
        self.download_btn.clicked.connect(self._start_download)
        layout.addWidget(self.download_btn)

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setVisible(False)
        layout.addWidget(self.progress)

        self.log = QTextEdit()
        self.log.setReadOnly(True)
        self.log.setFixedHeight(120)
        self.log.setStyleSheet("font-family: monospace; font-size: 11px; background:#0d0d1a; color:#00d4aa;")
        layout.addWidget(self.log)

        # Skip option for users who already have an image
        self.skip_cb = QCheckBox("I already have an image — skip download")
        self.skip_cb.stateChanged.connect(self._on_skip_changed)
        layout.addWidget(self.skip_cb)

        layout.addStretch()

    def _start_download(self):
        ver_map = {0: "14", 1: "13", 2: "12"}
        flv_map = {0: "gapps", 1: "vanilla"}
        android = ver_map[self.android_combo.currentIndex()]
        flavor  = flv_map[self.flavor_combo.currentIndex()]

        self.download_btn.setEnabled(False)
        self.progress.setVisible(True)
        self.progress.setValue(0)
        self.log.clear()

        self._worker = ImagePullWorker(android, "arm64", flavor)
        self._worker.progress.connect(self.progress.setValue)
        self._worker.log.connect(lambda msg: self.log.append(msg))
        self._worker.finished.connect(self._on_download_done)
        self._worker.start()

    def _on_download_done(self, success: bool, message: str):
        self.log.append(message)
        if success:
            self._complete = True
            self.completeChanged.emit()
            self.download_btn.setText("  Downloaded ")
        else:
            self.download_btn.setEnabled(True)
            self.progress.setValue(0)

    def _on_skip_changed(self, state):
        self._complete = bool(state)
        self.completeChanged.emit()

    def isComplete(self) -> bool:
        return self._complete


# ---------------------------------------------------------------------------
class AccountPage(QWizardPage):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.setTitle("Google Account")
        self.setSubTitle(
            "Sign in to access the Play Store inside the emulator.\n"
            "Your credentials are only stored locally and used to "
            "configure ADB auto-login."
        )
        layout = QVBoxLayout(self)

        info = QLabel(
            "🔒  AVM stores your email locally to auto-configure Play Store on first boot.\n"
            "   Your password is never stored — sign-in happens inside the Android VM."
        )
        info.setWordWrap(True)
        info.setStyleSheet("color:#8a8a9a; font-size:11px; padding:8px; "
                           "background:#1e1e30; border-radius:6px;")
        layout.addWidget(info)

        form = QFormLayout()
        self.email_edit = QLineEdit()
        self.email_edit.setPlaceholderText("you@gmail.com")
        self.email_edit.textChanged.connect(self._on_text_changed)
        form.addRow("Google email:", self.email_edit)
        layout.addLayout(form)

        note = QLabel(
            "After first boot, AVM will open the Google sign-in screen inside "
            "Android. Enter your password there directly."
        )
        note.setWordWrap(True)
        note.setStyleSheet("color:#8a8a9a; font-size:11px; margin-top:8px;")
        layout.addWidget(note)
        layout.addStretch()

        self.registerField("google_email*", self.email_edit)

    def _on_text_changed(self, text):
        self.completeChanged.emit()

    def isComplete(self) -> bool:
        return "@" in self.email_edit.text()

    def validatePage(self) -> bool:
        self.config.account["google_email"] = self.email_edit.text().strip()
        self.config.save_account()
        return True


# ---------------------------------------------------------------------------
class GlobalProfilePage(QWizardPage):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.setTitle("Global Performance Profile")
        self.setSubTitle(
            "This is the default profile for all games. "
            "You can override it per-game later."
        )
        layout = QVBoxLayout(self)

        form = QFormLayout()

        self.device_combo = QComboBox()
        self.device_combo.addItems([
            "ROG Phone 9 Pro  —  Best for gaming (120fps unlocks)",
            "ROG Phone 8 Pro  —  Android 14, wide compat",
            "Samsung Galaxy S25 Ultra",
            "Samsung Galaxy S24 Ultra",
            "Google Pixel 9 Pro  —  Best Play Integrity",
            "Google Pixel 8 Pro  —  Default / safe",
        ])
        self._device_keys = ["rog9pro", "rog8pro", "s25ultra", "s24ultra", "pixel9pro", "pixel8pro"]

        self.fps_combo = QComboBox()
        self.fps_combo.addItems(["60 fps", "90 fps", "120 fps", "Unlimited"])
        self._fps_values = [60, 90, 120, 0]

        self.ram_combo = QComboBox()
        self.ram_combo.addItems(["4 GB", "6 GB (Recommended)", "8 GB", "10 GB", "12 GB"])
        self._ram_values = [4096, 6144, 8192, 10240, 12288]
        self.ram_combo.setCurrentIndex(1)

        self.gpu_combo = QComboBox()
        self.gpu_combo.addItems(["Vulkan (Recommended)", "OpenGL", "Software (slow)"])
        self._gpu_values = ["vulkan", "opengl", "software"]

        form.addRow("Device profile:",  self.device_combo)
        form.addRow("Target FPS:",      self.fps_combo)
        form.addRow("RAM allocation:",  self.ram_combo)
        form.addRow("GPU backend:",     self.gpu_combo)
        layout.addLayout(form)

        hint = QLabel(
            "ℹ️  ROG Phone 9 Pro is recommended — it unlocks 120fps modes in most games."
        )
        hint.setWordWrap(True)
        hint.setStyleSheet("color:#00d4aa; font-size:11px; margin-top:8px;")
        layout.addWidget(hint)
        layout.addStretch()

    def validatePage(self) -> bool:
        self.config.global_cfg.update({
            "spoof_profile": self._device_keys[self.device_combo.currentIndex()],
            "target_fps":    self._fps_values[self.fps_combo.currentIndex()],
            "memory_mb":     self._ram_values[self.ram_combo.currentIndex()],
            "gpu_backend":   self._gpu_values[self.gpu_combo.currentIndex()],
        })
        self.config.save_global()
        return True


# ---------------------------------------------------------------------------
class DonePage(QWizardPage):
    def __init__(self):
        super().__init__()
        self.setTitle("All Set!")
        layout = QVBoxLayout(self)
        msg = QLabel(
            "✓  Android image ready\n"
            "✓  Google Account configured\n"
            "✓  Performance profile saved\n\n"
            "Click Finish to open the game library."
        )
        msg.setStyleSheet("font-size: 14px; line-height: 2; color: #e0e0e0;")
        layout.addWidget(msg)
        layout.addStretch()


# ---------------------------------------------------------------------------
class FirstRunWizard(QWizard):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.setWindowTitle("AVM Setup")
        self.setWizardStyle(QWizard.WizardStyle.ModernStyle)
        self.setFixedSize(680, 520)

        self.addPage(WelcomePage())
        self.addPage(DownloadPage(config))
        self.addPage(AccountPage(config))
        self.addPage(GlobalProfilePage(config))
        self.addPage(DonePage())

        self.setButtonText(QWizard.WizardButton.FinishButton, "Open Game Library")
