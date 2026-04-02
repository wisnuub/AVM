"""Main launcher window: sidebar + game library + launch bar."""

from PyQt6.QtWidgets import (
    QMainWindow, QWidget, QHBoxLayout, QVBoxLayout,
    QLabel, QPushButton, QLineEdit, QScrollArea,
    QFrame, QSizePolicy, QMessageBox, QStackedWidget,
    QStatusBar,
)
from PyQt6.QtCore import Qt, QSize, pyqtSignal, QTimer
from PyQt6.QtGui import QIcon, QPixmap, QFont

from launcher.config import LauncherConfig
from launcher.widgets.game_card import GameCard
from launcher.widgets.sidebar import Sidebar
from launcher.widgets.game_grid import GameGrid
from launcher.widgets.search_bar import SearchBar
from launcher.widgets.profile_editor import GlobalProfileEditor
from launcher.vm_controller import VMController


class MainWindow(QMainWindow):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.vm = VMController(config)
        self.setWindowTitle("AVM")
        self.setMinimumSize(1100, 700)
        self.resize(1280, 800)

        self._build_ui()
        self._connect_signals()
        self._refresh_library()

    # ------------------------------------------------------------------
    def _build_ui(self):
        central = QWidget()
        self.setCentralWidget(central)
        root = QHBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        # Sidebar
        self.sidebar = Sidebar(self.config)
        root.addWidget(self.sidebar)

        # Main content area
        content = QWidget()
        content.setStyleSheet("background: #12122a;")
        content_layout = QVBoxLayout(content)
        content_layout.setContentsMargins(24, 20, 24, 16)
        content_layout.setSpacing(12)

        # Top bar: title + search + settings
        top_bar = QHBoxLayout()
        self.page_title = QLabel("Game Library")
        self.page_title.setStyleSheet("font-size:20px; font-weight:700; color:#e0e0e0;")
        top_bar.addWidget(self.page_title)
        top_bar.addStretch()

        self.search_bar = SearchBar()
        self.search_bar.setFixedWidth(300)
        top_bar.addWidget(self.search_bar)

        self.global_profile_btn = QPushButton("  Global Profile")
        self.global_profile_btn.setFixedHeight(36)
        self.global_profile_btn.setStyleSheet(
            "background:#1e2040; color:#00d4aa; border:1px solid #00d4aa; "
            "border-radius:6px; padding:0 14px; font-size:12px;"
        )
        top_bar.addWidget(self.global_profile_btn)
        content_layout.addLayout(top_bar)

        # Status badge (VM running / stopped)
        self.vm_status = QLabel("  VM: Stopped")
        self.vm_status.setStyleSheet(
            "color:#8a8a9a; font-size:11px; padding:4px 10px; "
            "background:#1e1e30; border-radius:4px;"
        )
        content_layout.addWidget(self.vm_status, alignment=Qt.AlignmentFlag.AlignLeft)

        # Stacked: library  /  search results  /  global profile editor
        self.stack = QStackedWidget()

        # Page 0: library grid
        self.game_grid = GameGrid(self.config)
        self.stack.addWidget(self.game_grid)

        # Page 1: search results (reuses GameGrid with filtered data)
        self.search_results = GameGrid(self.config, search_mode=True)
        self.stack.addWidget(self.search_results)

        # Page 2: global profile editor
        self.profile_editor = GlobalProfileEditor(self.config)
        self.stack.addWidget(self.profile_editor)

        content_layout.addWidget(self.stack)
        root.addWidget(content, stretch=1)

        # Status bar
        sb = QStatusBar()
        sb.setStyleSheet("background:#0d0d1a; color:#8a8a9a; font-size:11px;")
        self.setStatusBar(sb)
        self.statusBar().showMessage(
            f"Profile: {self.config.global_cfg.get('spoof_profile','pixel8pro')}  │  "
            f"RAM: {self.config.global_cfg.get('memory_mb',6144)//1024} GB  │  "
            f"FPS cap: {self.config.global_cfg.get('target_fps',60)}"
        )

    def _connect_signals(self):
        self.sidebar.nav_library.clicked.connect(self._show_library)
        self.sidebar.nav_search.clicked.connect(self._show_search)
        self.sidebar.nav_settings.clicked.connect(self._show_profile)
        self.global_profile_btn.clicked.connect(self._show_profile)
        self.search_bar.searched.connect(self._on_search)
        self.game_grid.launch_requested.connect(self._launch_game)
        self.search_results.launch_requested.connect(self._launch_game)
        self.profile_editor.saved.connect(self._on_profile_saved)
        self.vm.status_changed.connect(self._on_vm_status)

    # ------------------------------------------------------------------
    def _show_library(self):
        self.page_title.setText("Game Library")
        self.stack.setCurrentIndex(0)

    def _show_search(self):
        self.page_title.setText("Search Play Store")
        self.stack.setCurrentIndex(1)
        self.search_bar.setFocus()

    def _show_profile(self):
        self.page_title.setText("Global Profile")
        self.profile_editor.load()
        self.stack.setCurrentIndex(2)

    def _refresh_library(self):
        self.game_grid.refresh(self.config.games)

    def _on_search(self, query: str):
        self.stack.setCurrentIndex(1)
        self.search_results.search(query)

    def _launch_game(self, package: str):
        profile = self.config.load_profile(package)
        image   = self.config.get_default_image()
        if not image:
            QMessageBox.warning(self, "No image", "No Android image found. Run avm-image-pull first.")
            return
        self.vm.launch(str(image), profile)

    def _on_vm_status(self, running: bool, msg: str):
        if running:
            self.vm_status.setText(f"  VM: Running  —  {msg}")
            self.vm_status.setStyleSheet(
                "color:#00d4aa; font-size:11px; padding:4px 10px; "
                "background:#0d2020; border-radius:4px;"
            )
        else:
            self.vm_status.setText("  VM: Stopped")
            self.vm_status.setStyleSheet(
                "color:#8a8a9a; font-size:11px; padding:4px 10px; "
                "background:#1e1e30; border-radius:4px;"
            )

    def _on_profile_saved(self):
        self.statusBar().showMessage(
            f"Profile: {self.config.global_cfg.get('spoof_profile','pixel8pro')}  │  "
            f"RAM: {self.config.global_cfg.get('memory_mb',6144)//1024} GB  │  "
            f"FPS cap: {self.config.global_cfg.get('target_fps',60)}"
        )
        self._show_library()
