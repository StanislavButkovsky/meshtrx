"""Ядро клиента: связь, голос и состояние сети в одном месте.

Интерфейс (Qt или консоль) только подписывается на события и дёргает методы.
Такое разделение позволяет проверять всю логику на живом устройстве без окна
и не тащить Qt в тесты.
"""

from __future__ import annotations

import atexit
import json
import os
import signal
import threading
import time
from collections.abc import Callable
from dataclasses import dataclass, field

from . import protocol as proto
from .audio import Capture, Playback
from .link import Device, Link

PEER_STALE_SEC = 15 * 60      # старше — абонент считается пропавшим
# Предел одной передачи. Столько же стоит в прошивке и в записи голосовых
# сообщений: в полудуплексе говорящий занимает канал целиком, и без предела
# один человек затыкает всю сеть.
PTT_MAX_SECONDS = 10
# Сколько ждём ответа на входящий вызов, как экран вызова в телефоне. Рация
# сама об окончании общего вызова не сообщает — без своего отсчёта вызов
# висел бы вечно и не считался пропущенным.
CALL_RING_SECONDS = 30


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
    stored: bool = False          # «принято нодой»: ждёт на рации адресата без телефона
    failed: bool = False
    encrypted: bool | None = None
    voice: bytes | None = None    # голосовое сообщение: кадры Codec2 3200
    file_name: str = ""

    @property
    def voice_seconds(self) -> int:
        return len(self.voice) // 8 * 20 // 1000 if self.voice else 0


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
    file_type: int | None = None


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
        self.band = proto.Band()
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

        # Шифрование эфира: ключ хранит рация, здесь только его отпечаток
        self.key_fingerprint = ""
        self.hear_plaintext: bool | None = None
        self.alien_packets = 0
        self.channel_scan: proto.ChannelScan | None = None
        self.firmware = ""

        # Счётчики на вкладках, как в телефоне: непрочитанные сообщения и
        # пропущенные вызовы. Обнуляет интерфейс, когда вкладку открывают.
        self.unread_messages = 0
        self.missed_calls = 0
        self.chat_open = False

        self.recording = False
        self._rec: bytearray = bytearray()
        self._rec_started = 0.0
        self._voice_out: Message | None = None
        self.upload_status: proto.UploadStatus | None = None
        self.mic_level = 0.0
        self._ptt_started = 0.0
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
        # таймаута и всё это время не объявляет о себе — снаружи выглядит как
        # «рация пропала». Поэтому закрываемся при любом выходе, включая Ctrl+C.
        atexit.register(self._emergency_stop)
        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                previous = signal.getsignal(sig)
                signal.signal(sig, lambda s, f, prev=previous: self._on_signal(s, f, prev))
            except (ValueError, OSError):
                pass       # не главный поток — обработчик поставить нельзя

    def _on_signal(self, sig, frame, previous):
        """Закрыть соединение и завершиться так, как завершились бы без нас.
        Раньше при стандартном обработчике сигнал просто глотался, и
        программа не закрывалась по SIGTERM."""
        self._emergency_stop()
        if callable(previous):
            previous(sig, frame)
        else:
            signal.signal(sig, signal.SIG_DFL)
            os.kill(os.getpid(), sig)

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

    def keepalive(self):
        """Напомнить устройству, что на другом конце кто-то есть.

        Устройство освобождает канал, если клиент долго молчит: иначе
        брошенное соединение делает рацию невидимой для всех остальных.
        Клиент, который просто слушает эфир, обязан подавать признаки жизни.
        """
        if self.link.connected:
            self.link.send(proto.get_settings())

    def apply_settings(self, **values):
        """Настройки уходят одним JSON — устройство так их и ждёт."""
        self.link.send_sync(proto.set_settings(json.dumps(values)))
        self.settings.update(values)

    def scan_peers(self):
        """Обновить список абонентов, не дожидаясь их очередных маяков."""
        self.link.send(proto.scan_peers())

    def set_channel(self, ch: int):
        self.link.send(proto.set_channel(ch))
        self.channel = ch
        self._emit("status", None)

    def set_call_sign(self, call_sign: str):
        self.call_sign = call_sign
        self.apply_settings(callsign=call_sign)

    def set_repeater(self, enabled: bool):
        self.link.send(proto.set_repeater(enabled))

    def scan_channels(self):
        self.link.send(proto.scan_channels())

    def set_channel_for_all(self, ch: int):
        self.link.send(proto.set_channel_all(ch))
        self.channel = ch
        self._emit("status", None)

    def set_passphrase(self, phrase: str):
        self.link.send(proto.set_key(phrase))
        self.link.send(proto.get_key_state())

    def clear_key(self):
        self.link.send(proto.clear_key())
        self.link.send(proto.get_key_state())

    def set_hear_plaintext(self, on: bool):
        self.link.send(proto.hear_plaintext(on))
        self.link.send(proto.get_key_state())

    def mark_chat_read(self):
        self.unread_messages = 0
        self._emit("counters", None)

    def mark_calls_seen(self):
        self.missed_calls = 0
        self._emit("counters", None)

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

    # ---------- голосовые сообщения ----------
    def start_voice_record(self):
        """Запись голосового сообщения: тот же захват, что у передачи, но
        кадры копятся в памяти, а не уходят в эфир."""
        if self.recording or self.ptt_active:
            return
        self.recording = True
        self._rec = bytearray()
        self._rec_started = time.time()
        if self.capture is None:
            self.capture = Capture(on_packet=self._on_captured, on_level=self._on_level)
        self.capture.start()
        self._emit("recording", True)

    def stop_voice_record(self) -> bytes:
        if not self.recording:
            return b""
        self.recording = False
        if self.capture:
            self.capture.stop()
        self.mic_level = 0.0
        self._emit("recording", False)
        return bytes(self._rec)

    def send_voice(self, data: bytes, dest_id: str | None = None) -> Message | None:
        """Голосовое уходит файлом. Всем — с прошивки 4.4.35, без подтверждений:
        итог «разослано», а не «доставлено»."""
        if len(data) < 8 * 50:            # меньше секунды — случайное нажатие
            return None
        msg = Message(text="", outgoing=True, peer_id=dest_id or proto.BROADCAST,
                      peer_name=self.peer_name(dest_id), voice=data)
        self.messages.append(msg)
        self._voice_out = msg
        self._emit("message", msg)
        name = time.strftime("voice_%H%M%S.c2")
        threading.Thread(target=self.send_file, daemon=True,
                         args=(name, data, proto.FileType.VOICE, dest_id)).start()
        return msg

    def play_voice(self, data: bytes):
        """Проиграть запись: кадры уходят в тот же буфер, что и живой эфир."""
        step = proto.AUDIO_PACKET_BYTES
        for off in range(0, len(data) - step + 1, step):
            self.playback.push(data[off:off + step], off + 2 * step > len(data))

    # ---------- голос ----------
    def start_ptt(self):
        if self.ptt_active or not self.link.connected:
            return
        self.ptt_active = True
        self._ptt_started = time.time()
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
        if self.recording:
            if time.time() - self._rec_started >= PTT_MAX_SECONDS:
                self._emit("record_limit", PTT_MAX_SECONDS)
                return
            self._rec.extend(packet)
            return
        # Предел речи проверяем здесь: кадры приходят каждые 80 мс, отдельный
        # таймер ради этого заводить незачем.
        if time.time() - self._ptt_started >= PTT_MAX_SECONDS:
            self._emit("ptt_limit", PTT_MAX_SECONDS)
            self.stop_ptt()
            return
        self.link.send(proto.audio(packet))

    def ptt_seconds_left(self) -> int:
        if not self.ptt_active:
            return 0
        return max(0, int(PTT_MAX_SECONDS - (time.time() - self._ptt_started)))

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
            # Рация могла смениться: сетку сбрасываем на 868 до её ответа.
            self.band = proto.Band()
            self._emit("band", self.band)
            self.link.send(proto.get_fw_version())
            self.request_settings()
            self.scan_peers()
            self.link.send(proto.get_key_state())
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
                          delivered=True, encrypted=event.encrypted,
                          time=time.time() - event.age_sec)
            self.messages.append(msg)
            self._count_unread()
            self._emit("message", msg)

        elif isinstance(event, proto.MessageAck):
            for msg in reversed(self.messages):
                if msg.outgoing and msg.seq == event.seq and not msg.delivered:
                    msg.delivered = not event.stored
                    msg.stored = event.stored
                    self._emit("message", msg)
                    break

        elif isinstance(event, proto.KeyState):
            self.key_fingerprint = event.fingerprint if event.has_key else ""
            if event.hear_plaintext is not None:
                self.hear_plaintext = event.hear_plaintext
            self._emit("key", event)

        elif isinstance(event, proto.ChannelScan):
            self.channel_scan = event
            self._emit("channel_scan", event)

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
            timer = threading.Timer(CALL_RING_SECONDS, self._call_unanswered, args=(event,))
            timer.daemon = True
            timer.start()

        elif isinstance(event, proto.FileProgress):
            if self._rx_file:
                self._rx_file.done, self._rx_file.total = event.done, event.total
                self._emit("transfer", self._rx_file)

        elif isinstance(event, proto.UploadStatus):
            self.upload_status = event
            if self._voice_out and event.final:
                self._voice_out.delivered = event.code == 3
                self._voice_out.stored = event.code == 6   # разослано всем
                self._voice_out.failed = not event.ok
                self._emit("message", self._voice_out)
                self._voice_out = None
            self._emit("upload_status", event)

        elif isinstance(event, proto.IncomingFileHeader):
            self._rx_file = FileTransfer(name=event.name, size=event.size,
                                         incoming=True, peer_id=event.sender_id,
                                         total=event.chunks, file_type=event.file_type)
            self.transfers.append(self._rx_file)
            self._emit("transfer", self._rx_file)

        elif isinstance(event, proto.Band):
            self.band = event
            self._emit("band", event)

        elif isinstance(event, tuple):
            self._handle_named(event[0], event[1])

    def _call_unanswered(self, call: proto.IncomingCall):
        """Вызов так и не приняли и не отклонили — пропущенный."""
        with self._lock:
            if self.incoming_call is call:
                self.incoming_call = None
                self.missed_calls += 1
                self._emit("counters", None)
                self._emit("call", None)

    def _count_unread(self):
        if not self.chat_open:
            self.unread_messages += 1
            self._emit("counters", None)

    def _file_received(self, tr: FileTransfer):
        """Голос — в чат или сразу в динамик, остальное — пользователю на диск."""
        ftype = getattr(tr, "file_type", None)
        if ftype == proto.FileType.VOICE:
            msg = Message(text="", outgoing=False, peer_id=tr.peer_id,
                          peer_name=self.peer_name(tr.peer_id), delivered=True,
                          voice=bytes(tr.data), file_name=tr.name)
            self.messages.append(msg)
            self._count_unread()
            self._emit("message", msg)
        elif ftype == proto.FileType.PTT_VOICE:
            # Адресная передача кнопкой «Говорить» — играем сразу, как эфир
            self.play_voice(bytes(tr.data))
            self._emit("audio_rx", proto.AudioFrame(tr.peer_id, b"", True))
        else:
            self._emit("file_done", tr)

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

        elif kind == "file_data" and self._rx_file:
            self._rx_file.data.extend(payload)
            if len(self._rx_file.data) >= self._rx_file.size:
                self._rx_file.finished = True
                self._file_received(self._rx_file)
                self._rx_file = None

        elif kind == "fw_version":
            self.firmware = payload
            self._emit("firmware", payload)

        elif kind == "crypto_alien":
            self.alien_packets = int(payload)
            self._emit("key", None)

        elif kind == "call_status":
            # 3 — время вышло, 4 — вызов отменён. Если окно вызова ещё ждало
            # ответа, вызов пропущен.
            if payload and payload[0] >= 3:
                if self.incoming_call is not None:
                    self.missed_calls += 1
                    self._emit("counters", None)
                self.incoming_call = None
                self.call_active = False
                self._emit("call", None)

        elif kind == "location_request":
            # Своих координат у настольного клиента нет: отвечаем тем, что
            # задал пользователь в настройках, иначе нулями — устройство ждёт
            # ответа и без него держит запрос.
            lat = self.settings.get("my_lat")
            lon = self.settings.get("my_lon")
            self.link.send(proto.location_update(lat, lon))

        elif kind == "incoming_call":
            pass
