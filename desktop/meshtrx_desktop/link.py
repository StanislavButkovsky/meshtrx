"""Связь с устройством по BLE: подключение, переподключение, обмен пакетами.

Живёт в своём потоке с собственным циклом asyncio — интерфейс не должен ждать
радиоэфир. Наружу отдаёт разобранные события через колбэк.

Переподключение здесь бесконечное и с повторным сканированием. Это не
перестраховка: на Android ровно эти две вещи и были сломаны — лимит в десять
попыток заставлял приложение замолчать навсегда, а connect по одному лишь
адресу падал, потому что система забывает устройство после разрыва.
"""

from __future__ import annotations

import asyncio
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass

from bleak import BleakClient, BleakScanner

from . import protocol as proto

RECONNECT_DELAYS = (3, 5, 7, 10, 20, 30, 60)   # секунды, дальше — по последней


@dataclass
class Device:
    name: str
    address: str
    rssi: int | None = None


class Link:
    def __init__(self, on_event: Callable[[object], None],
                 on_state: Callable[[str, str], None] | None = None):
        self._on_event = on_event
        self._on_state = on_state or (lambda state, detail: None)
        self._loop: asyncio.AbstractEventLoop | None = None
        self._thread: threading.Thread | None = None
        self._client: BleakClient | None = None
        self._address: str | None = None
        self._want_connection = False
        self._attempt = 0
        self._connected_at = 0.0
        self.authorized = False

    # ---------- жизненный цикл потока ----------
    def start(self):
        if self._thread:
            return
        ready = threading.Event()

        def runner():
            self._loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self._loop)
            ready.set()
            self._loop.run_forever()

        self._thread = threading.Thread(target=runner, name="meshtrx-ble", daemon=True)
        self._thread.start()
        ready.wait(5)

    def stop(self):
        self._want_connection = False
        if self._loop:
            asyncio.run_coroutine_threadsafe(self._disconnect(), self._loop).result(5)
            self._loop.call_soon_threadsafe(self._loop.stop)
        if self._thread:
            self._thread.join(timeout=3)
        self._thread = None

    def _submit(self, coro):
        if not self._loop:
            raise RuntimeError("Link.start() не вызван")
        return asyncio.run_coroutine_threadsafe(coro, self._loop)

    # ---------- поиск ----------
    def scan(self, timeout: float = 6.0) -> list[Device]:
        return self._submit(self._scan(timeout)).result(timeout + 5)

    async def _scan(self, timeout: float) -> list[Device]:
        self._on_state("scanning", "")
        found: list[Device] = []
        for dev, adv in (await BleakScanner.discover(timeout=timeout,
                                                     return_adv=True)).values():
            if dev.name and "MeshTRX" in dev.name:
                found.append(Device(dev.name, dev.address, adv.rssi))
        found.sort(key=lambda d: -(d.rssi or -999))
        self._on_state("scan_done", f"{len(found)}")
        return found

    # ---------- подключение ----------
    def connect(self, address: str):
        self._address = address
        self._want_connection = True
        self._attempt = 0
        self._submit(self._connect_loop())

    def disconnect(self):
        self._want_connection = False
        self._submit(self._disconnect())

    @property
    def connected(self) -> bool:
        return bool(self._client and self._client.is_connected)

    @property
    def uptime(self) -> float:
        return time.time() - self._connected_at if self.connected else 0.0

    async def _connect_loop(self):
        while self._want_connection and not self.connected:
            self._attempt += 1
            try:
                await self._connect_once()
                self._attempt = 0
                return
            except Exception as e:                                # noqa: BLE001
                delay = RECONNECT_DELAYS[min(self._attempt - 1, len(RECONNECT_DELAYS) - 1)]
                self._on_state("reconnecting",
                               f"попытка {self._attempt}, повтор через {delay} с: {e}")
                await asyncio.sleep(delay)

    async def _connect_once(self):
        self._on_state("connecting", self._address or "")
        target: object = self._address
        if self._attempt > 1:
            # Свежий скан: после разрыва система забывает устройство, и
            # подключение по одному лишь адресу не находит его.
            found = await BleakScanner.find_device_by_address(self._address, timeout=10)
            if found is None:
                raise RuntimeError("устройство не найдено при сканировании")
            target = found

        client = BleakClient(target, timeout=20.0,
                             disconnected_callback=self._on_disconnected)
        await client.connect()
        await client.start_notify(proto.TX_CHAR, self._on_notify)
        self._client = client
        self._connected_at = time.time()
        self.authorized = False
        self._on_state("connected", self._address or "")

    def _on_disconnected(self, _client):
        self._client = None
        self.authorized = False
        self._on_state("disconnected", "")
        if self._want_connection and self._loop:
            self._attempt = 0
            asyncio.run_coroutine_threadsafe(self._connect_loop(), self._loop)

    async def _disconnect(self):
        client, self._client = self._client, None
        if client:
            try:
                await client.disconnect()
            except Exception:                                     # noqa: BLE001
                pass

    # ---------- обмен ----------
    def _on_notify(self, _sender, data: bytearray):
        try:
            self._on_event(proto.parse(bytes(data)))
        except Exception as e:                                    # noqa: BLE001
            self._on_state("parse_error", str(e))

    def send(self, payload: bytes):
        """Отправить команду. Не блокирует вызывающий поток."""
        if not self.connected:
            return False
        self._submit(self._send(payload))
        return True

    async def _send(self, payload: bytes):
        try:
            await self._client.write_gatt_char(proto.RX_CHAR, payload, response=False)
        except Exception as e:                                    # noqa: BLE001
            self._on_state("send_error", str(e))

    def send_sync(self, payload: bytes, timeout: float = 5.0) -> bool:
        """Отправить и дождаться подтверждения записи — для команд, порядок
        которых важен (PIN, настройки, начало файла)."""
        if not self.connected:
            return False
        try:
            self._submit(self._send(payload)).result(timeout)
            return True
        except Exception:                                         # noqa: BLE001
            return False
