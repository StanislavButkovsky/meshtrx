"""Главное окно: шапка со связью и вкладки по функциям."""

from __future__ import annotations

from PySide6.QtCore import Qt, QThread, QTimer, Signal
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QInputDialog, QLabel,
                               QMainWindow, QMessageBox, QPushButton,
                               QStatusBar, QTabWidget, QVBoxLayout, QWidget)

from ..client import Client
from ..config import Config
from . import theme
from .bridge import Bridge
from .diagnostics import DiagnosticsTab
from .files import FilesTab
from .map import MapTab
from .messages import MessagesTab
from .radar import RadarTab
from .settings import SettingsTab
from .voice import VoiceTab


class ScanWorker(QThread):
    """Сканирование занимает секунды, а окно за это время не должно замирать."""

    done = Signal(list)

    def __init__(self, client: Client, seconds: float = 6.0):
        super().__init__()
        self.client, self.seconds = client, seconds

    def run(self):
        try:
            self.done.emit(self.client.scan(self.seconds))
        except Exception:                                        # noqa: BLE001
            self.done.emit([])


class MainWindow(QMainWindow):
    def __init__(self, client: Client, config: Config | None = None):
        super().__init__()
        self.client = client
        self.config = config or Config()
        self.bridge = Bridge(client)
        self.setWindowTitle("MeshTRX")
        self.resize(980, 700)

        central = QWidget()
        root = QVBoxLayout(central)
        root.addWidget(self._build_header())

        self.tabs = QTabWidget()
        self.tabs.addTab(VoiceTab(client, self.bridge), "Голос")
        self.tabs.addTab(MessagesTab(client, self.bridge), "Сообщения")
        self.tabs.addTab(FilesTab(client, self.bridge), "Файлы")
        self.tabs.addTab(RadarTab(client, self.bridge), "Радар")
        self.tabs.addTab(MapTab(client, self.bridge), "Карта")
        self.tabs.addTab(SettingsTab(client, self.bridge), "Настройки")
        self.tabs.addTab(DiagnosticsTab(client, self.bridge), "Диагностика")
        self.tabs.setEnabled(False)
        root.addWidget(self.tabs, 1)
        self.setCentralWidget(central)

        self.setStatusBar(QStatusBar())
        self.statusBar().showMessage("не подключено")

        self.bridge.state_changed.connect(self.on_state)
        self.bridge.status_changed.connect(self.on_status)
        self.bridge.pin_result.connect(self.on_pin)
        self.bridge.call_changed.connect(self.on_call)

        # Устройство, с которым уже работали, подключаем сами: пользователю
        # не нужно каждый раз искать его в списке.
        QTimer.singleShot(300, self.start_up)
        # Устройство освобождает канал от молчащего клиента, поэтому
        # напоминаем о себе, даже когда пользователь просто слушает эфир.
        self._keepalive = QTimer(self)
        self._keepalive.timeout.connect(self.client.keepalive)
        self._keepalive.start(20000)

    def start_up(self):
        last = self.config.last_device
        if last:
            self.statusBar().showMessage("подключаемся к последнему устройству…")
            from ..link import Device as LinkDevice
            self.client.connect(LinkDevice(name="MeshTRX", address=last))
            QTimer.singleShot(12000, self._scan_if_idle)
        else:
            self.scan()

    def _scan_if_idle(self):
        if not self.client.link.connected:
            self.scan()

    # ---------- шапка ----------
    def _build_header(self) -> QWidget:
        header = QWidget()
        row = QHBoxLayout(header)
        row.setContentsMargins(0, 0, 0, 6)

        self.devices = QComboBox(); self.devices.setMinimumWidth(260)
        row.addWidget(self.devices)
        self.btn_scan = QPushButton("Искать"); self.btn_scan.clicked.connect(self.scan)
        row.addWidget(self.btn_scan)
        self.btn_connect = QPushButton("Подключиться")
        self.btn_connect.setProperty("accent", True)
        self.btn_connect.clicked.connect(self.toggle_connection)
        row.addWidget(self.btn_connect)

        row.addStretch()
        self.link_label = QLabel("нет связи"); self.link_label.setProperty("dim", True)
        row.addWidget(self.link_label)
        self.signal_label = QLabel(""); self.signal_label.setProperty("dim", True)
        row.addWidget(self.signal_label)
        self.battery_label = QLabel(""); self.battery_label.setProperty("dim", True)
        row.addWidget(self.battery_label)
        return header

    # ---------- действия ----------
    def scan(self):
        if getattr(self, "_scanner", None) and self._scanner.isRunning():
            return
        self.btn_scan.setEnabled(False)
        self.statusBar().showMessage("поиск устройств…")
        self._scanner = ScanWorker(self.client)
        self._scanner.done.connect(self.on_scan_done)
        self._scanner.start()

    def on_scan_done(self, found: list):
        self.devices.clear()
        for dev in found:
            self.devices.addItem(f"{dev.name}   {dev.rssi} дБм", dev)
        self.btn_scan.setEnabled(True)
        self.statusBar().showMessage(
            f"найдено устройств: {len(found)}" if found
            else "устройства не найдены — включите питание и Bluetooth")

    def toggle_connection(self):
        if self.client.link.connected:
            self.client.disconnect()
            return
        dev = self.devices.currentData()
        if dev is None:
            self.statusBar().showMessage("сначала выберите устройство")
            return
        self.client.connect(dev)

    def on_state(self, state: str, detail: str):
        human = {
            "scanning": "поиск…", "connecting": "подключение…",
            "connected": "подключено", "disconnected": "связь потеряна",
            "reconnecting": "переподключение…",
        }.get(state, state)
        self.link_label.setText(human)
        self.link_label.setStyleSheet(
            f"color: {theme.ACCENT}" if state == "connected" else f"color: {theme.TEXT_DIM}")
        self.btn_connect.setText("Отключиться" if state == "connected" else "Подключиться")
        self.tabs.setEnabled(state == "connected")
        self.statusBar().showMessage(f"{human} {detail}".strip())

        if state == "connected":
            dev = self.client.device
            if dev:
                self.config.last_device = dev.address
            QTimer.singleShot(800, self.authorize)

    def authorize(self):
        """PIN спрашиваем только у незнакомого устройства.

        Телефон ведёт себя так же: один раз при первом подключении. Иначе
        каждое переподключение — а их бывает много — упиралось бы в диалог.
        """
        if self.client.authorized or not self.client.link.connected:
            return
        dev = self.client.device
        saved = self.config.known_pin(dev.address) if dev else None
        if saved is not None:
            self.client.submit_pin(saved)
            return
        self.ask_pin()

    def ask_pin(self):
        if self.client.authorized or not self.client.link.connected:
            return
        pin, ok = QInputDialog.getInt(self, "PIN устройства",
                                      "Введите PIN, показанный на экране устройства:",
                                      0, 0, 9999)
        if ok:
            self._pending_pin = pin
            self.client.submit_pin(pin)

    def on_pin(self, accepted: bool):
        dev = self.client.device
        if accepted:
            self.statusBar().showMessage("PIN принят")
            if dev and getattr(self, "_pending_pin", None) is not None:
                self.config.remember_pin(dev.address, self._pending_pin)
                self._pending_pin = None
        else:
            if dev:
                self.config.forget_device(dev.address)   # сохранённый не подошёл
            QMessageBox.warning(self, "PIN", "Устройство не приняло PIN")
            QTimer.singleShot(300, self.ask_pin)

    def on_status(self, status):
        if status is None:
            return
        self.signal_label.setText(f"канал {status.channel}   {status.rssi} дБм")
        if status.battery:
            self.battery_label.setText(f"{status.battery:.1f} В")

    def on_call(self, call):
        if call is not None:
            self.tabs.setCurrentIndex(0)
            self.raise_(); self.activateWindow()

    def closeEvent(self, event):
        self.client.stop()
        super().closeEvent(event)
