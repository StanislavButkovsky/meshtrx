"""Драйвер тестового устройства MeshTRX через UART.

Прошивка в dev-сборке (-DTEST_CONSOLE) принимает команды строками и отвечает
событиями формата `EVT <NAME> key=value ...`. Класс читает порт в фоновом
потоке, складывает события в очередь и даёт удобные ожидания.
"""

from __future__ import annotations

import queue
import re
import threading
import time
from dataclasses import dataclass, field

import serial

EVT_RE = re.compile(r"^EVT\s+(?P<name>[A-Z0-9_]+)\s*(?P<rest>.*)$")
KV_RE = re.compile(r"(\w+)=(\S+)")


@dataclass
class Event:
    name: str
    fields: dict = field(default_factory=dict)
    raw: str = ""
    t: float = 0.0

    def __getitem__(self, key):
        return self.fields[key]

    def get(self, key, default=None):
        return self.fields.get(key, default)

    def int(self, key, default=0):
        try:
            return int(self.fields[key], 0)
        except (KeyError, ValueError):
            return default

    def __repr__(self):
        return f"<{self.name} {self.fields}>"


def discover_ports(count: int = 2) -> list[str]:
    """Найти подключённые платы: V3 отдаётся мостом (ttyUSB), V4 — нативным USB
    (ttyACM). Порядок фиксируем сортировкой, чтобы прогоны были повторяемы."""
    import glob
    ports = sorted(glob.glob("/dev/ttyUSB*")) + sorted(glob.glob("/dev/ttyACM*"))
    return ports[:count]


class Device:
    def __init__(self, port: str, baud: int = 115200, name: str | None = None,
                 log_path: str | None = None):
        self.port = port
        self.name = name or port
        self.ser = serial.Serial(port, baud, timeout=0.2)
        self.events: "queue.Queue[Event]" = queue.Queue()
        self.lines: list[str] = []
        self._log = open(log_path, "a", encoding="utf-8") if log_path else None
        self._stop = threading.Event()
        self._reader = threading.Thread(target=self._read_loop, daemon=True)
        self._reader.start()

    # --- внутреннее ---
    def _read_loop(self):
        buf = b""
        while not self._stop.is_set():
            try:
                data = self.ser.read(256)
            except Exception:
                break
            if not data:
                continue
            buf += data
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                self.lines.append(line)
                if self._log:
                    self._log.write(f"{time.time():.3f} {self.name} {line}\n")
                    self._log.flush()
                m = EVT_RE.match(line)
                if m:
                    fields = dict(KV_RE.findall(m.group("rest")))
                    self.events.put(Event(m.group("name"), fields, line, time.time()))

    # --- публичное ---
    def close(self):
        self._stop.set()
        self._reader.join(timeout=1)
        try:
            self.ser.close()
        except Exception:
            pass
        if self._log:
            self._log.close()

    def send(self, cmd: str):
        self.ser.write((cmd.strip() + "\n").encode())
        self.ser.flush()

    def drain(self):
        """Выбросить накопленные события."""
        while not self.events.empty():
            self.events.get_nowait()

    def wait(self, name: str, timeout: float = 5.0, **match) -> Event | None:
        """Дождаться события с именем name и (опционально) совпадением полей."""
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                ev = self.events.get(timeout=max(0.05, deadline - time.time()))
            except queue.Empty:
                return None
            if ev.name != name:
                continue
            if all(str(ev.get(k)) == str(v) for k, v in match.items()):
                return ev
        return None

    def collect(self, name: str, duration: float) -> list[Event]:
        """Собрать все события с именем name за duration секунд."""
        out, deadline = [], time.time() + duration
        while time.time() < deadline:
            try:
                ev = self.events.get(timeout=max(0.05, deadline - time.time()))
            except queue.Empty:
                break
            if ev.name == name:
                out.append(ev)
        return out

    # --- команды консоли ---
    def ping(self, timeout: float = 3.0) -> Event | None:
        self.drain()
        self.send("PING")
        return self.wait("PONG", timeout)

    def info(self, timeout: float = 3.0) -> Event | None:
        self.drain()
        self.send("INFO")
        return self.wait("INFO", timeout)

    def testmode(self, on: bool = True) -> Event | None:
        self.send(f"TESTMODE {'ON' if on else 'OFF'}")
        return self.wait("TESTMODE", 3.0)

    def set_channel(self, ch: int) -> Event | None:
        self.send(f"CH {ch}")
        return self.wait("CH", 3.0)

    def set_power(self, dbm: int) -> Event | None:
        self.send(f"PWR {dbm}")
        return self.wait("PWR", 3.0)

    def rx_reset(self):
        self.send("RX RESET")
        return self.wait("RX_RESET", 3.0)

    def rx_stats(self, timeout: float = 5.0) -> tuple[Event | None, dict]:
        self.send("RX STATS")
        stats = self.wait("RX_STATS", timeout)
        types = {}
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                ev = self.events.get(timeout=0.3)
            except queue.Empty:
                break
            if ev.name == "RX_TYPE":
                types[ev.get("type")] = ev.int("count")
            elif ev.name == "RX_STATS_END":
                break
        return stats, types

    def send_text(self, dest: str, text: str) -> Event | None:
        self.send(f"TX TEXT {dest} {text}")
        return self.wait("TX_TEXT", 5.0)

    def send_audio(self, count: int, gap_ms: int = 80, timeout: float | None = None):
        self.send(f"TX AUDIO {count} {gap_ms}")
        return self.wait("TX_AUDIO_DONE", timeout or (count * gap_ms / 1000 + 15))

    def send_file(self, ftype: str, size: int, dest: str, timeout: float = 180.0):
        self.send(f"TX FILE {ftype} {size} {dest}")
        accepted = self.wait("TX_FILE", 5.0)
        if not accepted or accepted.get("accepted") != "1":
            return accepted, None
        return accepted, self.wait("FILE_TX", timeout)

    def repeater(self, mode: str = "") -> Event | None:
        """Режим ретранслятора: ON/OFF (с перезагрузкой), STATS, RESET."""
        self.drain()
        self.send(f"REPEATER {mode}".strip())
        name = {"STATS": "REPEATER_STATS", "RESET": "REPEATER_RESET"}.get(
            mode.upper(), "REPEATER")
        return self.wait(name, 8.0)

    def scan_peers(self) -> Event | None:
        """Попросить соседей отозваться маяками, не дожидаясь их интервала."""
        self.send("SCAN")
        return self.wait("SCAN_PEERS", 3.0)

    def send_beacon(self) -> Event | None:
        self.send("TX BEACON")
        return self.wait("TX_BEACON", 5.0)

    def call(self, kind: str, target: str = "") -> Event | None:
        self.send(f"TX CALL {kind} {target}".strip())
        return self.wait("TX_CALL", 5.0)

    def call_response(self, kind: str, seq: int = 0) -> Event | None:
        """Ответ на входящий вызов: accept / reject / cancel."""
        self.send(f"CALL {kind} {seq}".strip())
        return self.wait("CALL_RESP", 5.0)

    def ble_state(self) -> Event | None:
        self.drain()
        self.send("BLE")
        return self.wait("BLE_STATE", 3.0)

    def ble_stats(self) -> Event | None:
        self.drain()
        self.send("BLE STATS")
        return self.wait("BLE_STATS", 3.0)

    def set_loss(self, percent: int, scope: str = "") -> Event | None:
        """Эмуляция потерь канала на приёмнике (по умолчанию только чанки файла)."""
        self.send(f"LOSS {percent} {scope}".strip())
        return self.wait("LOSS", 3.0)

    def load_start(self, profile: str = "mixed", interval_ms: int = 1000,
                   dest: str = "BCAST") -> Event | None:
        self.send(f"LOAD START {profile} {interval_ms} {dest}")
        return self.wait("LOAD_STARTED", 3.0)

    def load_stop(self) -> Event | None:
        self.send("LOAD STOP")
        return self.wait("LOAD_STOPPED", 3.0)

    def load_stats(self) -> Event | None:
        self.send("LOAD STATS")
        return self.wait("LOAD_STATS", 3.0)

    def reboot(self) -> Event | None:
        self.send("REBOOT")
        return self.wait("BOOT", 10.0)

    def dev_id(self) -> str:
        """Собственный идентификатор устройства (кешируется).

        Тесты адресуют друг друга этим ID; захардкоженное значение молча
        ломается при смене порядка портов — адресный пакет становится чужим,
        и проверка «дошло/не дошло» врёт.
        """
        if getattr(self, "_dev_id", None):
            return self._dev_id
        info = self.info()
        self._dev_id = info.get("id") if info else ""
        return self._dev_id

    # --- Учёт времени радио (оценка энергопотребления) ---
    def radio_time(self) -> Event | None:
        self.drain()
        self.send("RADIO TIME")
        return self.wait("RADIO_TIME", 3.0)

    def radio_time_reset(self) -> Event | None:
        self.send("RADIO TIME RESET")
        return self.wait("RADIO_TIME_RESET", 3.0)
