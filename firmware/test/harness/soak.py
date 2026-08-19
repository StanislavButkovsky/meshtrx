#!/usr/bin/env python3
"""Длительный прогон: живёт ли устройство часами и не течёт ли память.

Короткие тесты ловят логику, но не ловят накопительные отказы — утечку кучи,
переполнение стека, редкий ребут раз в полчаса. Именно они и выглядели для
пользователя как «связь пропала». Скрипт крутит реальный трафик, снимает
показатели раз в интервал и пишет CSV, по которому видно тренд.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/soak.py \
        [--minutes 60] [--interval 60] [--csv /tmp/meshtrx-soak.csv]
"""

from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device  # noqa: E402


def snapshot(dev: Device) -> dict:
    info = dev.info()
    rt = dev.radio_time()
    ble = dev.ble_stats()
    if info is None:
        return {}
    row = {
        "boot": info.int("boot"),
        "uptime": info.int("uptime"),
        "heap": info.int("heap"),
        "min_heap": info.int("min_heap"),
        "file_state": info.int("file_state"),
        "mode": info.int("lora_mode"),
        "bat": info.get("bat"),
    }
    if rt:
        window = max(1, rt.int("window"))
        row["rx_pct"] = round(100 * rt.int("rx") / window, 1)
        row["tx_pct"] = round(100 * rt.int("tx") / window, 1)
        row["duty_pct"] = round(100 * rt.int("duty") / window, 1)
    if ble:
        row["ble_conn"] = ble.int("conn")
        row["ble_disc"] = ble.int("disc")
        row["notify_fail"] = ble.int("notify_fail")
    return row


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", default="/dev/ttyUSB0,/dev/ttyUSB1")
    ap.add_argument("--minutes", type=float, default=60.0)
    ap.add_argument("--interval", type=float, default=60.0, help="шаг замера, с")
    ap.add_argument("--csv", default="/tmp/meshtrx-soak.csv")
    ap.add_argument("--quiet", action="store_true",
                    help="без трафика — проверяем только покой")
    args = ap.parse_args()

    pa, pb = args.ports.split(",")
    A = Device(pa, name="A")
    B = Device(pb, name="B")

    t_end = time.time() + args.minutes * 60
    step = 0
    problems: list[str] = []

    try:
        time.sleep(0.4)
        A.testmode(True); B.testmode(True)
        A.radio_time_reset(); B.radio_time_reset()
        base = {d.name: snapshot(d) for d in (A, B)}
        heap0 = {n: s.get("heap", 0) for n, s in base.items()}
        boot0 = {n: s.get("boot", 0) for n, s in base.items()}
        print(f"старт: A heap={heap0['A']} boot={boot0['A']}, "
              f"B heap={heap0['B']} boot={boot0['B']}")
        print(f"прогон {args.minutes:.0f} мин, замер раз в {args.interval:.0f} с, "
              f"CSV: {args.csv}\n")

        cols = ["t_min", "dev", "boot", "uptime", "heap", "min_heap", "mode",
                "rx_pct", "tx_pct", "duty_pct", "ble_conn", "ble_disc",
                "notify_fail", "bat"]
        f = open(args.csv, "w")
        f.write(",".join(cols) + "\n")

        t0 = time.time()
        while time.time() < t_end:
            step += 1
            if not args.quiet:
                # Смешанный трафик: у каждого шага свой профиль, чтобы за час
                # прошли все пути — текст, голос, маяк, файл.
                kind = step % 4
                if kind == 0:
                    A.send_text(B.dev_id(), f"soak-{step}")
                elif kind == 1:
                    A.send_audio(24, 80)
                elif kind == 2:
                    B.send_beacon()
                else:
                    A.send_file("bin", 1024, B.dev_id(), timeout=90)

            time.sleep(max(1.0, args.interval - 5))

            t_min = (time.time() - t0) / 60
            for d in (A, B):
                row = snapshot(d)
                if not row:
                    problems.append(f"{t_min:.1f} мин: {d.name} не отвечает")
                    print(f"[{t_min:6.1f} мин] {d.name}: НЕТ ОТВЕТА")
                    continue
                if row["boot"] != boot0[d.name]:
                    problems.append(f"{t_min:.1f} мин: {d.name} перезагрузилось "
                                    f"({boot0[d.name]}→{row['boot']})")
                    boot0[d.name] = row["boot"]
                drop = heap0[d.name] - row["heap"]
                if drop > 8000:
                    problems.append(f"{t_min:.1f} мин: {d.name} потеряло "
                                    f"{drop} Б кучи")
                f.write(",".join(str(row.get(c, "")) if c != "t_min" and c != "dev"
                                 else (f"{t_min:.1f}" if c == "t_min" else d.name)
                                 for c in cols) + "\n")
                f.flush()
                print(f"[{t_min:6.1f} мин] {d.name}: heap={row['heap']} "
                      f"(мин {row['min_heap']}, дельта {-drop:+d}) "
                      f"boot={row['boot']} mode={row['mode']} "
                      f"rx={row.get('rx_pct', '?')}% tx={row.get('tx_pct', '?')}% "
                      f"bat={row['bat']}")
        f.close()
    finally:
        A.testmode(False); B.testmode(False)
        A.close(); B.close()

    print("\n" + "=" * 60)
    if problems:
        print(f"ПРОБЛЕМЫ ({len(problems)}):")
        for p in problems:
            print("  ✗ " + p)
        return 1
    print(f"Прогон {args.minutes:.0f} мин пройден: перезагрузок нет, "
          f"куча не просела. CSV: {args.csv}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
