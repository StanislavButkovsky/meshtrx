#!/usr/bin/env python3
"""Оценка энергопотребления по фактическому времени в режимах радио.

Амперметра в стенде нет, зато прошивка честно считает миллисекунды в TX/RX/
standby. Этого достаточно, чтобы посчитать средний ток радиочасти и увидеть,
какой сценарий и какой режим съедают батарею. Ток контроллера задан таблицей
ниже — его вклад обычно и оказывается решающим.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/power.py
    ... [--battery 2000] [--ports /dev/ttyUSB0,/dev/ttyUSB1]
"""

from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device, discover_ports  # noqa: E402

# Ток радиочипа SX1262, мА (даташит, DC-DC, 3.3 В)
I_RADIO = {"standby": 0.8, "rx": 5.3, "duty": 1.8}

# Ток передачи зависит от мощности; между узлами таблицы интерполируем
I_TX_BY_DBM = {-9: 18.0, 0: 24.0, 10: 40.0, 14: 55.0, 17: 75.0, 20: 100.0, 22: 118.0}

# Ток контроллера, мА — измеренные ориентиры для ESP32-S3 на 240 МГц
I_MCU = {
    "active_ble_adv": 45.0,     # цикл активен, BLE рекламируется
    "active_ble_conn": 50.0,    # телефон подключён
    "light_sleep": 2.5,         # автоматический light sleep между событиями
}
I_OLED = 12.0                   # экран включён


def tx_current(dbm: int) -> float:
    keys = sorted(I_TX_BY_DBM)
    if dbm <= keys[0]:
        return I_TX_BY_DBM[keys[0]]
    if dbm >= keys[-1]:
        return I_TX_BY_DBM[keys[-1]]
    for a, b in zip(keys, keys[1:]):
        if a <= dbm <= b:
            k = (dbm - a) / (b - a)
            return I_TX_BY_DBM[a] + k * (I_TX_BY_DBM[b] - I_TX_BY_DBM[a])
    return I_TX_BY_DBM[keys[-1]]


def radio_current(ev, dbm: int) -> tuple[float, dict]:
    """Средний ток радио за окно замера, мА, и доли режимов."""
    window = max(1, ev.int("window"))
    parts = {k: ev.int(k) for k in ("standby", "rx", "tx", "duty")}
    known = sum(parts.values())
    # Незакрытый остаток окна (переключения, ожидание) считаем standby
    parts["standby"] += max(0, window - known)
    ma = (parts["standby"] * I_RADIO["standby"] +
          parts["rx"] * I_RADIO["rx"] +
          parts["duty"] * I_RADIO["duty"] +
          parts["tx"] * tx_current(dbm)) / window
    shares = {k: v / window for k, v in parts.items()}
    return ma, shares


def hours(battery_mah: float, ma: float) -> float:
    return battery_mah / ma if ma > 0 else float("inf")


def measure(dev: Device, seconds: float, action=None):
    dev.radio_time_reset()
    if action:
        action()
    t_end = time.time() + seconds
    while time.time() < t_end:
        time.sleep(0.2)
    return dev.radio_time()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", default="",
                    help="через запятую; по умолчанию — найденные ttyUSB/ttyACM")
    ap.add_argument("--battery", type=float, default=2000.0, help="ёмкость, мАч")
    ap.add_argument("--window", type=float, default=20.0, help="длительность замера, с")
    args = ap.parse_args()

    ports = ([x.strip() for x in args.ports.split(",")]
             if args.ports else discover_ports(2))
    if len(ports) < 2:
        print(f"нужны две платы, найдено: {ports}")
        return 1
    pa, pb = ports
    A = Device(pa, name="A")
    B = Device(pb, name="B")

    try:
        time.sleep(0.4)
        info = A.info()
        dbm = info.int("pwr")
        pm = info.int("pm", -1)
        pm_note = ("light sleep включён" if pm == 0 else
                   f"light sleep НЕ включён (esp_pm_configure → 0x{pm:X})")
        print(f"устройство {info.get('name')}, мощность {dbm} дБм, "
              f"батарея {info.get('bat')} В, ёмкость расчёта {args.battery:.0f} мАч")
        print(f"контроллер: {pm_note}")
        if pm != 0:
            print("  → в расчёте ниже контроллер считается активным (I_MCU), "
                  "иначе покой стоил бы единицы миллиампер")
        print(f"окно замера {args.window:.0f} с\n")

        rows = []

        # 1. Покой в штатном режиме (как живёт устройство без телефона)
        A.testmode(False)
        ev = measure(A, args.window)
        rows.append(("покой, телефон не подключён", ev, "active_ble_adv", False))

        # 2. Покой с постоянным приёмом (режим «слушаем эфир»)
        A.testmode(True)
        ev = measure(A, args.window)
        rows.append(("постоянный приём (тестовый режим)", ev, "active_ble_adv", False))

        # 3. Голосовой поток: устройство передаёт
        B.testmode(True)
        n = int(args.window * 12)
        ev = measure(A, args.window + 2, lambda: A.send("TX AUDIO %d 80" % n))
        rows.append(("передача голоса (12 пак/с)", ev, "active_ble_conn", True))

        # 4. Голосовой поток: устройство принимает
        ev = measure(A, args.window + 2, lambda: B.send("TX AUDIO %d 80" % n))
        rows.append(("приём голоса", ev, "active_ble_conn", True))

        # 5. Маяки: только редкие передачи, радио в standby
        A.testmode(False)
        ev = measure(A, args.window, lambda: A.send_beacon())
        rows.append(("маяк раз в окно, радио в standby", ev, "active_ble_adv", False))

        print(f"{'сценарий':<38} {'RX':>6} {'TX':>6} {'STBY':>6} "
              f"{'радио':>8} {'всего':>8} {'ресурс':>10}")
        print("-" * 88)
        for title, ev, mcu, oled in rows:
            if ev is None:
                print(f"{title:<38}  нет ответа")
                continue
            ma_radio, sh = radio_current(ev, dbm)
            ma_total = ma_radio + I_MCU[mcu] + (I_OLED if oled else 0.0)
            h = hours(args.battery, ma_total)
            print(f"{title:<38} {sh['rx']*100:5.1f}% {sh['tx']*100:5.1f}% "
                  f"{sh['standby']*100:5.1f}% {ma_radio:7.2f}мА {ma_total:7.1f}мА "
                  f"{h:8.1f} ч")

        print("\nОценка потребления контроллера взята из таблицы I_MCU в этом файле;")
        print("радиочасть посчитана по фактическому времени в режимах.")
        print("Ресурс — от полной ёмкости до нуля, без запаса на просадку напряжения.")

        # Что дал бы light sleep
        base = I_MCU["active_ble_adv"]
        ls = I_MCU["light_sleep"]
        print(f"\nЕсли контроллер уйдёт в light sleep между событиями: "
              f"{base:.0f} мА → {ls:.1f} мА, покой примерно "
              f"{hours(args.battery, ls + 1.0):.0f} ч вместо "
              f"{hours(args.battery, base + 1.0):.0f} ч.")
        return 0
    finally:
        A.testmode(False); B.testmode(False)
        A.close(); B.close()


if __name__ == "__main__":
    sys.exit(main())
