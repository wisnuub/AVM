from PyQt6.QtWidgets import QLineEdit
from PyQt6.QtCore import pyqtSignal, Qt, QTimer


class SearchBar(QLineEdit):
    searched = pyqtSignal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setPlaceholderText("🔍  Search Play Store …")
        self.setStyleSheet("""
            QLineEdit {
                background: #1e1e30;
                color: #e0e0e0;
                border: 1px solid #2a2a40;
                border-radius: 8px;
                padding: 6px 12px;
                font-size: 13px;
            }
            QLineEdit:focus {
                border: 1px solid #00d4aa;
            }
        """)
        self._timer = QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(400)  # 400ms debounce
        self._timer.timeout.connect(self._emit)
        self.textChanged.connect(lambda _: self._timer.start())
        self.returnPressed.connect(self._emit)

    def _emit(self):
        text = self.text().strip()
        if len(text) >= 2:
            self.searched.emit(text)
