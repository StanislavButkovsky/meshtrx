"""Ядро клиента: связь, голос и состояние сети в одном месте.

Интерфейс (Qt или консоль) только подписывается на события и дёргает методы.
Такое разделение позволяет проверять всю логику на живом устройстве без окна
и не тащить Qt в тесты.
"""

from __future__ import annotations

import atexit
import json
import signal
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field

from . import protocol as proto
from .audio import Capture, Playback
from .link import Device, Link

PEER_STALE_SEC = 15 * 60      # старше — абонент считается пропавшим


@dataclass
class Message:
    text: str
    outgoing: bool
    peer_id: str
    peer_name: str = ""
    rssi: int | None = None
    delivered: bool = False
    seq: int | None = None
    time: float = field(default_factory=time.time)


@dataclass
class FileTransfer:
    name: str
    size: int
    incoming: bool
    peer_id: str = ""
    done: int = 0
    total: int = 0
    data: bytearray = field(default_factory=bytearray)
    finished: bool = False


class Client:
    """Всё состояние клиента. События наружу — через subscribe()."""

    def __init__(self):
        self.link = Link(self._on_link_event, self._on_link_state)
        self.playback = Playback()
        self.capture: Capture | None = None

        self.state = "disconnected"
        self.state_detail = ""
        self.authorized = False
        self.device: Device | None = None

        self.channel = 0
        self.rssi = 0
        self.snr = 0
        self.battery: float | None = None
        self.settings: dict = {}
        self.call_sign = ""

        self.peers: dict[str, proto.Peer] = {}
        self.peer_seen: dict[str, float] = {}
        self.messages: list[Message] = []
        self.incoming_call: proto.IncomingCall | None = None
        self.call_active = False
        self.transfers: list[FileTransfer] = []

        self.ptt_active = False
        self.listen_all = True
        self.upload_status: proto.UploadStatus | None = None
        self.mic_level = 0.0
        self._rx_file: FileTransfer | None = None
        self._seq = 0
        self._subscribers: list[Callable[[str, object], None]] = []
        self._lock = threading.Lock()

    # ---------- подписка ----------
    def subscribe(self, callback: Callable[[str, object], None]):
        self._subscribers.append(callback)

    def _emit(self, event: str, payload: object = None):
        for cb in list(self._subscribers):
            try:
                cb(event, payload)
            except Exception:                                    # noqa: BLE001
                pass

    # ---------- подключение ----------
    def start(self):
        self.link.start()
        self.playback.start()
        # Соединение, брошенное без разрыва, устройство держит до супервизорного
        # таймаута и всё это время не рекламируется — снаружи выглядит как
        # «рация пропала». Поэтому закрываемся при любом выходе, включая Ctrl+C.
        atexit.register(self._emergency_stop)
        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                previous = signal.getsignal(sig)
                signal.signal(sig, lambda s, f, prev=previous: (
                    self._emergency_stop(),
                    prev(s, f) if callable(prev) else None))
            except (ValueError, OSError):
                pass       # не главный поток — обработчик поставить нельзя

    def _emergency_stop(self):
        try:
            if self.link.connected:
                self.link.disconnect()
                time.sleep(0.3)
        except Exception:                                        # noqa: BLE001
            pass

    def stop(self):
        self.stop_ptt()
        if self.capture:
            self.capture.close()
            self.capture = None
        self.playback.close()
        self.link.stop()

    def scan(self, timeout: float = 6.0) -> list[Device]:
        return self.link.scan(timeout)

    def connect(self, device: Device):
        self.device = device
        self.link.connect(device.address)

    def disconnect(self):
        self.link.disconnect()

    def submit_pin(self, pin: int):
        self.link.send_sync(proto.pin_check(pin))

    # ---------- команды ----------
    def request_settings(self):
        self.link.send(proto.get_settings())

    def apply_settings(self, **values):
        """Настройки уходят одним JSON — устройство так их и ждёт."""
        self.link.send_sync(proto.set_settings(json.dumps(values)))
        self.settings.update(values)

    def set_channel(self, ch: int):
        self.link.send(proto.set_channel(ch))
        self.channel = ch
        self._emit("status", None)

    def set_call_sign(self, call_sign: str):
        self.call_sign = call_sign
        self.apply_settings(callsign=call_sign)

    def set_repeater(self, enabled: bool):
        self.link.send(proto.set_repeater(enabled))

    def send_message(self, text: str, dest_id: str | None = None) -> Message:
        self._seq = (self._seq + 1) & 0xFF
        self.link.send(proto.send_message(self._seq, text, dest_id))
        msg = Message(text=text, outgoing=True, peer_id=dest_id or proto.BROADCAST,
                      peer_name=self.peer_name(dest_id), seq=self._seq)
        self.messages.append(msg)
        self._emit("message", msg)
        return msg

    def call(self, kind: str, device_id: str | None = None):
        if kind == "all":
            self.link.send(proto.call_all())
        elif kind == "sos":
            self.link.send(proto.call_emergency())
        elif kind == "private" and device_id:
            self.link.send(proto.call_private(device_id))

    def accept_call(self):
        if self.incoming_call:
            self.link.send(proto.call_accept(self.incoming_call.seq))
            self.call_active = True
            self.incoming_call = None
            self._emit("call", None)

    def reject_call(self):
        if self.incoming_call:
            self.link.send(proto.call_reject(self.incoming_call.seq))
            self.incoming_call = None
            self._emit("call", None)

    def cancel_call(self):
        self.link.send(proto.call_cancel())
        self.call_active = False
        self._emit("call", None)

    def send_file(self, name: str, data: bytes, file_type: int = proto.FileType.BINARY,
                  dest_id: str | None = None, chunk: int = 96):
        """Файл сначала целиком загружается в память устройства, и только потом
        уходит в эфир — так же, как это делает телефон."""
        transfer = FileTransfer(name=name, size=len(data), incoming=False,
                                peer_id=dest_id or "")
        self.transfers.append(transfer)
        self._emit("transfer", transfer)

        self.upload_status = None
        self.link.send_sync(proto.file_upload_start(name, file_type, len(data), dest_id))
        # Ждём ответа устройства: молча слать содержимое в занятое или
        # переполненное устройство бессмысленно — оно всё равно его выбросит.
        deadline = time.time() + 3
        while time.time() < deadline and self.upload_status is None:
            time.sleep(0.05)
        if self.upload_status and not self.upload_status.accepted:
            transfer.finished = True
            self._emit("transfer_failed", (transfer, self.upload_status.text))
            return transfer

        for off in range(0, len(data), chunk):
            self.link.send_sync(proto.file_upload_data(data[off:off + chunk]))
            transfer.done = min(off + chunk, len(data))
            self._emit("transfer", transfer)
            time.sleep(0.02)          # устройство пишет в свою память, не спешим
        return transfer

    # ---------- голос ----------
    def start_ptt(self):
        if self.ptt_active or not self.link.connected:
            return
        self.ptt_active = True
        self.link.send(proto.ptt(True))
        if self.capture is None:
            self.capture = Capture(on_packet=self._on_captured,
                                   on_level=self._on_level)
        self.capture.start()
        self._emit("ptt", True)

    def stop_ptt(self):
        if not self.ptt_active:
            return
        self.ptt_active = False
        if self.capture:
            self.capture.stop()
        self.link.send(proto.ptt(False))
        self.mic_level = 0.0
        self._emit("ptt", False)

    def _on_captured(self, packet: bytes):
        self.link.send(proto.audio(packet))

    def _on_level(self, level: float):
        self.mic_level = level
        self._emit("mic_level", level)

    # ---------- абоненты ----------
    def peer_name(self, peer_id: str | None) -> str:
        if not peer_id or peer_id == proto.BROADCAST:
            return "всем"
        peer = self.peers.get(peer_id) or next(
            (p for k, p in self.peers.items() if k.endswith(peer_id[-4:])), None)
        return peer.call_sign if peer and peer.call_sign else f"TX-{peer_id[-4:]}"

    def active_peers(self) -> list[proto.Peer]:
        now = time.time()
        return [p for pid, p in self.peers.items()
                if now - self.peer_seen.get(pid, 0) < PEER_STALE_SEC]

    # ---------- события связи ----------
    def _on_link_state(self, state: str, detail: str):
        self.state, self.state_detail = state, detail
        if state == "connected":
            self.request_settings()
        if state == "disconnected":
            self.authorized = False
            self.call_active = False
        self._emit("state", (state, detail))

    def _on_link_event(self, event: object):
        with self._lock:
            self._handle(event)

    def _handle(self, event: object):
        if isinstance(event, proto.Status):
            self.channel = event.channel
            self.rssi, self.snr = event.rssi, event.snr
            if event.battery:
                self.battery = event.battery
            self._emit("status", event)

        elif isinstance(event, proto.IncomingMessage):
            msg = Message(text=event.text, outgoing=False, peer_id=event.sender_id,
                          peer_name=self.peer_name(event.sender_id), rssi=event.rssi,
                          delivered=True)
            self.messages.append(msg)
            self._emit("message", msg)

        elif isinstance(event, proto.AudioFrame):
            if self.listen_all or self.call_active:
                self.playback.push(event.payload, event.is_last)
                self._emit("audio_rx", event)

        elif isinstance(event, proto.Peer):
            self.peers[event.device_id] = event
            self.peer_seen[event.device_id] = time.time()
            self._emit("peers", event)

        elif isinstance(event, proto.IncomingCall):
            self.incoming_call = event
            self._emit("call", event)

        elif isinstance(event, proto.FileProgress):
            if self._rx_file:
                self._rx_file.done, self._rx_file.total = event.done, event.total
                self._emit("transfer", self._rx_file)

        elif isinstance(event, proto.UploadStatus):
            self.upload_status = event
            self._emit("upload_status", event)

        elif isinstance(event, proto.IncomingFileHeader):
            self._rx_file = FileTransfer(name=event.name, size=event.size,
                                         incoming=True, peer_id=event.sender_id,
                                         total=event.chunks)
            self.transfers.append(self._rx_file)
            self._emit("transfer", self._rx_file)

        elif isinstance(event, tuple):
            self._handle_named(event[0], event[1])

    def _handle_named(self, kind: str, payload):
        if kind == "settings":
            try:
                self.settings = json.loads(payload)
                self.call_sign = self.settings.get("callsign", self.call_sign)
            except json.JSONDecodeError:
                pass
            self._emit("settings", self.settings)

        elif kind == "pin":
            self.authorized = bool(payload)
            self.link.authorized = self.authorized
            self._emit("pin", self.authorized)

        elif kind == "message_ack":
            for msg in reversed(self.messages):
                if msg.outgoing and not msg.delivered and msg.seq == payload:
                    msg.delivered = True
                    self._emit("message", msg)
                    break

        elif kind == "file_data" and self._rx_file:
            self._rx_file.data.extend(payload)
            if len(self._rx_file.data) >= self._rx_file.size:
                self._rx_file.finished = True
                self._emit("file_done", self._rx_file)
                self._rx_file = None

        elif kind == "location_request":
            # Своих координат у настольного клиента нет: отвечаем тем, что
            # задал пользователь в настройках, иначе нулями — устройство ждёт
            # ответа и без него держит запрос.
            lat = self.settings.get("my_lat")
            lon = self.settings.get("my_lon")
            self.link.send(proto.location_update(lat, lon))

        elif kind == "incoming_call":
            pass
