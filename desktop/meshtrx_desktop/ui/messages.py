"""Вкладка сообщений: переписка с абонентами и общий эфир."""

from __future__ import annotations

import time

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QLabel, QLineEdit,
                               QListWidget, QListWidgetItem, QPushButton,
                               QVBoxLayout, QWidget)

from ..client import Client, Message
from . import theme
from .bridge import Bridge


class MessagesTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        layout = QVBoxLayout(self)

        self.list = QListWidget()
        self.list.setWordWrap(True)
        layout.addWidget(self.list, 1)

        row = QHBoxLayout()
        row.addWidget(QLabel("Кому:"))
        self.dest = QComboBox(); self.dest.setMinimumWidth(200)
        row.addWidget(self.dest)
        self.input = QLineEdit(); self.input.setPlaceholderText("Сообщение (до 84 символов)")
        self.input.setMaxLength(84)
        self.input.returnPressed.connect(self.send)
        row.addWidget(self.input, 1)
        btn = QPushButton("Отправить"); btn.setProperty("accent", True)
        btn.clicked.connect(self.send)
        row.addWidget(btn)
        layout.addLayout(row)

        bridge.message.connect(self.on_message)
        bridge.peers_changed.connect(lambda _p: self.refresh_dest())
        self.refresh_dest()

    def refresh_dest(self):
        current = self.dest.currentData()
        self.dest.clear()
        self.dest.addItem("всем", None)
        for peer in sorted(self.client.active_peers(), key=lambda p: -p.rssi):
            label = peer.call_sign or f"TX-{peer.device_id[-4:]}"
            self.dest.addItem(f"{label} [{peer.device_id[-4:]}]", peer.device_id[-4:])
        idx = self.dest.findData(current)
        if idx >= 0:
            self.dest.setCurrentIndex(idx)

    def send(self):
        text = self.input.text().strip()
        if not text:
            return
        self.client.send_message(text, self.dest.currentData())
        self.input.clear()

    def on_message(self, msg: Message):
        # Уже показанное исходящее сообщение могло получить подтверждение —
        # тогда обновляем строку, а не добавляем вторую.
        for row in range(self.list.count()):
            item = self.list.item(row)
            if item.data(Qt.UserRole) is msg:
                item.setText(self._format(msg))
                return
        item = QListWidgetItem(self._format(msg))
        item.setData(Qt.UserRole, msg)
        if msg.outgoing:
            item.setTextAlignment(Qt.AlignRight)
            item.setForeground(Qt.gray if not msg.delivered else Qt.white)
        self.list.addItem(item)
        self.list.scrollToBottom()

    def _format(self, msg: Message) -> str:
        stamp = time.strftime("%H:%M", time.localtime(msg.time))
        if msg.outgoing:
            mark = "✓" if msg.delivered else "…"
            return f"{stamp}  →  {msg.peer_name}: {msg.text}  {mark}"
        rssi = f"  [{msg.rssi} дБм]" if msg.rssi is not None else ""
        return f"{stamp}  ←  {msg.peer_name}: {msg.text}{rssi}"
