#!/usr/bin/env python3
"""BLE-тесты: подключение, PIN, устойчивость соединения, поток уведомлений.

Проверяет ровно тот путь, на котором связь и ломалась: телефон ↔ устройство.
Роль телефона играет ноутбук (bleak), второе устройство на UART создаёт
LoRa-трафик, который должен доходить до «телефона» уведомлениями.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/test_ble.py
    ... [--cycles 20] [--target /dev/ttyUSB1] [--peer /dev/ttyUSB0]
"""

from __future__ import annotations

import argparse
import asyncio
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device, discover_ports   # noqa: E402
from ble_phone import (Phone, CMD_AUDIO_RX, CMD_PEER_SEEN,      # noqa: E402
                       CMD_RECV_MESSAGE, CMD_STATUS_UPDATE)

results: list[tuple[str, bool, str]] = []


def check(name: str, ok: bool, detail: str = ""):
    results.append((name, ok, detail))
    print(f"  {'✓ OK  ' if ok else '✗ FAIL'} {name}" + (f" — {detail}" if detail else ""))
    return ok


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="", help="устройство, к которому подключается телефон")
    ap.add_argument("--peer", default="", help="второе устройство — источник LoRa-трафика")
    ap.add_argument("--cycles", type=int, default=20)
    args = ap.parse_args()

    found = discover_ports(2)
    if not (args.target and args.peer) and len(found) < 2:
        print(f"нужны две платы, найдено: {found}")
        return 1
    target = Device(args.target or found[1], name="target")
    peer = Device(args.peer or found[0], name="peer")
    t_all = time.time()

    try:
        await asyncio.sleep(0.4)
        st = target.ble_state()
        info = target.info()
        pin = int(st.get("pin"))
        name = st.get("name")
        boot0 = info.int("boot")
        # Адрес цели берём из неё самой: захардкоженный ID молча превращает
        # адресное сообщение в чужое, и тест «путь до телефона» врёт.
        target_id = info.get("id")
        print(f"цель: {name}, ID {target_id}, PIN {pin:04d}, boot={boot0}")

        addrs = await Phone.find()
        if name not in addrs:
            check("устройство видно в эфире BLE", False, f"найдено: {list(addrs)}")
            return 1
        check("устройство видно в эфире BLE", True, f"{name} @ {addrs[name]}")
        phone = Phone(addrs[name])

        # --- 1. Подключение и авторизация ---
        print("\n[1] Подключение и PIN")
        dt = await phone.connect()
        check("подключение установлено", phone.connected, f"{dt:.1f} с")
        ok = await phone.submit_pin(pin)
        check("PIN принят", ok)
        st = target.ble_state()
        check("устройство видит соединение", st and st.get("connected") == "1")

        # --- 2. Обмен данными ---
        print("\n[2] Обмен по BLE")
        settings = await phone.request_settings()
        check("настройки получены", settings is not None,
              settings[1:60].decode(errors="replace") if settings else "нет ответа")

        upd = await phone.wait_cmd(CMD_STATUS_UPDATE, 15.0)
        check("статус приходит периодически", upd is not None,
              f"канал {upd[1]}, батарея {upd[5] / 10:.1f} В" if upd and len(upd) >= 6 else "нет")

        # трафик по радио от второго устройства должен дойти до «телефона»
        peer.testmode(True)
        await asyncio.sleep(1.0)      # дать радио второго устройства встать в приём
        before = phone.count_cmd(CMD_RECV_MESSAGE)
        for attempt in range(3):      # эфир общий, одиночная потеря — не сбой пути
            peer.send_text(target_id, "BLE-PATH")
            await asyncio.sleep(2.5)
            if phone.count_cmd(CMD_RECV_MESSAGE) > before:
                break
        check("сообщение из эфира дошло до телефона",
              phone.count_cmd(CMD_RECV_MESSAGE) > before,
              f"получено {phone.count_cmd(CMD_RECV_MESSAGE) - before} шт "
              f"за {attempt + 1} попыт.")

        before = phone.count_cmd(CMD_PEER_SEEN)
        peer.send_beacon()
        await asyncio.sleep(3.0)
        check("beacon дошёл до телефона как PEER_SEEN",
              phone.count_cmd(CMD_PEER_SEEN) > before)

        # голосовой поток: 30 пакетов по радио → notify
        before = phone.count_cmd(CMD_AUDIO_RX)
        peer.send_audio(30, 80)
        await asyncio.sleep(3.0)
        got = phone.count_cmd(CMD_AUDIO_RX) - before
        check("голосовой поток доходит до телефона", got >= 27,
              f"{got}/30 пакетов")

        # --- 3. Устойчивость: циклы подключения ---
        print(f"\n[3] Устойчивость: {args.cycles} циклов подключения")
        times, failures = [], 0
        for i in range(args.cycles):
            await phone.disconnect()
            await asyncio.sleep(0.6)
            try:
                dt = await phone.connect()
                ok = await phone.submit_pin(pin)
                times.append(dt)
                if not (phone.connected and ok):
                    failures += 1
            except Exception as e:
                failures += 1
                print(f"    цикл {i+1}: ошибка {type(e).__name__}: {e}")
            if (i + 1) % 5 == 0:
                inf = target.info()
                print(f"    {i+1}/{args.cycles}: подключений {len(times)}, сбоев {failures}, "
                      f"boot={inf.int('boot') if inf else '?'}, "
                      f"heap={inf.int('heap') if inf else '?'}")
        avg = sum(times) / len(times) if times else 0
        check(f"{args.cycles} циклов подключения без сбоев", failures == 0,
              f"успешно {len(times)}/{args.cycles}, среднее время {avg:.1f} с, "
              f"макс {max(times):.1f} с" if times else "нет успешных")

        info = target.info()
        check("устройство не перезагружалось за весь тест",
              info and info.int("boot") == boot0,
              f"boot {boot0} → {info.int('boot') if info else '?'}, "
              f"uptime {info.int('uptime') if info else '?'} с")

        stats = target.ble_stats()
        if stats:
            fail_ratio = (stats.int("notify_fail") /
                          max(1, stats.int("notify_ok") + stats.int("notify_fail")))
            check("уведомления не теряются", fail_ratio < 0.10,
                  f"успешно {stats.int('notify_ok')}, неудач {stats.int('notify_fail')} "
                  f"({fail_ratio * 100:.1f}%)")
            print(f"    подключений {stats.int('conn')}, разрывов {stats.int('disc')}, "
                  f"последняя причина {stats.get('last_reason')}")

        # --- 4. Радио под живым BLE ---
        print("\n[4] Радио работает при подключённом телефоне")
        # После цикла телефон остаётся подключённым; устройство держит одно
        # соединение и рекламу не возобновляет — сначала честно отключаемся.
        await phone.disconnect()
        await asyncio.sleep(1.0)
        await phone.connect()
        await phone.submit_pin(pin)
        before = phone.count_cmd(CMD_RECV_MESSAGE)
        peer.send_text(target_id, "UNDER-BLE")
        await asyncio.sleep(3.0)
        check("сообщение доходит при активном BLE",
              phone.count_cmd(CMD_RECV_MESSAGE) > before)
        info = target.info()
        check("состояние устройства в норме",
              info and info.int("boot") == boot0 and info.get("file_state") == "0",
              f"heap {info.int('heap') if info else '?'}, "
              f"min_heap {info.int('min_heap') if info else '?'}")

        await phone.disconnect()
    finally:
        peer.testmode(False)
        target.close(); peer.close()

    passed = sum(1 for _, ok, _ in results if ok)
    print(f"\n{'=' * 60}")
    print(f"ИТОГО: {passed}/{len(results)} проверок за {time.time() - t_all:.0f} с")
    for n, ok, d in results:
        if not ok:
            print(f"  ✗ {n} — {d}")
    return 0 if passed == len(results) else 1


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
