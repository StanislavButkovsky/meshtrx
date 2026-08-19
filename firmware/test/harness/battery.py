#!/usr/bin/env python3
"""Замер реального разряда батареи: устройство на аккумуляторе, ноутбук по BLE.

Расчёт по времени в режимах радио даёт оценку, а не факт: неизвестен ток самой
платы и контроллера. Здесь измеряется то, что важно пользователю — как быстро
падает напряжение в реальном сценарии. USB не подключаем: он и питает, и
искажает картину, поэтому связь идёт по BLE, как у телефона.

Запуск (устройство ОТКЛЮЧЕНО от USB, работает от аккумулятора):
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/battery.py \
        [--hours 8] [--interval 60] [--csv /tmp/meshtrx-battery.csv] [--pin 1234]
"""

from __future__ import annotations

import argparse
import asyncio
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ble_phone import Phone, CMD_STATUS_UPDATE  # noqa: E402

# Кривая разряда одноэлементного Li-ion: напряжение → остаток ёмкости
DISCHARGE = [(4.20, 1.00), (4.10, 0.90), (4.00, 0.80), (3.90, 0.68),
             (3.80, 0.55), (3.70, 0.42), (3.60, 0.28), (3.50, 0.15),
             (3.40, 0.07), (3.30, 0.02), (3.00, 0.00)]


def remaining_fraction(v: float) -> float:
    if v >= DISCHARGE[0][0]:
        return 1.0
    for (v1, f1), (v2, f2) in zip(DISCHARGE, DISCHARGE[1:]):
        if v2 <= v <= v1:
            k = (v - v2) / (v1 - v2)
            return f2 + k * (f1 - f2)
    return 0.0


async def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hours", type=float, default=8.0)
    ap.add_argument("--interval", type=float, default=60.0)
    ap.add_argument("--csv", default="/tmp/meshtrx-battery.csv")
    ap.add_argument("--capacity", type=float, default=2000.0, help="ёмкость, мАч")
    ap.add_argument("--pin", type=int, default=None,
                    help="PIN устройства; без него статус всё равно приходит")
    args = ap.parse_args()

    addrs = await Phone.find()
    if not addrs:
        print("устройство не найдено в эфире BLE")
        return 1
    name, addr = next(iter(addrs.items()))
    print(f"устройство: {name} @ {addr}")

    phone = Phone(addr)
    await phone.connect()
    if args.pin is not None:
        await phone.submit_pin(args.pin)

    t0 = time.time()
    t_end = t0 + args.hours * 3600
    first_v = None
    f = open(args.csv, "w")
    f.write("t_min,volts,remaining_pct,ma_avg\n")
    print(f"пишем {args.csv}, шаг {args.interval:.0f} с, до {args.hours:.1f} ч\n")

    try:
        while time.time() < t_end:
            upd = await phone.wait_cmd(CMD_STATUS_UPDATE, args.interval + 30)
            if upd is None or len(upd) < 6:
                if not phone.connected:
                    print("соединение потеряно — переподключаемся")
                    try:
                        await phone.connect()
                        if args.pin is not None:
                            await phone.submit_pin(args.pin)
                    except Exception as e:                       # noqa: BLE001
                        print(f"  переподключение не удалось: {e}")
                        await asyncio.sleep(30)
                continue

            v = upd[5] / 10.0
            t_min = (time.time() - t0) / 60
            if first_v is None:
                first_v = v
            used = (remaining_fraction(first_v) - remaining_fraction(v)) * args.capacity
            ma = used / max(t_min / 60, 1e-6)
            f.write(f"{t_min:.1f},{v:.2f},{remaining_fraction(v) * 100:.1f},{ma:.1f}\n")
            f.flush()
            left = remaining_fraction(v) * args.capacity / ma if ma > 0.5 else float("inf")
            print(f"[{t_min:6.1f} мин] {v:.2f} В, заряд {remaining_fraction(v) * 100:5.1f}%, "
                  f"средний ток {ma:6.1f} мА, осталось "
                  f"{'—' if left == float('inf') else f'{left:.1f} ч'}")
            await asyncio.sleep(args.interval)
    finally:
        f.close()
        await phone.disconnect()

    print(f"\nCSV: {args.csv}")
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
