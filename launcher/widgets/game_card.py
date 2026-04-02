from PyQt6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QFrame, QMenu,
)
from PyQt6.QtCore import Qt, pyqtSignal, QSize
from PyQt6.QtGui import QPixmap, QIcon
from typing import Dict


class GameCard(QFrame):
    launch_requested  = pyqtSignal(str)   # package name
    profile_requested = pyqtSignal(str)   # package name
    remove_requested  = pyqtSignal(str)   # package name

    def __init__(self, game: Dict, parent=None):
        super().__init__(parent)
        self.game = game
        self.package = game.get("package", "")
        self._build_ui()

    def _build_ui(self):
        self.setFixedSize(160, 220)
        self.setStyleSheet("""
            GameCard {
                background: #1a1a2e;
                border: 1px solid #2a2a40;
                border-radius: 10px;
            }
            GameCard:hover {
                border: 1px solid #00d4aa;
                background: #1e2040;
            }
        """)
        self.setCursor(Qt.CursorShape.PointingHandCursor)

        layout = QVBoxLayout(self)
        layout.setContentsMargins(10, 10, 10, 10)
        layout.setSpacing(6)

        # Icon
        icon_label = QLabel()
        icon_label.setFixedSize(100, 100)
        icon_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        icon_label.setStyleSheet(
            "background:#0d0d2a; border-radius:16px; font-size:40px;"
        )
        icon_url = self.game.get("icon_url", "")
        if icon_url:
            # In a real build, load async via QNetworkAccessManager
            icon_label.setText(self.game.get("icon_emoji", "🎮"))
        else:
            icon_label.setText(self.game.get("icon_emoji", "🎮"))
        layout.addWidget(icon_label, alignment=Qt.AlignmentFlag.AlignHCenter)

        # Title
        title = QLabel(self.game.get("name", self.package))
        title.setWordWrap(True)
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        title.setStyleSheet("color:#e0e0e0; font-size:11px; font-weight:600;")
        layout.addWidget(title)

        # Profile badge
        profile = self.game.get("profile", {}).get("spoof_profile", "global")
        badge = QLabel(profile)
        badge.setAlignment(Qt.AlignmentFlag.AlignCenter)
        badge.setStyleSheet(
            "color:#00d4aa; font-size:9px; background:#0d2020; "
            "border-radius:4px; padding:2px 6px;"
        )
        layout.addWidget(badge)

        layout.addStretch()

        # Play button
        play_btn = QPushButton("  Play")
        play_btn.setFixedHeight(30)
        play_btn.setStyleSheet(
            "background:#00d4aa; color:#000; font-weight:bold; "
            "border-radius:6px; font-size:11px;"
        )
        play_btn.clicked.connect(lambda: self.launch_requested.emit(self.package))
        layout.addWidget(play_btn)

    def contextMenuEvent(self, event):
        menu = QMenu(self)
        menu.setStyleSheet(
            "QMenu { background:#1a1a2e; color:#e0e0e0; border:1px solid #2a2a40; }"
            "QMenu::item:selected { background:#00d4aa; color:#000; }"
        )
        play_a    = menu.addAction("▶  Play")
        profile_a = menu.addAction("⚙️  Edit Profile")
        menu.addSeparator()
        remove_a  = menu.addAction("🗑  Remove")

        action = menu.exec(event.globalPos())
        if action == play_a:    self.launch_requested.emit(self.package)
        if action == profile_a: self.profile_requested.emit(self.package)
        if action == remove_a:  self.remove_requested.emit(self.package)
