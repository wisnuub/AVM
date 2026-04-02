"""Game grid — displays library or Play Store search results."""

import json
from typing import List, Dict

from PyQt6.QtWidgets import (
    QWidget, QScrollArea, QVBoxLayout, QHBoxLayout,
    QLabel, QPushButton, QGridLayout, QFrame,
    QSizePolicy, QSpacerItem,
)
from PyQt6.QtCore import Qt, pyqtSignal, QThread
from PyQt6.QtGui import QFont

from launcher.config import LauncherConfig
from launcher.widgets.game_card import GameCard
from launcher.widgets.game_profile_dialog import GameProfileDialog
from launcher.workers import PlayStoreSearchWorker

# Curated featured games shown in library when empty
FEATURED_GAMES: List[Dict] = [
    {"name": "PUBG Mobile",           "package": "com.tencent.ig",                "icon_emoji": "🔫", "genre": "Battle Royale"},
    {"name": "Call of Duty Mobile",   "package": "com.activision.callofduty.shooter", "icon_emoji": "🎖️", "genre": "FPS"},
    {"name": "Mobile Legends",        "package": "com.mobile.legends",            "icon_emoji": "⚔️", "genre": "MOBA"},
    {"name": "Genshin Impact",         "package": "com.miHoYo.GenshinImpact",      "icon_emoji": "🗺️", "genre": "RPG"},
    {"name": "Honkai: Star Rail",      "package": "com.HoYoverse.hkrpgoversea",   "icon_emoji": "⭐", "genre": "RPG"},
    {"name": "Wuthering Waves",        "package": "com.kurogame.wutheringwaves.global", "icon_emoji": "🌊", "genre": "RPG"},
    {"name": "Free Fire",              "package": "com.dts.freefireth",            "icon_emoji": "🔥", "genre": "Battle Royale"},
    {"name": "Clash of Clans",         "package": "com.supercell.clashofclans",   "icon_emoji": "🏰", "genre": "Strategy"},
]


class GameGrid(QWidget):
    launch_requested = pyqtSignal(str)

    def __init__(self, config: LauncherConfig, search_mode=False):
        super().__init__()
        self.config = config
        self.search_mode = search_mode
        self._search_worker = None
        self._build_ui()

    def _build_ui(self):
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; }")

        self._inner = QWidget()
        self._inner.setStyleSheet("background: transparent;")
        self._grid = QGridLayout(self._inner)
        self._grid.setSpacing(16)
        self._grid.setContentsMargins(0, 0, 0, 0)

        scroll.setWidget(self._inner)
        outer.addWidget(scroll)

        if self.search_mode:
            self._status = QLabel("Search for games above")
            self._status.setStyleSheet("color:#8a8a9a; font-size:13px; margin:20px;")
            outer.insertWidget(0, self._status)

    def refresh(self, games: List[Dict]):
        """Populate with user library + featured cards for empty slots."""
        self._clear_grid()
        installed_packages = {g["package"] for g in games}
        all_cards = list(games)

        # Add featured games not yet in library
        for fg in FEATURED_GAMES:
            if fg["package"] not in installed_packages:
                all_cards.append({**fg, "_featured": True})

        self._populate(all_cards)

    def search(self, query: str):
        """Search Play Store (stub — returns curated matches + ADB install trigger)."""
        self._clear_grid()
        if hasattr(self, "_status"):
            self._status.setText(f'Searching for "{query}" …')

        if self._search_worker and self._search_worker.isRunning():
            self._search_worker.terminate()

        self._search_worker = PlayStoreSearchWorker(query)
        self._search_worker.results.connect(self._on_search_results)
        self._search_worker.start()

    def _on_search_results(self, results: List[Dict]):
        if hasattr(self, "_status"):
            self._status.setText(
                f"{len(results)} results" if results else "No results found"
            )
        self._populate(results)

    def _populate(self, games: List[Dict]):
        self._clear_grid()
        col_count = 6
        for i, game in enumerate(games):
            card = GameCard(game)
            card.launch_requested.connect(self.launch_requested)
            card.profile_requested.connect(self._edit_profile)
            card.remove_requested.connect(self._remove_game)
            self._grid.addWidget(card, i // col_count, i % col_count)

        # Fill remaining cells with spacers for alignment
        remainder = len(games) % col_count
        if remainder:
            for j in range(col_count - remainder):
                spacer = QWidget()
                spacer.setFixedSize(160, 220)
                self._grid.addWidget(spacer, len(games) // col_count, remainder + j)

    def _clear_grid(self):
        while self._grid.count():
            item = self._grid.takeAt(0)
            if item.widget():
                item.widget().deleteLater()

    def _edit_profile(self, package: str):
        dlg = GameProfileDialog(package, self.config, self)
        dlg.exec()

    def _remove_game(self, package: str):
        self.config.remove_game(package)
        self.refresh(self.config.games)
