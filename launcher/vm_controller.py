"""VMController — manages launching/stopping the AVM process from the launcher."""

import subprocess
import shutil
import sys
from pathlib import Path
from typing import Optional, Dict, Any

from PyQt6.QtCore import QObject, pyqtSignal, QThread

from launcher.config import LauncherConfig


class _LaunchThread(QThread):
    status = pyqtSignal(bool, str)

    def __init__(self, cmd, parent=None):
        super().__init__(parent)
        self.cmd = cmd
        self._proc = None

    def run(self):
        try:
            self._proc = subprocess.Popen(
                self.cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            self.status.emit(True, " ".join(self.cmd[:3]))
            self._proc.wait()
            self.status.emit(False, "exited")
        except Exception as e:
            self.status.emit(False, str(e))

    def stop(self):
        if self._proc:
            self._proc.terminate()


class VMController(QObject):
    status_changed = pyqtSignal(bool, str)

    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config  = config
        self._thread: Optional[_LaunchThread] = None

    @property
    def is_running(self) -> bool:
        return self._thread is not None and self._thread.isRunning()

    def launch(self, image_path: str, profile: Dict[str, Any]):
        if self.is_running:
            return

        avm_bin = self._find_avm_binary()
        if not avm_bin:
            self.status_changed.emit(False, "avm binary not found. Build the project first.")
            return

        cmd = [
            avm_bin,
            "--android",   str(profile.get("android", 14)),
            "--image",     image_path,
            "--memory",    str(profile.get("memory_mb", 6144)),
            "--cores",     str(profile.get("cores", 4)),
            "--fps",       str(profile.get("target_fps", 60)),
            "--gpu",       profile.get("gpu_backend", "vulkan"),
            "--spoof-profile", profile.get("spoof_profile", "rog9pro"),
        ]
        if profile.get("gapps", True):
            cmd.append("--gapps")
        if profile.get("show_hud", True):
            cmd.append("--hud")

        self._thread = _LaunchThread(cmd)
        self._thread.status.connect(self.status_changed)
        self._thread.start()

    def stop(self):
        if self._thread:
            self._thread.stop()

    @staticmethod
    def _find_avm_binary() -> Optional[str]:
        # Look for the binary relative to repo root first
        repo_root = Path(__file__).parent.parent
        candidates = [
            repo_root / "build" / "avm",
            repo_root / "build" / "avm.exe",
            Path("/usr/local/bin/avm"),
        ]
        for c in candidates:
            if c.exists():
                return str(c)
        # Fall back to PATH
        found = shutil.which("avm")
        return found
