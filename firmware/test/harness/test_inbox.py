#!/usr/bin/env python3
"""Сообщения, пришедшие без телефона, и честная отметка о доставке (4.4.32).

До 4.4.32 нода, к которой не подключён телефон, ставила отправителю галочку
«доставлено», а сам текст выбрасывала. Проверяем:
  1. без телефона адресное сообщение подтверждается как «принято нодой»;
  2. после подключения телефона накопленное приходит, по порядку и с возрастом;
  3. при подключённом телефоне подтверждение — «доставлено», без возраста.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/test_inbox.py
"""

from __future__ import annotations

import asyncio
import os
import struct
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device, discover_ports   # noqa: E402
from ble_phone import Phone, CMD_RECV_MESSAGE   # noqa: E402

results: list[tuple[str, bool, str]] = []


def check(name: str, ok: bool, detail: str = ""):
    results.append((name, ok, detail))
    print(f"  {'✓ OK  ' if ok else '✗ FAIL'} {name}" + (f" — {detail}" if detail else ""))
    return ok


def parse_recv(data: bytes) -> tuple[str, int | None]:
    """Текст и возраст в секундах (None — пакет без хвоста возраста)."""
    end = data.index(0, 2)
    text = data[2:end].decode("utf-8", errors="replace")
    age = struct.unpack_from("<I", data, end + 5)[0] if len(data) >= end + 9 else None
    return text, age


def send_and_ack(peer: Device, dest: str, text: str):
    """Отправить адресное и дождаться подтверждения; эфир общий — до трёх попыток."""
    for _ in range(3):
        peer.drain()
        peer.send_text(dest, text)
        ack = peer.wait("TEXT_ACK_RX", 8.0)
        if ack:
            return ack
    return None


async def main():
    found = discover_ports(2)
    if len(found) < 2:
        print(f"нужны две платы, найдено: {found}")
        return 1
    target = Device(found[1], name="target")   # к ней подключается «телефон»
    peer = Device(found[0], name="peer")       # отправитель
    await asyncio.sleep(0.4)
    st = target.ble_state()
    name = st.get("name")
    target_id = target.info().get("id")
    peer.testmode(True)
    target.testmode(True)
    await asyncio.sleep(1.0)
    print(f"телефон подключается к {name} (ID {target_id})")

    print("\n[1] Телефон не подключён")
    check("у цели нет BLE-клиента", st.get("connected") == "0", st.raw)
    texts = [f"OFFLINE-{i}-{int(time.time()) % 1000}" for i in range(3)]
    for t in texts:
        target.drain()
        ack = send_and_ack(peer, target_id, t)
        check(f"«{t}»: подтверждение «принято нодой»",
              bool(ack and ack.get("status") == "1"),
              ack.raw if ack else "нет ACK")
        await asyncio.sleep(0.5)
    info = target.info()
    check("сообщения лежат в буфере", info.int("inbox") >= len(texts),
          f"inbox={info.get('inbox')} из {info.get('inbox_cap')} байт")

    await asyncio.sleep(3.0)   # чтобы возраст был заметно больше нуля
    print("\n[2] Телефон подключился")
    addrs = {}
    for _ in range(4):                 # V4 объявляет о себе тише V3 — скан может промахнуться
        addrs = await Phone.find(timeout=8.0)
        if name in addrs:
            break
    if name not in addrs:
        check("цель видна в эфире BLE", False, f"найдено: {list(addrs)}")
        return 1
    phone = Phone(addrs[name])
    await phone.connect()
    await phone.request_settings()     # телефон «заговорил» — дальше отдача
    deadline = time.time() + 20
    got: list[tuple[str, int | None]] = []
    while time.time() < deadline and len(got) < len(texts):
        # Своё ищем среди всего пришедшего: в буфере могло остаться чужое от
        # прерванного прогона, и оно законно приходит первым.
        got = [g for g in (parse_recv(d) for _, d in phone.rx if d and d[0] == CMD_RECV_MESSAGE)
               if g[0] in texts]
        await asyncio.sleep(0.3)
    got_texts = [g[0] for g in got]
    check("накопленное дошло до телефона", got_texts[:len(texts)] == texts,
          f"пришло {got_texts}")
    ages = [g[1] for g in got[:len(texts)]]
    check("у накопленного есть возраст", all(a is not None and a >= 3 for a in ages),
          f"возраст, с: {ages}")
    check("буфер опустел", target.info().int("inbox") == 0)

    print("\n[3] Телефон подключён — обычная доставка")
    before = len(phone.rx)
    t = f"ONLINE-{int(time.time()) % 1000}"
    ack = send_and_ack(peer, target_id, t)
    check("подтверждение «доставлено»", bool(ack and ack.get("status") == "0"),
          ack.raw if ack else "нет ACK")
    await asyncio.sleep(1.0)
    fresh = [parse_recv(d) for _, d in phone.rx[before:] if d and d[0] == CMD_RECV_MESSAGE]
    check("сообщение пришло сразу и без возраста",
          any(f[0] == t and f[1] is None for f in fresh), f"{fresh}")
    await phone.disconnect()

    peer.testmode(False)
    target.testmode(False)
    fails = [r for r in results if not r[1]]
    print(f"\nИтого: {len(results) - len(fails)}/{len(results)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
