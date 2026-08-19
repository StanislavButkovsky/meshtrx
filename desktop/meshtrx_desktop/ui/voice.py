"""Вкладка голоса: кнопка передачи, абоненты в эфире, вызовы."""

from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QKeyEvent
from PySide6.QtWidgets import (QComboBox, QGroupBox, QHBoxLayout, QLabel,
                               QListWidget, QListWidgetItem, QProgressBar,
                               QPushButton, QVBoxLayout, QWidget)

from ..client import Client
from . import theme
from .bridge import Bridge


class PttButton(QPushButton):
    """Передача идёт, пока кнопка нажата — мышью или пробелом.

    Рация не терпит переключателей: отпустил — замолчал. Поэтому здесь
    именно удержание, а не тумблер.
    """

    def __init__(self, client: Client):
        super().__init__("ГОВОРИТЬ  (пробел)")
        self.client = client
        self.setMinimumHeight(90)
        self.setProperty("accent", True)
        self.setFocusPolicy(Qt.StrongFocus)
        self.pressed.connect(self._down)
        self.released.connect(self._up)

    def _down(self):
        self.client.start_ptt()

    def _up(self):
        self.client.stop_ptt()

    def keyPressEvent(self, event: QKeyEvent):
        if event.key() == Qt.Key_Space and not event.isAutoRepeat():
            self.setDown(True); self._down(); return
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event: QKeyEvent):
        if event.key() == Qt.Key_Space and not event.isAutoRepeat():
            self.setDown(False); self._up(); return
        super().keyReleaseEvent(event)


class VoiceTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        layout = QVBoxLayout(self)

        # --- передача ---
        self.ptt = PttButton(client)
        layout.addWidget(self.ptt)

        level_row = QHBoxLayout()
        level_row.addWidget(QLabel("Микрофон"))
        self.level = QProgressBar(); self.level.setRange(0, 100); self.level.setTextVisible(False)
        level_row.addWidget(self.level)
        self.rx_label = QLabel("—"); self.rx_label.setProperty("dim", True)
        level_row.addWidget(self.rx_label)
        layout.addLayout(level_row)

        # --- вызовы ---
        calls = QGroupBox("Вызовы")
        call_row = QHBoxLayout(calls)
        btn_all = QPushButton("Вызов всем"); btn_all.clicked.connect(lambda: client.call("all"))
        btn_priv = QPushButton("Вызов абоненту"); btn_priv.clicked.connect(self._call_private)
        btn_sos = QPushButton("SOS"); btn_sos.setProperty("danger", True)
        btn_sos.clicked.connect(lambda: client.call("sos"))
        for b in (btn_all, btn_priv, btn_sos):
            call_row.addWidget(b)
        layout.addWidget(calls)

        self.call_banner = QLabel(""); self.call_banner.setVisible(False)
        self.call_banner.setProperty("accent", True)
        layout.addWidget(self.call_banner)
        self.call_actions = QWidget(); actions = QHBoxLayout(self.call_actions)
        self.btn_accept = QPushButton("Принять"); self.btn_accept.setProperty("accent", True)
        self.btn_reject = QPushButton("Отклонить"); self.btn_reject.setProperty("danger", True)
        self.btn_accept.clicked.connect(client.accept_call)
        self.btn_reject.clicked.connect(client.reject_call)
        actions.addWidget(self.btn_accept); actions.addWidget(self.btn_reject)
        self.call_actions.setVisible(False)
        layout.addWidget(self.call_actions)

        # --- абоненты ---
        peers_box = QGroupBox("В эфире")
        peers_layout = QVBoxLayout(peers_box)
        mode_row = QHBoxLayout()
        mode_row.addWidget(QLabel("Слушать:"))
        self.listen = QComboBox(); self.listen.addItems(["всех", "только вызвавшего"])
        self.listen.currentIndexChanged.connect(
            lambda i: setattr(client, "listen_all", i == 0))
        mode_row.addWidget(self.listen); mode_row.addStretch()
        peers_layout.addLayout(mode_row)
        self.peers = QListWidget()
        peers_layout.addWidget(self.peers)
        layout.addWidget(peers_box, 1)

        bridge.mic_level.connect(lambda v: self.level.setValue(int(v * 100)))
        bridge.peers_changed.connect(lambda _p: self.refresh_peers())
        bridge.call_changed.connect(self.on_call)
        bridge.audio_rx.connect(self.on_audio)
        bridge.ptt_changed.connect(self.on_ptt)

        self._rx_timer = QTimer(self); self._rx_timer.setSingleShot(True)
        self._rx_timer.timeout.connect(lambda: self.rx_label.setText("—"))
        QTimer(self, interval=5000, timeout=self.refresh_peers).start()

    # --- обновления ---
    def refresh_peers(self):
        current = self.peers.currentItem().data(Qt.UserRole) if self.peers.currentItem() else None
        self.peers.clear()
        for peer in sorted(self.client.active_peers(), key=lambda p: -p.rssi):
            bat = f", {peer.battery}%" if peer.battery is not None else ""
            item = QListWidgetItem(f"{peer.call_sign or 'TX-' + peer.device_id[-4:]}"
                                   f"   {peer.rssi} дБм / SNR {peer.snr}{bat}"
                                   f"   [{peer.device_id}]")
            item.setData(Qt.UserRole, peer.device_id)
            self.peers.addItem(item)
            if peer.device_id == current:
                self.peers.setCurrentItem(item)

    def _call_private(self):
        item = self.peers.currentItem()
        if item:
            self.client.call("private", item.data(Qt.UserRole))

    def on_call(self, call):
        if call is None:
            self.call_banner.setVisible(False)
            self.call_actions.setVisible(False)
            return
        who = call.call_sign or f"TX-{call.sender_id[-4:]}"
        kind = {0: "вызов всем", 1: "личный вызов", 2: "групповой вызов",
                3: "ТРЕВОГА"}.get(int(call.call_type), "вызов")
        self.call_banner.setText(f"Входящий {kind}: {who}")
        self.call_banner.setVisible(True)
        self.call_actions.setVisible(True)

    def on_audio(self, frame):
        self.rx_label.setText(f"приём: {self.client.peer_name(frame.sender_id)}")
        self._rx_timer.start(1200)

    def on_ptt(self, active: bool):
        self.ptt.setText("ПЕРЕДАЧА…" if active else "ГОВОРИТЬ  (пробел)")
        self.ptt.setStyleSheet(
            f"background: {theme.DANGER}; color: #fff; font-weight: bold;" if active else "")
