#!/usr/bin/env python3
"""Фаззинг приёмной стороны: мусор и злонамеренно составленные пакеты в эфир.

В диапазоне 868 МГц полно чужого трафика, и приёмник обязан переживать любой
мусор: не падать, не зависать, не течь памятью. Генерация — на стороне ПК
(воспроизводимо по seed), прошивке нужна лишь команда TX RAW.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/fuzz.py [--rounds 300] [--seed 1]
"""

from __future__ import annotations

import argparse
import os
import random
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device, discover_ports  # noqa: E402

TYPES = [0xA0, 0xB0, 0xB1, 0xC0, 0xC1, 0xC2, 0xC3, 0xD0,
         0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6]

# Целевые размеры структур (packet.h) — чтобы бить именно по границам
STRUCT_LEN = {0xA0: 39, 0xB0: 93, 0xB1: 7, 0xC0: 35, 0xC1: 128,
              0xC2: 107, 0xC3: 5, 0xD0: 36, 0xE0: 24, 0xE1: 29,
              0xE2: 0, 0xE3: 30, 0xE4: 8, 0xE5: 8, 0xE6: 8}


def gen_case(rnd: random.Random, my_id: bytes) -> tuple[str, bytes]:
    """Вернуть (описание, байты) очередного тестового пакета."""
    kind = rnd.randrange(8)

    if kind == 0:                                    # чистый шум
        n = rnd.randrange(1, 200)
        return "случайные байты", bytes(rnd.randrange(256) for _ in range(n))

    t = rnd.choice(TYPES)
    full = STRUCT_LEN.get(t, 32) or 32

    if kind == 1:                                    # обрезанный пакет
        n = rnd.randrange(1, max(2, full))
        return f"обрезанный {t:02X}", bytes([t]) + bytes(rnd.randrange(256) for _ in range(n - 1))

    if kind == 2:                                    # раздутый пакет
        n = min(222, full + rnd.randrange(1, 100))
        return f"раздутый {t:02X}", bytes([t]) + bytes(rnd.randrange(256) for _ in range(n - 1))

    if kind == 3:                                    # верная длина, мусорное тело
        body = bytes(rnd.randrange(256) for _ in range(full - 1))
        return f"мусорное тело {t:02X}", bytes([t]) + body

    if kind == 4:                                    # заголовок файла с абсурдными размерами
        size = rnd.choice([0, 1, 0xFFFFFFFF, 0x7FFFFFFF, 200_000, 65535])
        chunks = rnd.choice([0, 1, 65535, 1023, 1024])
        pkt = bytearray(35)
        pkt[0] = 0xC0
        pkt[1] = 22                                   # канал
        pkt[2] = rnd.randrange(256)                   # session
        pkt[3] = 2                                    # ttl
        pkt[4:6] = bytes([0xAA, 0xBB])                # sender
        pkt[6:8] = my_id                              # dest = получатель
        pkt[8] = rnd.choice([1, 2, 3, 4, 5, 99])      # file_type
        pkt[9:11] = chunks.to_bytes(2, "little")
        pkt[11:15] = size.to_bytes(4, "little")
        pkt[15:35] = b"fuzz" + bytes(16)
        return f"FILE_START size={size} chunks={chunks}", bytes(pkt)

    if kind == 5:                                    # чанк с индексом вне диапазона
        idx = rnd.choice([0, 1, 1023, 1024, 65535, 32768])
        pkt = bytearray(8 + rnd.randrange(1, 121))
        pkt[0] = 0xC1
        pkt[1] = 22
        pkt[2] = rnd.randrange(256)
        pkt[3] = 2
        pkt[4:6] = my_id                              # dest
        pkt[6:8] = idx.to_bytes(2, "little")
        return f"FILE_CHUNK idx={idx}", bytes(pkt)

    if kind == 6:                                    # ACK/NACK с абсурдным missing_count
        cnt = rnd.choice([0, 1, 50, 51, 1000, 65535])
        pkt = bytearray(7 + min(cnt, 50) * 2)
        pkt[0] = 0xC2
        pkt[1] = rnd.randrange(256)
        pkt[2] = rnd.choice([0, 1, 99])
        pkt[3:5] = my_id                              # dest
        pkt[5:7] = cnt.to_bytes(2, "little")
        return f"FILE_ACK missing_count={cnt}", bytes(pkt)

    # kind == 7: текст без нуль-терминатора, полный буфер
    pkt = bytearray(93)
    pkt[0] = 0xB0
    pkt[1] = 22
    pkt[2] = rnd.randrange(256)
    pkt[3] = 2
    pkt[4:6] = bytes([0xAA, 0xBB])
    pkt[6:8] = rnd.choice([my_id, b"\x00\x00"])
    for i in range(8, 93):
        pkt[i] = rnd.randrange(1, 256)                # ни одного нуля
    return "TEXT без терминатора", bytes(pkt)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rounds", type=int, default=200)
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--ports", default="",
                    help="через запятую; по умолчанию — найденные ttyUSB/ttyACM")
    args = ap.parse_args()

    ports = ([x.strip() for x in args.ports.split(",")]
             if args.ports else discover_ports(2))
    if len(ports) < 2:
        print(f"нужны две платы, найдено: {ports}")
        return 1
    pa, pb = ports
    A = Device(pa, name="fuzzer")     # отправитель мусора
    B = Device(pb, name="target")     # цель
    rnd = random.Random(args.seed)

    try:
        time.sleep(0.4)
        A.testmode(True); B.testmode(True)
        info = B.info()
        my_id = bytes([int(info.get("id")[2:4], 16), int(info.get("id")[0:2], 16)])
        boot0 = info.int("boot")
        heap0 = info.int("heap")
        print(f"цель: {info.get('name')} boot={boot0} heap={heap0}")
        print(f"фаззинг: {args.rounds} пакетов, seed={args.seed}\n")

        crashed = []
        for i in range(args.rounds):
            desc, data = gen_case(rnd, my_id)
            A.send("TX RAW " + data.hex())
            A.wait("TX_RAW", 5.0)
            time.sleep(0.12)

            if i % 25 == 24 or i == args.rounds - 1:
                st = B.info()
                if st is None:
                    print(f"  [{i+1:4d}] ЦЕЛЬ НЕ ОТВЕЧАЕТ после «{desc}»")
                    crashed.append((i, desc, "нет ответа"))
                    time.sleep(3)
                    continue
                boot, heap = st.int("boot"), st.int("heap")
                mark = ""
                if boot != boot0:
                    mark = f"  ПЕРЕЗАГРУЗКА! boot {boot0}→{boot}"
                    crashed.append((i, desc, f"reboot {boot0}->{boot}"))
                    boot0 = boot
                elif heap < heap0 - 20000:
                    mark = f"  память просела: {heap0}→{heap}"
                    crashed.append((i, desc, f"heap {heap0}->{heap}"))
                print(f"  [{i+1:4d}] отправлено, цель жива: heap={heap} "
                      f"file_state={st.get('file_state')}{mark}")

        print("\n" + "=" * 60)
        st = B.info()
        if st is None:
            print("ИТОГ: цель не отвечает — падение")
            return 1
        print(f"ИТОГ: отправлено {args.rounds} пакетов, "
              f"перезагрузок {len([c for c in crashed if 'reboot' in c[2]])}, "
              f"проблем {len(crashed)}")
        print(f"состояние цели: boot={st.int('boot')} heap={st.int('heap')} "
              f"min_heap={st.int('min_heap')} uptime={st.int('uptime')} с")
        if crashed:
            print("\nПроблемные случаи:")
            for i, desc, what in crashed:
                print(f"  пакет #{i+1}: {desc} → {what}")
        return 1 if crashed else 0
    finally:
        A.testmode(False); B.testmode(False)
        A.close(); B.close()


if __name__ == "__main__":
    sys.exit(main())
