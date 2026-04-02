import sys
import os
from pathlib import Path

try:
    from PyQt6.QtWidgets import QApplication
    from PyQt6.QtCore import Qt
except ImportError:
    print("PyQt6 not found. Install with: pip install PyQt6")
    sys.exit(1)

from launcher.config import LauncherConfig
from launcher.windows.main_window import MainWindow
from launcher.windows.first_run import FirstRunWizard


class AVMLauncherApp(QApplication):
    def __init__(self, argv):
        super().__init__(argv)
        self.setApplicationName("AVM")
        self.setApplicationDisplayName("AVM — Android Virtual Machine")
        self.setApplicationVersion("0.7.0")
        self.setOrganizationName("wisnuub")

        self.config = LauncherConfig()

        self._apply_dark_theme()

        if self.config.needs_first_run():
            self.wizard = FirstRunWizard(self.config)
            self.wizard.finished.connect(self._on_wizard_done)
            self.wizard.show()
        else:
            self._open_main()

    def _on_wizard_done(self, result):
        if result == 1:  # accepted
            self._open_main()
        else:
            self.quit()

    def _open_main(self):
        self.main_window = MainWindow(self.config)
        self.main_window.show()

    def _apply_dark_theme(self):
        self.setStyle("Fusion")
        from PyQt6.QtGui import QPalette, QColor
        palette = QPalette()
        bg      = QColor("#1a1a2e")
        surface = QColor("#16213e")
        accent  = QColor("#00d4aa")
        text    = QColor("#e0e0e0")
        muted   = QColor("#8a8a9a")
        palette.setColor(QPalette.ColorRole.Window,          bg)
        palette.setColor(QPalette.ColorRole.WindowText,      text)
        palette.setColor(QPalette.ColorRole.Base,            surface)
        palette.setColor(QPalette.ColorRole.AlternateBase,   bg)
        palette.setColor(QPalette.ColorRole.Text,            text)
        palette.setColor(QPalette.ColorRole.Button,          surface)
        palette.setColor(QPalette.ColorRole.ButtonText,      text)
        palette.setColor(QPalette.ColorRole.Highlight,       accent)
        palette.setColor(QPalette.ColorRole.HighlightedText, QColor("#000000"))
        palette.setColor(QPalette.ColorRole.PlaceholderText, muted)
        palette.setColor(QPalette.ColorRole.ToolTipBase,     surface)
        palette.setColor(QPalette.ColorRole.ToolTipText,     text)
        self.setPalette(palette)
