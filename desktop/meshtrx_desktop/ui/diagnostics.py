"""Вкладка диагностики: журнал событий и счётчики — то, чего нет в телефоне."""

from __future__ import annotations

import time
from collections import Counter

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import (QCheckBox, QHBoxLayout, QLabel, QPlainTextEdit,
                               QPushButton, QVBoxLayout, QWidget)

from ..client import Client
from .bridge import Bridge

MAX_LINES = 2000


class DiagnosticsTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        self.counts: Counter[str] = Counter()
        self._start = time.time()

        layout = QVBoxLayout(self)
        row = QHBoxLayout()
        self.summary = QLabel("—")
        row.addWidget(self.summary, 1)
        self.audio_log = QCheckBox("показывать голосовые пакеты")
        row.addWidget(self.audio_log)
        clear = QPushButton("Очистить"); clear.clicked.connect(self.clear)
        row.addWidget(clear)
        save = QPushButton("Сохранить журнал"); save.clicked.connect(self.save)
        row.addWidget(save)
        layout.addLayout(row)

        self.log = QPlainTextEdit(); self.log.setReadOnly(True)
        self.log.setMaximumBlockCount(MAX_LINES)
        layout.addWidget(self.log, 1)

        bridge.any_event.connect(self._on_event)
        QTimer(self, interval=2000, timeout=self.refresh).start()

    def _on_event(self, event: str, payload: object):
        self.counts[event] += 1
        if event in ("mic_level",) or (event == "audio_rx" and not self.audio_log.isChecked()):
            return
        stamp = time.strftime("%H:%M:%S")
        self.log.appendPlainText(f"{stamp}  {event}: {payload}")

    def refresh(self):
        link = self.client.link
        uptime = int(link.uptime)
        parts = [f"связь: {self.client.state}",
                 f"в соединении {uptime // 60} мин {uptime % 60} с" if link.connected else "",
                 f"абонентов {len(self.client.active_peers())}",
                 f"сообщений {self.counts['message']}",
                 f"голосовых пакетов {self.counts['audio_rx']}",
                 f"разрывов {self.counts['state']}"]
        self.summary.setText("   ".join(p for p in parts if p))

    def clear(self):
        self.log.clear()
        self.counts.clear()

    def save(self):
        from PySide6.QtWidgets import QFileDialog
        path, _ = QFileDialog.getSaveFileName(
            self, "Сохранить журнал", f"meshtrx-{time.strftime('%Y%m%d-%H%M%S')}.log")
        if path:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.log.toPlainText())
