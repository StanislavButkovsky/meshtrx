"""Оформление — то же, что у телефона с Android 4.4.31 и iOS 1.0.4.

Основное — тёмно-синее с жёлтым акцентом, дополнительное — «Хаки». Цвета
заданы ролями: виджеты берут их отсюда через QSS, а переключение темы просто
пересобирает таблицу стилей.
"""

from __future__ import annotations

PALETTES = {
    "main": dict(
        BG="#0E1A33", BG_DEEP="#0B152B", BG_CARD="#14233F", ELEVATED="#1B2C4E",
        BORDER="#22355A", BORDER_STRONG="#2A3F66",
        ACCENT="#FABE14", ON_ACCENT="#101E3C",
        SECONDARY="#5B9BFF", SECONDARY_BG="#1D3566",
        TEXT="#EEF2FA", TEXT_DIM="#8E9AB3", TEXT_FAINT="#7F8FB0"),
    "khaki": dict(
        BG="#1F2318", BG_DEEP="#191C13", BG_CARD="#2A2F20", ELEVATED="#343A28",
        BORDER="#3E4530", BORDER_STRONG="#4A523A",
        ACCENT="#CE9C3A", ON_ACCENT="#1F2318",
        SECONDARY="#A3B56A", SECONDARY_BG="#3A4425",
        TEXT="#EAE2C8", TEXT_DIM="#A39C82", TEXT_FAINT="#8A846C"),
}
THEME_NAMES = {"main": "Основное (сине-жёлтое)", "khaki": "Хаки"}

DANGER = "#ef4444"
WARN = "#f59e0b"
OK = "#5CCB8A"

# Текущие значения ролей — модули интерфейса читают их как theme.ACCENT и т. п.
BG = BG_DEEP = BG_CARD = ELEVATED = BORDER = BORDER_STRONG = ""
ACCENT = ON_ACCENT = SECONDARY = SECONDARY_BG = TEXT = TEXT_DIM = TEXT_FAINT = ""
ACCENT_DIM = ""
current = "main"


def _shade(hex_color: str, k: float) -> str:
    h = hex_color.lstrip("#")
    r, g, b = (min(255, int(int(h[i:i + 2], 16) * k)) for i in (0, 2, 4))
    return f"#{r:02X}{g:02X}{b:02X}"


def use(name: str) -> str:
    """Выбрать тему и вернуть таблицу стилей для приложения."""
    global current, ACCENT_DIM
    current = name if name in PALETTES else "main"
    globals().update(PALETTES[current])
    ACCENT_DIM = _shade(ACCENT, 0.8)
    return qss()


def ptt_style(color: str, text: str) -> str:
    """Кнопка «Говорить» — объёмная, как в телефоне: светлее сверху, темнее
    снизу, с бликом. В Qt это делается градиентом фона."""
    return (f"QPushButton {{ color: {text}; font-weight: bold; font-size: 18px;"
            f" border: 3px solid {_shade(color, 0.7)}; border-radius: 16px;"
            f" background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
            f" stop:0 {_shade(color, 1.18)}, stop:0.45 {color}, stop:1 {_shade(color, 0.72)}); }}"
            f" QPushButton:pressed {{ background: {_shade(color, 0.8)}; }}")


def qss() -> str:
    return f"""
QWidget {{
    background: {BG};
    color: {TEXT};
    font-size: 13px;
}}
QTabWidget::pane {{ border: 1px solid {BORDER}; border-radius: 14px; }}
QTabBar::tab {{
    background: {BG_CARD}; color: {TEXT_FAINT};
    padding: 9px 18px; margin-right: 3px;
    border: 1px solid {BORDER}; border-bottom: none;
    border-top-left-radius: 12px; border-top-right-radius: 12px;
}}
QTabBar::tab:selected {{ color: {ACCENT}; border-color: {ACCENT}; }}
QPushButton {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 14px; padding: 8px 16px;
}}
QPushButton:hover {{ border-color: {ACCENT}; }}
QPushButton:disabled {{ color: {TEXT_FAINT}; border-color: {BORDER}; }}
QPushButton[accent="true"] {{
    background: {ACCENT}; color: {ON_ACCENT}; font-weight: bold; border: none;
}}
QPushButton[accent="true"]:hover {{ background: {ACCENT_DIM}; }}
QPushButton[danger="true"] {{ background: {DANGER}; color: #fff; border: none; }}
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QComboBox {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 12px; padding: 7px; selection-background-color: {SECONDARY_BG};
}}
QLineEdit:focus, QTextEdit:focus, QComboBox:focus {{ border-color: {ACCENT}; }}
QListWidget, QTableWidget {{
    background: {BG_CARD}; border: 1px solid {BORDER}; border-radius: 14px;
}}
QListWidget::item {{ padding: 4px; }}
QListWidget::item:selected {{ background: {ELEVATED}; color: {ACCENT}; }}
QProgressBar {{
    background: {BG_CARD}; border: 1px solid {BORDER};
    border-radius: 9px; text-align: center; height: 18px;
}}
QProgressBar::chunk {{ background: {ACCENT}; border-radius: 8px; }}
QGroupBox {{
    border: 1px solid {BORDER}; border-radius: 14px;
    margin-top: 14px; padding-top: 10px;
}}
QGroupBox::title {{ subcontrol-origin: margin; left: 14px; color: {TEXT_DIM}; }}
QCheckBox::indicator:checked {{ background: {ACCENT}; border-radius: 4px; }}
QLabel[dim="true"] {{ color: {TEXT_DIM}; }}
QLabel[accent="true"] {{ color: {ACCENT}; }}
QLabel[secondary="true"] {{ color: {SECONDARY}; }}
QStatusBar {{ color: {TEXT_DIM}; }}
"""


use("main")
QSS = qss()      # прежнее имя: таблица стилей основной темы
