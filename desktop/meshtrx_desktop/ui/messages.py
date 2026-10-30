"""Вкладка сообщений: переписка с абонентами и общий эфир, текст и голосовые."""

from __future__ import annotations

import time

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QColor
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QLabel, QLineEdit,
                               QListWidget, QListWidgetItem, QPushButton,
                               QVBoxLayout, QWidget)

from ..client import PTT_MAX_SECONDS, Client, Message
from . import theme
from .bridge import Bridge


class RecordButton(QPushButton):
    """Голосовое записывается, пока кнопку держат, — как в телефоне."""

    def __init__(self, tab: "MessagesTab"):
        super().__init__("🎤 Удерживать")
        self.tab = tab
        self.pressed.connect(tab.start_record)
        self.released.connect(tab.stop_record)


class MessagesTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        layout = QVBoxLayout(self)

        self.list = QListWidget()
        self.list.setWordWrap(True)
        self.list.itemDoubleClicked.connect(self.play)
        layout.addWidget(self.list, 1)

        self.hint = QLabel("Голосовое — удерживайте кнопку с микрофоном (до "
                           f"{PTT_MAX_SECONDS} с). Двойной щелчок по голосовому — прослушать.")
        self.hint.setProperty("dim", True); self.hint.setWordWrap(True)
        layout.addWidget(self.hint)

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
        self.record = RecordButton(self)
        row.addWidget(self.record)
        layout.addLayout(row)

        self._rec_timer = QTimer(self)
        self._rec_timer.timeout.connect(self._tick_record)

        bridge.message.connect(self.on_message)
        bridge.peers_changed.connect(lambda _p: self.refresh_dest())
        bridge.record_limit.connect(lambda _s: self.stop_record())
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

    # --- голосовые ---
    def start_record(self):
        if not self.client.link.connected:
            return
        self.client.start_voice_record()
        self._rec_timer.start(250)
        self._tick_record()

    def stop_record(self):
        if not self.client.recording:
            return
        self._rec_timer.stop()
        self.record.setText("🎤 Удерживать")
        data = self.client.stop_voice_record()
        if self.client.send_voice(data, self.dest.currentData()) is None:
            self.hint.setText("Слишком коротко — держите кнопку дольше секунды.")

    def _tick_record(self):
        left = max(0, int(PTT_MAX_SECONDS - (time.time() - self.client._rec_started)))
        self.record.setText(f"● запись, ещё {left} с")

    def play(self, item: QListWidgetItem):
        msg = item.data(Qt.UserRole)
        if msg is not None and msg.voice:
            self.client.play_voice(msg.voice)

    # --- список ---
    def on_message(self, msg: Message):
        # Уже показанное сообщение могло получить подтверждение — тогда
        # обновляем строку, а не добавляем вторую.
        for row in range(self.list.count()):
            item = self.list.item(row)
            if item.data(Qt.UserRole) is msg:
                self._render(item, msg)
                return
        item = QListWidgetItem()
        item.setData(Qt.UserRole, msg)
        self._render(item, msg)
        self.list.addItem(item)
        self.list.scrollToBottom()

    def _render(self, item: QListWidgetItem, msg: Message):
        item.setText(self._format(msg))
        if msg.outgoing:
            item.setTextAlignment(Qt.AlignRight)
            color = (theme.DANGER if msg.failed else
                     theme.TEXT if (msg.delivered or msg.stored) else theme.TEXT_DIM)
            item.setForeground(QColor(color))
        elif msg.voice:
            item.setForeground(QColor(theme.SECONDARY))

    def _format(self, msg: Message) -> str:
        stamp = time.strftime("%H:%M", time.localtime(msg.time))
        body = f"🎤 голосовое, {msg.voice_seconds} с  ▶" if msg.voice else msg.text
        if msg.outgoing:
            if msg.failed:
                mark = "✗ не доставлено"
            elif msg.delivered:
                mark = "✓ доставлено"
            elif msg.stored:
                # Текст — ждёт на рации адресата; голосовое всем — разослано
                mark = "✓ разослано" if msg.voice else "✓ принято нодой"
            else:
                mark = "…"
            return f"{stamp}  →  {msg.peer_name}: {body}  {mark}"
        rssi = f"  [{msg.rssi} дБм]" if msg.rssi is not None else ""
        # Открытые сообщения при заданном ключе помечаем словами: их мог
        # прочитать кто угодно на канале
        open_mark = ("  [открыто]" if msg.encrypted is False and self.client.key_fingerprint
                     else "")
        return f"{stamp}  ←  {msg.peer_name}: {body}{rssi}{open_mark}"
