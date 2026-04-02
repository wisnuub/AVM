#!/usr/bin/env python3
"""
AVM Launcher  —  Main entry point.

Startup flow:
  1. Check first-run (no images, no Google account stored)
     → show FirstRunWizard
  2. Show GameLibrary (main window)
     → user can browse Play Store search, launch installed games
  3. Per-game profile editor or Global profile editor
"""

import sys
import os

# Allow running from repo root: python3 launcher/main.py
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from launcher.app import AVMLauncherApp

if __name__ == "__main__":
    app = AVMLauncherApp(sys.argv)
    sys.exit(app.exec())
