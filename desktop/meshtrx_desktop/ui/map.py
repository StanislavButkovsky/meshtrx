"""Карта: абоненты по координатам из маяков.

Тайлов не грузим — клиент должен работать там, где интернета нет по условию
задачи. Рисуем простую проекцию: свои координаты в центре, остальные вокруг
с масштабом в метрах.
"""

from __future__ import annotations

import math

from PySide6.QtCore import QPointF, Qt, QTimer
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import (QDoubleSpinBox, QHBoxLayout, QLabel, QPushButton,
                               QVBoxLayout, QWidget)

from ..client import Client
from . import theme
from .bridge import Bridge


class MapView(QWidget):
    def __init__(self, client: Client):
        super().__init__()
        self.client = client
        self.setMinimumSize(420, 420)
        self.scale_m = 2000.0        # половина стороны экрана в метрах

    def paintEvent(self, _event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        p.fillRect(self.rect(), QColor(theme.BG))
        w, h = self.width(), self.height()
        cx, cy = w / 2, h / 2
        half = min(w, h) / 2 - 20

        p.setPen(QPen(QColor(theme.BORDER), 1))
        for i in range(1, 5):
            r = half * i / 4
            p.drawEllipse(QPointF(cx, cy), r, r)
            p.setPen(QPen(QColor(theme.TEXT_DIM)))
            p.drawText(QPointF(cx + r - 46, cy - 4), f"{self.scale_m * i / 4:.0f} м")
            p.setPen(QPen(QColor(theme.BORDER), 1))

        my_lat = self.client.settings.get("my_lat")
        my_lon = self.client.settings.get("my_lon")
        located = [p_ for p_ in self.client.active_peers() if p_.lat and p_.lon]

        if my_lat is None or my_lon is None:
            if located:
                my_lat = sum(x.lat for x in located) / len(located)
                my_lon = sum(x.lon for x in located) / len(located)
            else:
                p.setPen(QPen(QColor(theme.TEXT_DIM)))
                p.drawText(self.rect(), Qt.AlignCenter,
                           "координат нет: задайте свои в настройках\n"
                           "или дождитесь маяка с GPS")
                return
        else:
            p.setBrush(QColor(theme.ACCENT)); p.setPen(Qt.NoPen)
            p.drawEllipse(QPointF(cx, cy), 6, 6)
            p.setPen(QPen(QColor(theme.TEXT_DIM)))
            p.drawText(QPointF(cx + 10, cy + 4), "я")

        for peer in located:
            dx = (peer.lon - my_lon) * 111320 * math.cos(math.radians(my_lat))
            dy = (peer.lat - my_lat) * 110540
            x = cx + half * dx / self.scale_m
            y = cy - half * dy / self.scale_m
            dist = math.hypot(dx, dy)
            p.setBrush(QColor(theme.ACCENT if dist <= self.scale_m else theme.WARN))
            p.setPen(Qt.NoPen)
            p.drawEllipse(QPointF(max(8, min(w - 8, x)), max(8, min(h - 8, y))), 7, 7)
            p.setPen(QPen(QColor(theme.TEXT)))
            p.setFont(QFont(self.font().family(), 9))
            label = peer.call_sign or f"TX-{peer.device_id[-4:]}"
            p.drawText(QPointF(min(w - 90, x + 11), max(12, min(h - 4, y + 3))),
                       f"{label}  {dist / 1000:.2f} км")


class MapTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        layout = QVBoxLayout(self)

        row = QHBoxLayout()
        row.addWidget(QLabel("Мои координаты:"))
        self.lat = QDoubleSpinBox(); self.lat.setRange(-90, 90); self.lat.setDecimals(6)
        self.lon = QDoubleSpinBox(); self.lon.setRange(-180, 180); self.lon.setDecimals(6)
        row.addWidget(self.lat); row.addWidget(self.lon)
        save = QPushButton("Задать"); save.clicked.connect(self.save_coords)
        row.addWidget(save)
        row.addWidget(QLabel("Масштаб:"))
        for label, meters in (("1 км", 1000.0), ("2 км", 2000.0), ("10 км", 10000.0)):
            btn = QPushButton(label)
            btn.clicked.connect(lambda _c=False, m=meters: self.set_scale(m))
            row.addWidget(btn)
        row.addStretch()
        layout.addLayout(row)

        self.view = MapView(client)
        layout.addWidget(self.view, 1)
        bridge.peers_changed.connect(lambda _p: self.view.update())
        QTimer(self, interval=5000, timeout=self.view.update).start()

    def save_coords(self):
        self.client.settings["my_lat"] = self.lat.value()
        self.client.settings["my_lon"] = self.lon.value()
        self.view.update()

    def set_scale(self, meters: float):
        self.view.scale_m = meters
        self.view.update()
