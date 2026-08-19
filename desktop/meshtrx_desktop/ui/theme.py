"""Тёмная тактическая тема — та же палитра, что у Android-клиента и сайта."""

BG = "#141414"
BG_CARD = "#1c1c1c"
BORDER = "#2a2a2a"
ACCENT = "#4ade80"
ACCENT_DIM = "#22c55e"
TEXT = "#e5e5e5"
TEXT_DIM = "#8a8a8a"
DANGER = "#ef4444"
WARN = "#f59e0b"

QSS = f"""
QWidget {{
    background: {BG};
    color: {TEXT};
    font-family: "JetBrains Mono", "DejaVu Sans Mono", monospace;
    font-size: 13px;
}}
QTabWidget::pane {{ border: 1px solid {BORDER}; border-radius: 6px; }}
QTabBar::tab {{
    background: {BG_CARD}; color: {TEXT_DIM};
    padding: 8px 18px; margin-right: 2px;
    border: 1px solid {BORDER}; border-bottom: none;
    border-top-left-radius: 6px; border-top-right-radius: 6px;
}}
QTabBar::tab:selected {{ color: {ACCENT}; border-color: {ACCENT}; }}
QPushButton {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 6px; padding: 7px 14px;
}}
QPushButton:hover {{ border-color: {ACCENT}; }}
QPushButton:disabled {{ color: {TEXT_DIM}; border-color: {BORDER}; }}
QPushButton[accent="true"] {{
    background: {ACCENT}; color: {BG}; font-weight: bold; border: none;
}}
QPushButton[danger="true"] {{ background: {DANGER}; color: #fff; border: none; }}
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QComboBox {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 6px; padding: 6px; selection-background-color: {ACCENT_DIM};
}}
QLineEdit:focus, QTextEdit:focus, QComboBox:focus {{ border-color: {ACCENT}; }}
QListWidget, QTableWidget {{
    background: {BG_CARD}; border: 1px solid {BORDER}; border-radius: 6px;
}}
QListWidget::item:selected {{ background: {BORDER}; color: {ACCENT}; }}
QProgressBar {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 6px; text-align: center; height: 18px;
}}
QProgressBar::chunk {{ background: {ACCENT}; border-radius: 5px; }}
QGroupBox {{
    border: 1px solid {BORDER}; border-radius: 8px;
    margin-top: 14px; padding-top: 10px;
}}
QGroupBox::title {{ subcontrol-origin: margin; left: 12px; color: {TEXT_DIM}; }}
QLabel[dim="true"] {{ color: {TEXT_DIM}; }}
QLabel[accent="true"] {{ color: {ACCENT}; }}
"""
