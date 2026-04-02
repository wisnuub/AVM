from PyQt6.QtWidgets import QWidget, QVBoxLayout, QPushButton, QLabel, QSpacerItem, QSizePolicy
from PyQt6.QtCore import Qt, QSize
from launcher.config import LauncherConfig


class Sidebar(QWidget):
    def __init__(self, config: LauncherConfig):
        super().__init__()
        self.config = config
        self.setFixedWidth(200)
        self.setStyleSheet("background: #0d0d1a; border-right: 1px solid #1e1e30;")

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # Logo
        logo = QLabel("AVM")
        logo.setStyleSheet(
            "font-size:22px; font-weight:900; color:#00d4aa; "
            "padding:24px 20px 16px 20px; letter-spacing:4px;"
        )
        layout.addWidget(logo)

        # Nav buttons
        def nav_btn(label: str, icon: str) -> QPushButton:
            btn = QPushButton(f"  {icon}  {label}")
            btn.setFixedHeight(44)
            btn.setCheckable(True)
            btn.setStyleSheet("""
                QPushButton {
                    text-align: left;
                    border: none;
                    color: #8a8a9a;
                    font-size: 13px;
                    padding-left: 20px;
                    background: transparent;
                }
                QPushButton:hover  { background: #1a1a2e; color: #e0e0e0; }
                QPushButton:checked { background: #1a2a2a; color: #00d4aa;
                                      border-left: 3px solid #00d4aa; }
            """)
            return btn

        self.nav_library  = nav_btn("Library",  "🎮")
        self.nav_search   = nav_btn("Search",   "🔍")
        self.nav_settings = nav_btn("Settings", "⚙️")
        self.nav_library.setChecked(True)

        for btn in (self.nav_library, self.nav_search, self.nav_settings):
            layout.addWidget(btn)
            btn.clicked.connect(lambda _, b=btn: self._select(b))

        layout.addSpacerItem(QSpacerItem(0, 0, QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Expanding))

        # Account info at bottom
        from launcher.config import ACCOUNT_FILE
        import json
        email = ""
        if ACCOUNT_FILE.exists():
            try:
                email = json.load(open(ACCOUNT_FILE)).get("google_email", "")
            except Exception:
                pass
        acct = QLabel(f"👤  {email or 'Not signed in'}")
        acct.setWordWrap(True)
        acct.setStyleSheet("color:#8a8a9a; font-size:10px; padding:12px 16px;")
        layout.addWidget(acct)

        self._btns = [self.nav_library, self.nav_search, self.nav_settings]

    def _select(self, active):
        for btn in self._btns:
            btn.setChecked(btn is active)
