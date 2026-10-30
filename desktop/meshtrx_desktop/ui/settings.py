"""Вкладка настроек: канал, мощность, позывной, шифрование, звук, оформление."""

from __future__ import annotations

import threading

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (QCheckBox, QComboBox, QFormLayout, QGroupBox,
                               QHBoxLayout, QLabel, QLineEdit, QMessageBox,
                               QPushButton, QScrollArea, QSpinBox, QVBoxLayout,
                               QWidget)

from .. import __version__, update_check
from ..audio import list_devices
from ..client import Client
from . import theme
from .bridge import Bridge

MIN_PASSPHRASE = 8


class SettingsTab(QScrollArea):
    update_result = Signal(object)

    def __init__(self, client: Client, bridge: Bridge, on_theme=None, current_theme="main"):
        super().__init__()
        self.client = client
        self.on_theme = on_theme
        # Последние значения, пришедшие из рации. Поле переписываем, только
        # когда значение в рации поменялось: опрос раз в 20 секунд иначе
        # сбивал бы число, которое человек как раз набирает.
        self._radio: dict = {}
        self.setWidgetResizable(True)
        body = QWidget(); self.setWidget(body)
        layout = QVBoxLayout(body)

        # --- радио ---
        radio = QGroupBox("Радио")
        form = QFormLayout(radio)
        self.channel = QComboBox()
        apply_ch = QPushButton("Применить канал")
        apply_ch.clicked.connect(lambda: client.set_channel(self.channel.currentData()))
        ch_row = QHBoxLayout(); ch_row.addWidget(self.channel); ch_row.addWidget(apply_ch)
        form.addRow("Канал", ch_row)

        scan_row = QHBoxLayout()
        self.btn_scan = QPushButton("Найти свободный канал")
        self.btn_scan.clicked.connect(self.scan_channels)
        self.btn_all = QPushButton("Сменить канал у всех")
        self.btn_all.clicked.connect(self.channel_for_all)
        scan_row.addWidget(self.btn_scan); scan_row.addWidget(self.btn_all); scan_row.addStretch()
        form.addRow("", scan_row)
        self.scan_info = QLabel(""); self.scan_info.setProperty("dim", True)
        self.scan_info.setWordWrap(True)
        form.addRow("", self.scan_info)

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

        # --- шифрование ---
        crypto = QGroupBox("Шифрование эфира")
        cf = QFormLayout(crypto)
        self.key_state = QLabel("открытый эфир: ключ не задан")
        self.key_state.setWordWrap(True)
        cf.addRow("Ключ", self.key_state)
        self.phrase = QLineEdit(); self.phrase.setEchoMode(QLineEdit.Password)
        self.phrase.setPlaceholderText(f"Кодовое слово группы, от {MIN_PASSPHRASE} символов")
        key_row = QHBoxLayout()
        btn_key = QPushButton("Применить ключ"); btn_key.setProperty("accent", True)
        btn_key.clicked.connect(self.apply_key)
        btn_clear = QPushButton("Убрать"); btn_clear.clicked.connect(self.clear_key)
        key_row.addWidget(self.phrase, 1); key_row.addWidget(btn_key); key_row.addWidget(btn_clear)
        cf.addRow("", key_row)
        self.hear_open = QCheckBox("Слышать и открытый эфир")
        self.hear_open.clicked.connect(lambda on: client.set_hear_plaintext(on))
        cf.addRow("", self.hear_open)
        hint = QLabel("Ключ выводит сама рация, фраза здесь не хранится. Введите то же "
                      "слово на всех рациях группы — отпечаток из четырёх знаков должен "
                      "совпасть.")
        hint.setProperty("dim", True); hint.setWordWrap(True)
        cf.addRow("", hint)
        self.alien = QLabel(""); self.alien.setWordWrap(True)
        self.alien.setStyleSheet(f"color: {theme.WARN}")
        cf.addRow("", self.alien)
        layout.addWidget(crypto)

        # --- звук ---
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
        ahint = QLabel("Голос идёт кадрами по 20 мс при 8 кГц — как в устройстве. "
                       "Смена динамика применяется сразу, микрофона — со следующей передачи.")
        ahint.setWordWrap(True); ahint.setProperty("dim", True)
        audio_form.addRow("", ahint)
        layout.addWidget(audio)

        # --- оформление ---
        look = QGroupBox("Оформление")
        lf = QFormLayout(look)
        self.theme = QComboBox()
        for key, name in theme.THEME_NAMES.items():
            self.theme.addItem(name, key)
        self.theme.setCurrentIndex(max(0, self.theme.findData(current_theme)))
        self.theme.currentIndexChanged.connect(
            lambda _i: self.on_theme and self.on_theme(self.theme.currentData()))
        lf.addRow("Тема", self.theme)
        layout.addWidget(look)

        # --- о программе ---
        about = QGroupBox("О программе")
        af = QVBoxLayout(about)
        self.versions = QLabel(f"Клиент {__version__} · прошивка рации —")
        af.addWidget(self.versions)
        upd_row = QHBoxLayout()
        self.btn_update = QPushButton("Проверить обновления")
        self.btn_update.clicked.connect(self.check_updates)
        upd_row.addWidget(self.btn_update); upd_row.addStretch()
        af.addLayout(upd_row)
        self.update_info = QLabel(""); self.update_info.setWordWrap(True)
        self.update_info.setOpenExternalLinks(True)
        af.addWidget(self.update_info)
        self.device_info = QLabel("устройство не подключено")
        self.device_info.setProperty("dim", True)
        af.addWidget(self.device_info)
        layout.addWidget(about)
        layout.addStretch()

        bridge.settings_changed.connect(self.on_settings)
        bridge.band_changed.connect(self.on_band)
        self.on_band(client.band)
        bridge.status_changed.connect(self.on_status)
        bridge.key_changed.connect(lambda _k: self.on_key())
        bridge.channel_scan.connect(self.on_scan)
        bridge.firmware.connect(self.on_firmware)
        self.update_result.connect(self.on_update)

    # --- звук ---
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

    # --- каналы ---
    def scan_channels(self):
        self.scan_info.setText("Рация слушает все каналы, это займёт несколько секунд…")
        self.client.scan_channels()

    def on_scan(self, scan):
        band = self.client.band
        levels = sorted(enumerate(scan.levels), key=lambda x: x[1])
        quiet = ", ".join(f"{ch} ({lvl} дБм)" for ch, lvl in levels[:3])
        loud = ", ".join(f"{ch} ({lvl} дБм)" for ch, lvl in levels[-2:])
        self.scan_info.setText(
            f"Тише всего: {quiet}. Шумнее всего: {loud}. "
            f"Канал {scan.best} — {band.mhz(scan.best):.2f} МГц выбран в списке; "
            "чтобы перевести на него всю группу — «Сменить канал у всех».")
        idx = self.channel.findData(scan.best)
        if idx >= 0:
            self.channel.setCurrentIndex(idx)

    def channel_for_all(self):
        ch = self.channel.currentData()
        if ch is None:
            return
        if QMessageBox.question(
                self, "Сменить канал у всех",
                f"Рация разошлёт команду в эфир, и все, кто её услышит, перейдут на "
                f"канал {ch}. Те, кто не услышит, останутся на старом. Перевести?") \
                == QMessageBox.Yes:
            self.client.set_channel_for_all(ch)

    # --- ключ ---
    def apply_key(self):
        phrase = self.phrase.text()
        if len(phrase) < MIN_PASSPHRASE:
            self.key_state.setText(f"Слово слишком короткое — нужно от {MIN_PASSPHRASE} символов")
            return
        self.client.set_passphrase(phrase)
        self.phrase.clear()

    def clear_key(self):
        if QMessageBox.question(self, "Убрать ключ",
                                "Эфир станет открытым: позывной, координаты и речь "
                                "услышит любой на канале. Убрать ключ?") == QMessageBox.Yes:
            self.client.clear_key()

    def on_key(self):
        fp = self.client.key_fingerprint
        if fp:
            self.key_state.setText(f"Ключ задан, отпечаток <b>{fp}</b> — сверьте его с группой")
        else:
            self.key_state.setText("Открытый эфир: ключ не задан, слышно всем на канале")
        if self.client.hear_plaintext is not None:
            self.hear_open.setChecked(self.client.hear_plaintext)
        self.hear_open.setEnabled(bool(fp))
        n = self.client.alien_packets
        self.alien.setText(f"Рядом говорят с другим ключом: {n} пакетов. Проверьте, у всех ли "
                           "одинаковое кодовое слово." if n else "")

    # --- обновления ---
    def check_updates(self):
        self.btn_update.setEnabled(False)
        self.update_info.setText("проверяю…")
        threading.Thread(target=lambda: self.update_result.emit(update_check.check()),
                         daemon=True).start()

    def on_update(self, res: dict):
        self.btn_update.setEnabled(True)
        if res.get("error"):
            self.update_info.setText("Сайт не ответил — нет интернета? Клиент работает и без него.")
        elif res.get("newer"):
            self.update_info.setText(
                f"Вышла версия {res['latest']}: <a href=\"{res['url']}\">скачать</a>")
        else:
            self.update_info.setText(f"У вас последняя версия ({res['current']}).")
        fw = res.get("firmware")
        if fw and self.client.firmware and fw != self.client.firmware:
            self.update_info.setText(self.update_info.text() +
                                     f" Для рации вышла прошивка {fw}: meshtrx.ru/flash")

    def on_firmware(self, version: str):
        self.versions.setText(f"Клиент {__version__} · прошивка рации {version}")

    # --- значения из рации ---
    def on_settings(self, settings: dict):
        def fresh(key):
            changed = key in settings and settings[key] != self._radio.get(key)
            if changed:
                self._radio[key] = settings[key]
            return changed
        if fresh("tx_power"):
            self.power.setValue(int(settings["tx_power"]))
        if fresh("beacon_interval"):
            self.beacon.setValue(int(settings["beacon_interval"]))
        if fresh("callsign") and settings["callsign"]:
            self.call_sign.setText(settings["callsign"])

    def on_band(self, band):
        """Список каналов и предел мощности — по сетке рации (868 или 433)."""
        self.channel.clear()
        digits = 3 if band.is_433 else 2
        for ch in range(band.count):
            self.channel.addItem(f"{ch} — {band.mhz(ch):.{digits}f} МГц", ch)
        idx = self.channel.findData(self.client.channel)
        if idx >= 0:
            self.channel.setCurrentIndex(idx)
        self.power.setRange(band.min_dbm, band.max_dbm)
        self._radio.pop("tx_power", None)     # после смены предела выставить заново

    def on_status(self, status):
        if status is None:
            return
        idx = self.channel.findData(status.channel)
        if idx >= 0 and not self.channel.hasFocus() and not self.scan_info.text():
            self.channel.setCurrentIndex(idx)
        bat = f", батарея {status.battery:.1f} В" if status.battery else ""
        self.device_info.setText(
            f"канал {status.channel}, сигнал {status.rssi} дБм, SNR {status.snr}{bat}")
