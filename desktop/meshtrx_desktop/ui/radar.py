"""Радар: абоненты по уровню сигнала. Ближе к центру — сильнее сигнал."""

from __future__ import annotations

import math
import time

from PySide6.QtCore import QPointF, Qt, QTimer
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QHBoxLayout, QPushButton, QVBoxLayout, QWidget

from ..client import Client
from . import theme
from .bridge import Bridge

RSSI_NEAR = -40      # сильнее — считаем «рядом»
RSSI_FAR = -120      # слабее — на краю экрана


class RadarView(QWidget):
    def __init__(self, client: Client):
        super().__init__()
        self.client = client
        self.setMinimumSize(420, 420)

    def paintEvent(self, _event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        w, h = self.width(), self.height()
        cx, cy = w / 2, h / 2
        radius = min(w, h) / 2 - 30

        p.fillRect(self.rect(), QColor(theme.BG))

        # Кольца дальности с подписями уровня — без них расстояние на радаре
        # ничего не значит.
        p.setPen(QPen(QColor(theme.BORDER), 1))
        for i in range(1, 5):
            r = radius * i / 4
            p.drawEllipse(QPointF(cx, cy), r, r)
            rssi = RSSI_NEAR + (RSSI_FAR - RSSI_NEAR) * (i / 4)
            # Подписи колец — вверх по вертикали: там их не перекрывают метки
            # абонентов, которые расходятся по кругу.
            p.setPen(QPen(QColor(theme.TEXT_DIM), 1))
            p.drawText(QPointF(cx + 4, cy - r + 14), f"{int(rssi)} дБм")
            p.setPen(QPen(QColor(theme.BORDER), 1))
        p.drawLine(QPointF(cx - radius, cy), QPointF(cx + radius, cy))
        p.drawLine(QPointF(cx, cy - radius), QPointF(cx, cy + radius))

        # Мы в центре
        p.setBrush(QColor(theme.ACCENT))
        p.setPen(Qt.NoPen)
        p.drawEllipse(QPointF(cx, cy), 6, 6)
        p.setPen(QPen(QColor(theme.TEXT_DIM)))
        # Подпись своей точки — вниз: вправо уходят метки абонентов
        p.drawText(QPointF(cx - 5, cy + 20), "я")

        peers = self.client.active_peers()
        if not peers:
            p.setPen(QPen(QColor(theme.TEXT_DIM)))
            p.drawText(self.rect(), Qt.AlignBottom | Qt.AlignHCenter,
                       "никого не слышно")
            return

        # Направления на абонентов радио не даёт, поэтому раскладываем их по
        # кругу равномерно и в стабильном порядке: так метки не наезжают друг
        # на друга и не прыгают от кадра к кадру.
        ordered = sorted(peers, key=lambda p_: p_.device_id)
        for index, peer in enumerate(ordered):
            frac = (max(RSSI_FAR, min(RSSI_NEAR, peer.rssi)) - RSSI_NEAR) / (RSSI_FAR - RSSI_NEAR)
            r = radius * max(0.12, frac)
            angle = (2 * math.pi * index / len(ordered)) - math.pi / 2
            x, y = cx + r * math.cos(angle), cy + r * math.sin(angle)

            age = time.time() - self.client.peer_seen.get(peer.device_id, 0)
            color = QColor(theme.ACCENT if age < 120 else theme.WARN)
            p.setBrush(color); p.setPen(Qt.NoPen)
            p.drawEllipse(QPointF(x, y), 7, 7)

            p.setPen(QPen(QColor(theme.TEXT)))
            p.setFont(QFont(self.font().family(), 9))
            label = peer.call_sign or f"TX-{peer.device_id[-4:]}"
            text = f"{label}  {peer.rssi}"
            # У правого края подпись уводим влево, иначе она уезжает за экран
            tx = x + 11 if x < w - 110 else x - 11 - len(text) * 7
            p.drawText(QPointF(tx, y + 3), text)


class RadarTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        layout = QVBoxLayout(self)
        row = QHBoxLayout()
        refresh = QPushButton("Обновить список")
        refresh.clicked.connect(client.scan_peers)
        row.addWidget(refresh); row.addStretch()
        layout.addLayout(row)
        self.view = RadarView(client)
        layout.addWidget(self.view)
        bridge.peers_changed.connect(lambda _p: self.view.update())
        QTimer(self, interval=3000, timeout=self.view.update).start()
