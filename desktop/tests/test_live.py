#!/usr/bin/env python3
"""Проверка настольного клиента на живой паре: клиент ↔ устройство ↔ устройство.

Слева — компьютер с клиентом, подключённый по BLE к своей плате. Справа —
вторая плата с тестовой консолью, она играет роль корреспондента в эфире.
Так проверяется весь путь целиком: интерфейсное ядро, BLE, радио и обратно.

Запуск (из корня репозитория):
    PYTHONPATH=firmware/test/harness/vendor \
        .venv-desktop/bin/python desktop/tests/test_live.py [--only text,voice]

Блоки: link, text, voice, calls, beacon, files, settings, reconnect, load.
"""

from __future__ import annotations

import argparse
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "desktop"))
sys.path.insert(0, os.path.join(ROOT, "firmware/test/harness"))

import numpy as np                                          # noqa: E402
from device import Device as Board                          # noqa: E402
from meshtrx_desktop import protocol as proto               # noqa: E402
from meshtrx_desktop.client import Client                   # noqa: E402
from meshtrx_desktop.codec2 import Codec2, PACKET_SAMPLES   # noqa: E402

CHANNEL = 22
BOARD_POWER = 9          # плата-корреспондент: платы стоят на столе
CLIENT_POWER = 14        # мощность устройства клиента задаём через сам клиент

results: list[tuple[str, bool, str]] = []


def check(name: str, ok: bool, detail: str = "") -> bool:
    results.append((name, ok, detail))
    print(f"  {'✓ OK  ' if ok else '✗ FAIL'} {name}" + (f" — {detail}" if detail else ""))
    return ok


def wait_until(predicate, timeout: float, step: float = 0.2) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(step)
    return False


class Collector:
    """Считает события клиента — тесту нужен не факт «что-то пришло», а сколько."""

    def __init__(self, client: Client):
        self.counts: dict[str, int] = {}
        self.last: dict[str, object] = {}
        client.subscribe(self._on)

    def _on(self, event: str, payload: object):
        self.counts[event] = self.counts.get(event, 0) + 1
        self.last[event] = payload

    def count(self, event: str) -> int:
        return self.counts.get(event, 0)

    def reset(self, event: str | None = None):
        if event:
            self.counts.pop(event, None)
        else:
            self.counts.clear()


# ---------------------------------------------------------------- блоки

def test_link(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[1] Связь клиента с устройством")
    check("устройство подключено", client.link.connected)
    check("настройки прочитаны", bool(client.settings),
          ", ".join(f"{k}={v}" for k, v in list(client.settings.items())[:3]))
    ok = wait_until(lambda: col.count("status") > 0, 20)
    check("устройство шлёт статус", ok,
          f"канал {client.channel}, сигнал {client.rssi} дБм" if ok else "нет статуса за 20 с")


def test_text(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[2] Текстовые сообщения")
    board.drain()
    client.send_message("desktop-to-air", peer_addr)
    ev = board.wait("TEXT_RX", 12.0)
    check("адресное сообщение клиента дошло до платы", ev is not None,
          f"len={ev.get('len')} crc={ev.get('crc')}" if ev else "не принято")

    ack = wait_until(lambda: any(m.outgoing and m.delivered for m in client.messages), 8)
    check("подтверждение доставки вернулось клиенту", ack)

    before = len(client.messages)
    board.send_text(client.own_id, "air-to-desktop")
    got = wait_until(lambda: len(client.messages) > before, 12)
    check("сообщение из эфира дошло до клиента", got,
          client.messages[-1].text if got else "не принято")

    before = len(client.messages)
    board.send_text("BCAST", "broadcast-check")
    got = wait_until(lambda: len(client.messages) > before, 12)
    check("широковещательное сообщение принято", got)

    board.drain()
    client.send_message("Кириллица и знаки: №«»—", peer_addr)
    ev = board.wait("TEXT_RX", 12.0)
    if ev:
        text = bytes.fromhex(ev.get("hex", "")).decode("utf-8", "replace")
        check("текст не искажается", text == "Кириллица и знаки: №«»—", text)
    else:
        check("текст не искажается", False, "сообщение не дошло")


def test_voice(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[3] Голос")
    codec = Codec2()
    tone = (7000 * np.sin(2 * np.pi * 440 * np.arange(PACKET_SAMPLES) / 8000)).astype("<i2")
    packet = codec.encode_packet(tone.tobytes())

    board.rx_reset(); board.drain()
    client.link.send(proto.ptt(True))
    time.sleep(0.3)
    for _ in range(30):
        client.link.send(proto.audio(packet))
        time.sleep(0.085)
    client.link.send(proto.ptt(False))
    time.sleep(3)
    _stats, types = board.rx_stats()
    got = types.get("A0", 0)
    check("голос клиента дошёл до платы", got >= 24, f"{got} из 30 пакетов")

    col.reset("audio_rx")
    board.send_audio(30, 80)
    time.sleep(6)
    received = col.count("audio_rx")
    check("голос из эфира дошёл до клиента", received >= 27, f"{received} из 30 пакетов")
    codec.close()


def test_calls(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[4] Вызовы")
    col.reset("call")
    board.call("all")
    got = wait_until(lambda: client.incoming_call is not None, 12)
    check("входящий вызов виден клиенту", got,
          f"от {client.incoming_call.call_sign or client.incoming_call.sender_id}"
          if got else "вызов не пришёл")
    if got:
        board.drain()
        client.accept_call()
        ev = board.wait("LORA_RX", 10.0, type="E4")
        check("принятие вызова дошло до платы", ev is not None)
        client.call_active = False

    # Прошивка не начинает новый вызов, пока не закрыт предыдущий, поэтому
    # сперва снимаем принятый выше.
    client.cancel_call()
    time.sleep(2)
    board.drain()
    client.call("all")
    ev = board.wait("LORA_RX", 12.0, type="E0")
    check("вызов клиента ушёл в эфир", ev is not None)
    time.sleep(1)
    client.cancel_call()
    time.sleep(1.5)

    # Тревожный вызов убран из проекта; проверяем отклонение обычного
    board.call("all")
    wait_until(lambda: client.incoming_call is not None, 12)
    if client.incoming_call:
        board.drain()
        client.reject_call()
        ev = board.wait("LORA_RX", 10.0, type="E5")
        check("отклонение вызова дошло до платы", ev is not None)


def test_beacon(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[5] Маяки и список абонентов")
    client.peers.clear(); client.peer_seen.clear()
    board.send_beacon()
    got = wait_until(lambda: len(client.active_peers()) > 0, 15)
    check("абонент появился в списке", got,
          ", ".join(f"{p.call_sign or p.device_id} {p.rssi} дБм"
                    for p in client.active_peers()) if got else "маяк не принят")
    if got:
        peer = client.active_peers()[0]
        check("адрес абонента пригоден для отправки",
              peer.device_id[-4:].upper() == peer_addr.upper(),
              f"из маяка {peer.device_id[-4:]}, ожидали {peer_addr}")


def test_files(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[6] Файлы")
    payload = bytes((i * 31 + 7) & 0xFF for i in range(1024))
    # Устройство могло остаться занятым после предыдущего блока — дадим ему
    # закончить, иначе проверяли бы не файлы, а чужой таймаут.
    wait_until(lambda: client.settings is not None, 1)
    time.sleep(3)
    board.drain()
    client.send_file("probe.bin", payload, proto.FileType.BINARY, peer_addr)
    status = client.upload_status
    check("устройство приняло файл на загрузку",
          status is not None and status.accepted,
          status.text if status else "устройство не ответило на заголовок")
    ev = board.wait("FILE_RX_START", 40.0)
    check("плата начала приём файла от клиента", ev is not None,
          f"размер {ev.get('size')} Б, чанков {ev.get('chunks')}" if ev else "заголовок не пришёл")
    if ev:
        done = board.wait("FILE_RX", 90.0)
        check("файл дошёл целиком",
              done is not None and done.get("pattern_ok") != "0",
              f"{done.get('size')} Б, чанков {done.get('chunks')}, "
              f"расхождений {done.get('bad')}" if done else "не завершился")


def test_settings(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[7] Настройки устройства через клиента")
    base_channel = client.channel
    target = 5 if base_channel != 5 else 7
    client.set_channel(target)
    time.sleep(2)
    ok = wait_until(lambda: client.channel == target, 10)
    check("канал сменился", ok, f"{base_channel} → {client.channel}")

    # На другом канале плата не должна нас слышать — это и подтверждает смену
    board.drain()
    client.send_message("wrong-channel", peer_addr)
    ev = board.wait("TEXT_RX", 6.0)
    check("на другом канале плата не слышит", ev is None,
          "приняла, хотя не должна" if ev else "тишина, как и ожидалось")

    client.set_channel(base_channel)
    time.sleep(2)
    board.drain()
    client.send_message("back-on-channel", peer_addr)
    ev = board.wait("TEXT_RX", 12.0)
    check("после возврата канала связь восстановилась", ev is not None)

    client.apply_settings(tx_power=CLIENT_POWER)
    time.sleep(1.5)
    client.request_settings()
    ok = wait_until(lambda: client.settings.get("tx_power") == CLIENT_POWER, 8)
    check("мощность применилась", ok, f"tx_power={client.settings.get('tx_power')}")


def test_reconnect(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[8] Переподключение")
    address = client.device.address
    times = []
    for i in range(3):
        client.disconnect()
        ok = wait_until(lambda: not client.link.connected, 10)
        if not ok:
            check(f"цикл {i + 1}: отключение", False, "устройство не отпустило соединение")
            continue
        time.sleep(1.0)
        t0 = time.time()
        client.link.connect(address)
        ok = wait_until(lambda: client.link.connected, 40)
        times.append(time.time() - t0)
        if not ok:
            check(f"цикл {i + 1}: подключение", False, "не подключились за 40 с")
            return
    check("три цикла переподключения", len(times) == 3,
          f"время: {', '.join(f'{t:.1f} с' for t in times)}" if times else "нет успешных")

    time.sleep(2)
    board.drain()
    client.send_message("after-reconnect", peer_addr)
    ev = board.wait("TEXT_RX", 12.0)
    check("после переподключения связь работает", ev is not None)


def test_load(client: Client, board: Board, col: Collector, peer_addr: str):
    print("\n[9] Клиент под встречным трафиком")
    # Интервал выбран так, чтобы эфир был занят, но не забит наглухо: при
    # полудуплексе плата, которая непрерывно передаёт, физически не может
    # ничего принять, и тест проверял бы законы природы, а не клиента.
    board.load_start("mixed", 1500)
    time.sleep(1)
    col.reset("message"); col.reset("audio_rx")
    delivered = 0
    for i in range(5):
        board.drain()
        client.send_message(f"under-load-{i}", peer_addr)
        if board.wait("TEXT_RX", 12.0):
            delivered += 1
        time.sleep(0.8)
    board.load_stop()
    check("сообщения проходят под нагрузкой", delivered >= 3, f"{delivered} из 5")
    check("клиент принимал встречный трафик", col.count("message") > 0,
          f"принято {col.count('message')} сообщений")


BLOCKS = {
    "link": test_link, "text": test_text, "voice": test_voice, "calls": test_calls,
    "beacon": test_beacon, "files": test_files, "settings": test_settings,
    "reconnect": test_reconnect, "load": test_load,
}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--board", default="", help="порт платы-корреспондента")
    ap.add_argument("--device", default="", help="имя BLE-устройства клиента")
    ap.add_argument("--only", default="", help=",".join(BLOCKS))
    args = ap.parse_args()

    from device import discover_ports
    port = args.board or (discover_ports(2)[-1] if discover_ports(2) else None)
    if not port:
        print("не найден порт платы-корреспондента")
        return 1

    board = Board(port, name="board")
    time.sleep(1.2)
    board.set_channel(CHANNEL)
    board.testmode(True)
    board.set_power(BOARD_POWER)
    time.sleep(1)
    info = board.info()
    print(f"плата-корреспондент: {info.get('name')} канал {info.get('ch')} "
          f"мощность {info.get('pwr')} дБм на {port}")
    # Адрес платы в том порядке байтов, в каком его ждёт эфир
    board_id = info.get("id")
    peer_addr = board_id[2:4] + board_id[0:2]

    client = Client()
    client.start()
    found = client.scan(8)
    print(f"BLE: {[d.name for d in found]}")
    target = next((d for d in found if args.device in d.name), None) if args.device \
        else (found[0] if found else None)
    if target is None:
        print("устройство клиента не найдено по BLE")
        board.close(); client.stop()
        return 1
    print(f"клиент подключается к {target.name}")
    client.connect(target)
    if not wait_until(lambda: client.link.connected, 40):
        print("не удалось подключиться")
        board.close(); client.stop()
        return 1
    time.sleep(2.5)

    # Свой адрес для платы: в имени устройства байты идут в обратном порядке
    # относительно того, как их читает консольная команда TX TEXT.
    name_tail = target.name[-4:]
    client.own_id = name_tail[2:4] + name_tail[0:2]
    client.set_channel(CHANNEL)
    client.apply_settings(tx_power=CLIENT_POWER)
    time.sleep(2)

    col = Collector(client)
    only = {x.strip() for x in args.only.split(",") if x.strip()}
    t0 = time.time()
    try:
        for name, func in BLOCKS.items():
            if only and name not in only:
                continue
            func(client, board, col, peer_addr)
    finally:
        board.load_stop()
        board.testmode(False)
        board.close()
        client.disconnect()
        time.sleep(1.5)
        client.stop()

    passed = sum(1 for _, ok, _ in results if ok)
    print("\n" + "=" * 60)
    print(f"ИТОГО: {passed}/{len(results)} проверок за {time.time() - t0:.0f} с")
    for name, ok, detail in results:
        if not ok:
            print(f"  ✗ {name} — {detail}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(main())
