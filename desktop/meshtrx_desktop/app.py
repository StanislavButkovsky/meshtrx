"""Точка входа настольного клиента MeshTRX."""

from __future__ import annotations

import os
import sys

from PySide6.QtGui import QIcon
from PySide6.QtWidgets import QApplication, QMessageBox

from .client import Client
from .codec2 import Codec2Error
from .config import Config
from .ui import theme
from .ui.main_window import MainWindow


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("MeshTRX")
    app.setWindowIcon(QIcon(os.path.join(os.path.dirname(__file__), "assets", "icon.png")))
    config = Config()
    app.setStyleSheet(theme.use(config.get("ui_theme", "main")))

    client = Client()
    try:
        client.start()
    except Codec2Error as e:
        QMessageBox.critical(None, "Codec2", str(e))
        return 2

    window = MainWindow(client, config)
    window.show()
    code = app.exec()
    client.stop()
    return code


if __name__ == "__main__":
    sys.exit(main())
