"""Связь с рацией по USB-кабелю.

По проводу идут те же пакеты, что по Bluetooth, в рамках
``0xC1  SLIP(пакет + CRC-16)  0xC0``. Всё, что между рамками, — служебный
журнал прошивки: он пишется в тот же порт, и мы его пропускаем. Байты 0xC0 и
0xC1 в тексте UTF-8 не встречаются, поэтому рамки с журналом не путаются.

Кабель не сообщает, слушают ли на другом конце, поэтому раз в несколько секунд
уходит пустая рамка: без неё рация через 15 с решает, что хоста нет.
"""

from __future__ import annotations

import threading
import time
from collections.abc import Callable

import serial
from serial.tools import list_ports

FRAME_START = 0xC1
FRAME_END = 0xC0
ESC = 0xDB
KEEPALIVE_S = 5.0
BAUD = 115200

# Мост CP210x на Heltec V3 и встроенный USB ESP32-S3 на V4
USB_IDS = {(0x10C4, 0xEA60), (0x303A, 0x1001)}


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) if crc & 0x8000 else crc << 1
            crc &= 0xFFFF
    return crc


def encode(payload: bytes) -> bytes:
    body = payload + crc16(payload).to_bytes(2, "big")
    out = bytearray([FRAME_START])
    for b in body:
        if b in (FRAME_START, FRAME_END, ESC):
            out += bytes([ESC, b ^ 0x20])
        else:
            out.append(b)
    out.append(FRAME_END)
    return bytes(out)


class Decoder:
    """Собирает рамки из потока байтов; журнал между рамками отбрасывает."""

    def __init__(self):
        self._buf = bytearray()
        self._in = False
        self._esc = False

    def feed(self, data: bytes) -> list[bytes]:
        frames = []
        for b in data:
            if b == FRAME_START:
                self._buf.clear()
                self._in, self._esc = True, False
            elif not self._in:
                continue
            elif b == FRAME_END:
                self._in = False
                if len(self._buf) >= 2:
                    body, crc = bytes(self._buf[:-2]), int.from_bytes(self._buf[-2:], "big")
                    if crc16(body) == crc and body:
                        frames.append(body)
            elif self._esc:
                self._buf.append(b ^ 0x20)
                self._esc = False
            elif b == ESC:
                self._esc = True
            elif len(self._buf) < 1100:
                self._buf.append(b)
            else:
                self._in = False
        return frames


def find_ports() -> list[tuple[str, str]]:
    """Порты, похожие на рацию: (устройство, подпись)."""
    found = []
    for p in list_ports.comports():
        if (p.vid, p.pid) in USB_IDS:
            found.append((p.device, p.description or p.device))
    return found


class UsbTransport:
    def __init__(self, port: str, on_payload: Callable[[bytes], None],
                 on_state: Callable[[str, str], None]):
        self.port = port
        self._on_payload = on_payload
        self._on_state = on_state
        self._ser: serial.Serial | None = None
        self._want = False
        self._lock = threading.Lock()
        self._threads: list[threading.Thread] = []

    @property
    def is_open(self) -> bool:
        return bool(self._ser and self._ser.is_open)

    def start(self):
        self._want = True
        t = threading.Thread(target=self._run, name="meshtrx-usb", daemon=True)
        t.start()
        self._threads = [t]

    def stop(self):
        self._want = False
        self._close()

    def _open(self) -> serial.Serial:
        s = serial.Serial()
        s.port, s.baudrate, s.timeout = self.port, BAUD, 0.2
        # Линии DTR/RTS подключены к сбросу и загрузчику платы. При открытии
        # порта система поднимает обе разом — это безопасно. Опускать их нельзя:
        # по дороге они проходят сочетание «сброс», и рация перезагружается
        # (проверено на V3 и V4.3).
        s.open()
        return s

    def _close(self):
        ser, self._ser = self._ser, None
        if ser:
            try:
                ser.close()
            except Exception:                                     # noqa: BLE001
                pass

    def _run(self):
        while self._want:
            try:
                self._on_state("connecting", self.port)
                self._ser = self._open()
                self.send(b"")
                self._on_state("connected", self.port)
                self._read_loop()
            except Exception as e:                                # noqa: BLE001
                if self._want:
                    self._on_state("reconnecting", f"{self.port}: {e}")
            self._close()
            if self._want:
                self._on_state("disconnected", "")
                time.sleep(3)

    def _read_loop(self):
        dec = Decoder()
        last_ka = time.time()
        while self._want and self._ser:
            data = self._ser.read(512)
            for frame in dec.feed(data):
                self._on_payload(frame)
            if time.time() - last_ka >= KEEPALIVE_S:
                self.send(b"")
                last_ka = time.time()

    def send(self, payload: bytes) -> bool:
        ser = self._ser
        if not ser:
            return False
        try:
            with self._lock:
                ser.write(encode(payload))
            return True
        except Exception as e:                                    # noqa: BLE001
            self._on_state("send_error", str(e))
            return False
