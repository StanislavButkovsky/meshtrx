"""Эмулятор телефона MeshTRX поверх BLE (Nordic UART Service).

Повторяет то, что делает Android-приложение: сканирование, подключение,
подписка на notify, авторизация по PIN, отправка команд. Нужен, чтобы гонять
BLE-путь без телефона — именно на нём и ломалась связь.
"""

from __future__ import annotations

import asyncio
import time

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakDeviceNotFoundError

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_CHAR = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   # телефон → устройство
TX_CHAR = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   # устройство → телефон

CMD_AUDIO_TX = 0x01
CMD_AUDIO_RX = 0x02
CMD_PTT_START = 0x03
CMD_PTT_END = 0x04
CMD_STATUS_UPDATE = 0x06
CMD_SEND_MESSAGE = 0x07
CMD_RECV_MESSAGE = 0x08
CMD_GET_SETTINGS = 0x0B
CMD_SETTINGS_RESP = 0x0C
CMD_PEER_SEEN = 0x17
CMD_PIN_CHECK = 0x25
CMD_PIN_RESULT = 0x26


class Phone:
    def __init__(self, address: str):
        self.address = address
        self.client: BleakClient | None = None
        self.rx: list[tuple[float, bytes]] = []
        self.authorized = False

    # --- поиск ---
    @staticmethod
    async def find(name_part: str = "MeshTRX", timeout: float = 8.0) -> dict[str, str]:
        found = {}
        devices = await BleakScanner.discover(timeout=timeout)
        for d in devices:
            if d.name and name_part in d.name:
                found[d.name] = d.address
        return found

    # --- соединение ---
    async def connect(self, timeout: float = 20.0, retries: int = 3) -> float:
        """Подключиться, при необходимости заново найдя устройство сканом.

        После разрыва BlueZ забывает устройство, и повторный connect по одному
        лишь адресу падает с BleakDeviceNotFoundError. Телефон в такой ситуации
        сканирует заново — делаем то же самое.
        """
        t0 = time.time()
        self.rx.clear()
        self.authorized = False
        last = None
        for attempt in range(retries):
            target = self.address
            if attempt > 0:
                found = await BleakScanner.find_device_by_address(
                    self.address, timeout=min(10.0, timeout))
                if found is None:
                    last = BleakDeviceNotFoundError(self.address)
                    continue
                target = found
            try:
                self.client = BleakClient(target, timeout=timeout)
                await self.client.connect()
                await self.client.start_notify(TX_CHAR, self._on_notify)
                return time.time() - t0
            except Exception as e:            # noqa: BLE001 — пробуем ещё раз
                last = e
                self.client = None
                await asyncio.sleep(1.0)
        raise last if last else RuntimeError("подключение не удалось")

    async def disconnect(self):
        if self.client:
            try:
                await self.client.disconnect()
            except Exception:
                pass
            self.client = None

    @property
    def connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    def _on_notify(self, _sender, data: bytearray):
        self.rx.append((time.time(), bytes(data)))

    # --- обмен ---
    async def send(self, payload: bytes):
        await self.client.write_gatt_char(RX_CHAR, payload, response=False)

    async def wait_cmd(self, cmd: int, timeout: float = 5.0) -> bytes | None:
        deadline = time.time() + timeout
        seen = 0
        while time.time() < deadline:
            while seen < len(self.rx):
                _, data = self.rx[seen]
                seen += 1
                if data and data[0] == cmd:
                    return data
            await asyncio.sleep(0.05)
        return None

    def count_cmd(self, cmd: int) -> int:
        return sum(1 for _, d in self.rx if d and d[0] == cmd)

    # --- операции приложения ---
    async def submit_pin(self, pin: int, timeout: float = 6.0) -> bool:
        await self.send(bytes([CMD_PIN_CHECK]) + pin.to_bytes(4, "little"))
        resp = await self.wait_cmd(CMD_PIN_RESULT, timeout)
        self.authorized = bool(resp and len(resp) >= 2 and resp[1] == 1)
        return self.authorized

    async def send_text(self, seq: int, dest: bytes, text: str):
        await self.send(bytes([CMD_SEND_MESSAGE, seq]) + dest + text.encode())

    async def ptt(self, on: bool):
        await self.send(bytes([CMD_PTT_START if on else CMD_PTT_END]))

    async def send_audio_frame(self, payload: bytes = b"\x00" * 32):
        await self.send(bytes([CMD_AUDIO_TX]) + payload)

    async def request_settings(self):
        await self.send(bytes([CMD_GET_SETTINGS]))
        return await self.wait_cmd(CMD_SETTINGS_RESP, 6.0)
