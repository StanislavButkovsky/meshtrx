"""Вкладка настроек: канал, мощность, позывной, звук, ретранслятор."""

from __future__ import annotations

from PySide6.QtWidgets import (QCheckBox, QComboBox, QFormLayout, QGroupBox,
                               QHBoxLayout, QLabel, QLineEdit, QPushButton,
                               QSpinBox, QVBoxLayout, QWidget)

from ..audio import list_devices
from ..client import Client
from .bridge import Bridge

NUM_CHANNELS = 23
FREQ_BASE = 863.15          # МГц, канал 0
FREQ_STEP = 0.30


class SettingsTab(QWidget):
    def __init__(self, client: Client, bridge: Bridge):
        super().__init__()
        self.client = client
        layout = QVBoxLayout(self)

        radio = QGroupBox("Радио")
        form = QFormLayout(radio)
        self.channel = QComboBox()
        for ch in range(NUM_CHANNELS):
            self.channel.addItem(f"{ch} — {FREQ_BASE + ch * FREQ_STEP:.2f} МГц", ch)
        apply_ch = QPushButton("Применить канал")
        apply_ch.clicked.connect(lambda: client.set_channel(self.channel.currentData()))
        ch_row = QHBoxLayout(); ch_row.addWidget(self.channel); ch_row.addWidget(apply_ch)
        form.addRow("Канал", ch_row)

        self.power = QSpinBox(); self.power.setRange(-9, 22); self.power.setSuffix(" дБм")
        apply_pw = QPushButton("Применить мощность")
        apply_pw.clicked.connect(lambda: client.apply_settings(tx_power=self.power.value()))
        pw_row = QHBoxLayout(); pw_row.addWidget(self.power); pw_row.addWidget(apply_pw)
        form.addRow("Мощность", pw_row)

        self.call_sign = QLineEdit(); self.call_sign.setMaxLength(8)
        apply_cs = QPushButton("Сохранить позывной")
        apply_cs.clicked.connect(lambda: client.set_call_sign(self.call_sign.text().strip()))
        cs_row = QHBoxLayout(); cs_row.addWidget(self.call_sign); cs_row.addWidget(apply_cs)
        form.addRow("Позывной", cs_row)

        self.beacon = QSpinBox(); self.beacon.setRange(30, 3600); self.beacon.setSuffix(" с")
        apply_bc = QPushButton("Применить")
        apply_bc.clicked.connect(
            lambda: client.apply_settings(beacon_interval=self.beacon.value()))
        bc_row = QHBoxLayout(); bc_row.addWidget(self.beacon); bc_row.addWidget(apply_bc)
        form.addRow("Интервал маяка", bc_row)

        self.repeater = QCheckBox("Работать ретранслятором")
        self.repeater.toggled.connect(client.set_repeater)
        form.addRow("", self.repeater)
        layout.addWidget(radio)

        audio = QGroupBox("Звук")
        audio_form = QFormLayout(audio)
        ins, outs = list_devices()
        self.mic = QComboBox(); self.mic.addItem("по умолчанию", None)
        for idx, name in ins:
            self.mic.addItem(name, idx)
        self.spk = QComboBox(); self.spk.addItem("по умолчанию", None)
        for idx, name in outs:
            self.spk.addItem(name, idx)
        self.mic.currentIndexChanged.connect(self._apply_audio)
        self.spk.currentIndexChanged.connect(self._apply_audio)
        audio_form.addRow("Микрофон", self.mic)
        audio_form.addRow("Динамик", self.spk)
        hint = QLabel("Голос идёт кадрами по 20 мс при 8 кГц — как в устройстве. "
                      "Смена динамика применяется сразу, микрофона — со следующей передачи.")
        hint.setWordWrap(True); hint.setProperty("dim", True)
        audio_form.addRow("", hint)
        layout.addWidget(audio)

        self.device_info = QLabel("устройство не подключено")
        self.device_info.setProperty("dim", True)
        layout.addWidget(self.device_info)
        layout.addStretch()

        bridge.settings_changed.connect(self.on_settings)
        bridge.status_changed.connect(self.on_status)

    def _apply_audio(self):
        """Динамик можно переключить на лету, микрофон — только между
        передачами: поток захвата пересоздаётся при следующем нажатии."""
        self.client.playback.stop()
        self.client.playback._device = self.spk.currentData()
        self.client.playback.start()
        if self.client.capture:
            self.client.capture.close()
            self.client.capture = None
        self.client.mic_device = self.mic.currentData()

    def on_settings(self, settings: dict):
        if "tx_power" in settings:
            self.power.setValue(int(settings["tx_power"]))
        if "beacon_interval" in settings:
            self.beacon.setValue(int(settings["beacon_interval"]))
        if settings.get("callsign"):
            self.call_sign.setText(settings["callsign"])

    def on_status(self, status):
        if status is None:
            return
        idx = self.channel.findData(status.channel)
        if idx >= 0 and not self.channel.hasFocus():
            self.channel.setCurrentIndex(idx)
        bat = f", батарея {status.battery:.1f} В" if status.battery else ""
        self.device_info.setText(
            f"канал {status.channel}, сигнал {status.rssi} дБм, SNR {status.snr}{bat}")
