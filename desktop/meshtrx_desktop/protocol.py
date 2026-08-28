"""Протокол MeshTRX поверх BLE (Nordic UART Service).

Одно место, где описаны форматы пакетов между клиентом и устройством. Раньше
они существовали дважды — в прошивке и в Android-приложении, — и любое
расхождение вылезало уже в эфире. Здесь только разбор и сборка байтов:
ни ввода-вывода, ни состояния, чтобы форматы можно было проверять тестами.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from enum import IntEnum

SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
RX_CHAR = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"   # клиент → устройство
TX_CHAR = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   # устройство → клиент


class Cmd(IntEnum):
    AUDIO_TX = 0x01
    AUDIO_RX = 0x02
    PTT_START = 0x03
    PTT_END = 0x04
    SET_CHANNEL = 0x05
    STATUS_UPDATE = 0x06
    SEND_MESSAGE = 0x07
    RECV_MESSAGE = 0x08
    MESSAGE_ACK = 0x09
    SET_SETTINGS = 0x0A
    GET_SETTINGS = 0x0B
    SETTINGS_RESP = 0x0C
    FILE_START = 0x0D
    FILE_CHUNK = 0x0E
    FILE_RECV = 0x0F
    FILE_PROGRESS = 0x10
    SET_TX_MODE = 0x11
    VOX_STATUS = 0x12
    VOX_LEVEL = 0x13
    GET_LOCATION = 0x14
    LOCATION_UPD = 0x15
    BEACON_SENT = 0x16
    PEER_SEEN = 0x17
    CALL_ALL = 0x18
    CALL_PRIVATE = 0x19
    CALL_GROUP = 0x1A
    CALL_EMERGENCY = 0x1B   # оставлен в таблице ради старых прошивок; не шлём
    CALL_ACCEPT = 0x1C
    CALL_REJECT = 0x1D
    CALL_CANCEL = 0x1E
    INCOMING_CALL = 0x1F
    CALL_STATUS = 0x20
    PIN_CHECK = 0x25
    PIN_RESULT = 0x26
    FILE_DATA = 0x27
    SET_REPEATER = 0x28
    FILE_END = 0x29
    FILE_UPLOAD_START = 0x30
    FILE_UPLOAD_DATA = 0x31
    FILE_UPLOAD_STATUS = 0x32
    SCAN_PEERS = 0x33


class CallType(IntEnum):
    ALL = 0
    PRIVATE = 1
    GROUP = 2
    EMERGENCY = 3      # приходит только от устройств со старой прошивкой


class FileType(IntEnum):
    PHOTO = 1
    TEXT = 2
    BINARY = 3
    VOICE = 4
    PTT_VOICE = 5


PKT_FLAG_PTT_END = 0x02          # последний кадр голосовой посылки
AUDIO_PACKET_BYTES = 32          # Codec2 3200: четыре кадра по 8 байт
BROADCAST = "0000"


# ---------- разобранные уведомления ----------

@dataclass
class Status:
    channel: int
    rssi: int
    snr: int
    battery: float | None


@dataclass
class IncomingMessage:
    text: str
    sender_id: str
    rssi: int


@dataclass
class AudioFrame:
    sender_id: str
    payload: bytes
    is_last: bool


@dataclass
class Peer:
    device_id: str
    call_sign: str
    rssi: int
    snr: int
    tx_power: int
    battery: int | None
    lat: float | None = None
    lon: float | None = None


@dataclass
class IncomingCall:
    call_type: CallType
    sender_id: str
    call_sign: str
    seq: int
    lat: float | None = None
    lon: float | None = None


@dataclass
class FileProgress:
    done: int
    total: int
    percent: int = field(init=False)

    def __post_init__(self):
        self.percent = int(100 * self.done / self.total) if self.total else 0


UPLOAD_STATUS = {
    0: "принято",
    1: "устройство занято другой передачей",
    2: "передаётся в эфир",
    3: "доставлено",
    4: "не доставлено",
    5: "не хватает памяти устройства",
}


@dataclass
class UploadStatus:
    """Ответ устройства на загрузку файла. Без него отказ выглядел молчанием:
    клиент отправлял заголовок и ждал у моря погоды."""
    code: int
    session: int

    @property
    def text(self) -> str:
        return UPLOAD_STATUS.get(self.code, f"код {self.code}")

    @property
    def accepted(self) -> bool:
        return self.code in (0, 2, 3)


@dataclass
class IncomingFileHeader:
    file_type: int
    size: int
    chunks: int
    sender_id: str
    name: str


# ---------- вспомогательное ----------

def _hex_id(data: bytes) -> str:
    return "".join(f"{b:02X}" for b in data)


def _cstr(data: bytes) -> str:
    return data.split(b"\x00", 1)[0].decode("utf-8", "replace")


def dest_bytes(dest_id: str | None) -> bytes:
    """Адрес получателя двумя байтами; None или пусто — широковещательно."""
    if not dest_id or len(dest_id) < 4:
        return b"\x00\x00"
    return bytes([int(dest_id[0:2], 16), int(dest_id[2:4], 16)])


# ---------- сборка команд клиента ----------

def ptt(on: bool) -> bytes:
    return bytes([Cmd.PTT_START if on else Cmd.PTT_END])


def audio(payload: bytes) -> bytes:
    return bytes([Cmd.AUDIO_TX]) + payload


def set_channel(ch: int) -> bytes:
    return bytes([Cmd.SET_CHANNEL, ch & 0xFF])


def send_message(seq: int, text: str, dest_id: str | None = None) -> bytes:
    return bytes([Cmd.SEND_MESSAGE, seq & 0xFF]) + dest_bytes(dest_id) + text.encode()


def get_settings() -> bytes:
    return bytes([Cmd.GET_SETTINGS])


def set_settings(json_text: str) -> bytes:
    return bytes([Cmd.SET_SETTINGS]) + json_text.encode()


def pin_check(pin: int) -> bytes:
    return bytes([Cmd.PIN_CHECK]) + struct.pack("<I", pin)


def location_update(lat: float | None, lon: float | None, alt_m: int = 0) -> bytes:
    lat_e7 = int(lat * 1e7) if lat is not None else 0
    lon_e7 = int(lon * 1e7) if lon is not None else 0
    return bytes([Cmd.LOCATION_UPD]) + struct.pack("<iih", lat_e7, lon_e7, alt_m)


def call_all() -> bytes:
    return bytes([Cmd.CALL_ALL])


def call_private(device_id: str) -> bytes:
    """device_id — восемь hex-символов (полный идентификатор устройства)."""
    raw = bytes.fromhex(device_id[:8].ljust(8, "0"))
    return bytes([Cmd.CALL_PRIVATE]) + raw


def call_accept(seq: int) -> bytes:
    return bytes([Cmd.CALL_ACCEPT, seq & 0xFF])


def call_reject(seq: int) -> bytes:
    return bytes([Cmd.CALL_REJECT, seq & 0xFF])


def call_cancel() -> bytes:
    return bytes([Cmd.CALL_CANCEL])


def scan_peers() -> bytes:
    """Попросить соседей отозваться маяками.

    Штатный маяк уходит раз в несколько минут, поэтому сразу после запуска
    список абонентов пуст и человек считает, что связь не работает.
    """
    return bytes([Cmd.SCAN_PEERS])


def set_repeater(enabled: bool) -> bytes:
    return bytes([Cmd.SET_REPEATER, 1 if enabled else 0])


def file_upload_start(name: str, file_type: int, size: int,
                      dest_id: str | None = None) -> bytes:
    """Заголовок загрузки: cmd + тип + адрес + размер + имя, ровно 28 байт.

    Порядок полей здесь не произволен — прошивка читает адрес до размера, и
    перестановка приводит к молчаливому отказу: устройство просто не начинает
    приём, ничего об этом не сообщая.
    """
    name_b = name.encode()[:19].ljust(20, b"\x00")
    return (bytes([Cmd.FILE_UPLOAD_START, file_type & 0xFF])
            + dest_bytes(dest_id) + struct.pack("<I", size) + name_b)


def file_upload_data(chunk: bytes) -> bytes:
    return bytes([Cmd.FILE_UPLOAD_DATA]) + chunk


# ---------- разбор уведомлений устройства ----------

def parse(data: bytes):
    """Вернуть разобранное уведомление или (cmd, raw) для того, что не разбираем."""
    if not data:
        return None
    cmd = data[0]

    if cmd == Cmd.STATUS_UPDATE and len(data) >= 5:
        rssi = struct.unpack_from("<h", data, 2)[0]
        bat = data[5] / 10.0 if len(data) >= 6 and data[5] else None
        return Status(channel=data[1], rssi=rssi, snr=struct.unpack_from("b", data, 4)[0],
                      battery=bat)

    if cmd == Cmd.RECV_MESSAGE and len(data) >= 4:
        end = data.find(b"\x00", 2)
        if end < 0:
            end = len(data)
        text = data[2:end].decode("utf-8", "replace")
        sender = _hex_id(data[end + 1:end + 3]) if end + 2 < len(data) else "??"
        return IncomingMessage(text=text, sender_id=sender,
                               rssi=struct.unpack_from("b", data, 1)[0])

    if cmd == Cmd.AUDIO_RX and len(data) >= 4 + AUDIO_PACKET_BYTES:
        return AudioFrame(sender_id=_hex_id(data[2:4]),
                          payload=bytes(data[4:4 + AUDIO_PACKET_BYTES]),
                          is_last=bool(data[1] & PKT_FLAG_PTT_END))

    if cmd == Cmd.PEER_SEEN and len(data) >= 28:
        lat_e7, lon_e7 = struct.unpack_from("<ii", data, 14)
        rssi = struct.unpack_from("<h", data, 22)[0]
        return Peer(device_id=_hex_id(data[1:5]),
                    call_sign=_cstr(data[5:14]),
                    rssi=rssi,
                    snr=struct.unpack_from("b", data, 24)[0],
                    tx_power=data[25],
                    battery=None if data[26] == 0xFF else data[26],
                    lat=lat_e7 / 1e7 if (lat_e7 or lon_e7) else None,
                    lon=lon_e7 / 1e7 if (lat_e7 or lon_e7) else None)

    if cmd == Cmd.INCOMING_CALL and len(data) >= 24:
        lat_e7, lon_e7 = struct.unpack_from("<ii", data, 15)
        return IncomingCall(call_type=CallType(data[1]) if data[1] in
                            CallType._value2member_map_ else CallType.ALL,
                            sender_id=_hex_id(data[2:6]),
                            call_sign=_cstr(data[6:15]),
                            seq=data[23],
                            lat=lat_e7 / 1e7 if (lat_e7 or lon_e7) else None,
                            lon=lon_e7 / 1e7 if (lat_e7 or lon_e7) else None)

    if cmd == Cmd.SETTINGS_RESP:
        return ("settings", data[1:].decode("utf-8", "replace"))

    if cmd == Cmd.PIN_RESULT and len(data) >= 2:
        return ("pin", bool(data[1]))

    if cmd == Cmd.MESSAGE_ACK and len(data) >= 2:
        return ("message_ack", data[1])

    if cmd == Cmd.FILE_PROGRESS and len(data) >= 6:
        done, total = struct.unpack_from("<HH", data, 2)
        return FileProgress(done=done, total=total)

    if cmd == Cmd.FILE_RECV and len(data) >= 29:
        size = struct.unpack_from("<I", data, 2)[0]
        return IncomingFileHeader(file_type=data[1], size=size, chunks=data[6],
                                  sender_id=_hex_id(data[7:9]),
                                  name=_cstr(data[9:29]))

    if cmd == Cmd.FILE_UPLOAD_STATUS and len(data) >= 3:
        return UploadStatus(code=data[1], session=data[2])

    if cmd == Cmd.FILE_DATA:
        return ("file_data", bytes(data[1:]))

    if cmd == Cmd.GET_LOCATION:
        return ("location_request", None)

    return (Cmd(cmd).name.lower() if cmd in Cmd._value2member_map_ else f"raw_{cmd:02X}",
            bytes(data[1:]))
