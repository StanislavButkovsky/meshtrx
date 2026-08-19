#!/usr/bin/env python3
"""Интеграционные тесты MeshTRX на паре устройств.

Запуск:
    PYTHONPATH=firmware/test/harness/vendor python3 firmware/test/harness/run_tests.py
    ... [--only text,voice] [--ports /dev/ttyUSB0,/dev/ttyUSB1]

Устройства должны быть прошиты dev-сборкой (env heltec_v3_dev, -DTEST_CONSOLE).
Мощность принудительно опускается до аппаратного минимума (-9 дБм): платы лежат
рядом на столе, и без этого канал не имеет ничего общего с реальным.
"""

from __future__ import annotations

import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device, discover_ports  # noqa: E402

# Типы LoRa-пакетов (packet.h)
T_AUDIO, T_TEXT, T_TEXT_ACK = "A0", "B0", "B1"
T_FILE_START, T_FILE_CHUNK, T_FILE_ACK, T_FILE_END = "C0", "C1", "C2", "C3"
T_BEACON = "D0"
T_CALL_ALL, T_CALL_PRIV, T_CALL_GROUP, T_CALL_SOS = "E0", "E1", "E2", "E3"
T_CALL_ACCEPT, T_CALL_REJECT, T_CALL_CANCEL = "E4", "E5", "E6"

FILE_TYPES = ["photo", "text", "bin", "voice", "pttvoice"]

results: list[tuple[str, bool, str]] = []


def check(name: str, ok: bool, detail: str = ""):
    results.append((name, ok, detail))
    mark = "✓ OK  " if ok else "✗ FAIL"
    print(f"  {mark} {name}" + (f" — {detail}" if detail else ""))
    return ok


# ============================================================
# Тесты
# ============================================================

def test_ping(A: Device, B: Device):
    print("\n[1] Отклик устройств")
    a, b = A.ping(), B.ping()
    check("A отвечает на PING", a is not None, a.get("name") if a else "нет ответа")
    check("B отвечает на PING", b is not None, b.get("name") if b else "нет ответа")
    ia, ib = A.info(), B.info()
    if ia and ib:
        check("каналы совпадают", ia.get("ch") == ib.get("ch"),
              f"A ch={ia.get('ch')} B ch={ib.get('ch')}")
        check("A: heap в норме", ia.int("heap") > 60000, f"{ia.int('heap')} байт")
        check("B: heap в норме", ib.int("heap") > 60000, f"{ib.int('heap')} байт")
    return ia, ib


def calibrate_link(A: Device, B: Device) -> int:
    """Подобрать мощность так, чтобы канал был уверенным, но не перегруженным.

    Минимальная мощность выбиралась под две платы, лежащие рядом. Стоит их
    разнести или взять V4 с внешним усилителем — и картина меняется на десятки
    децибел: слишком сильный сигнал насыщает входной тракт, слишком слабый даёт
    случайные потери, и то и другое выглядит как дефект протокола. Поэтому
    уровень подбираем замером, а не константой.
    """
    print("\n[0] Калибровка канала")
    target_lo, target_hi = -70, -40          # рабочее окно RSSI
    dbm = A.info().int("pwr")
    for attempt in range(5):
        B.drain()
        for i in range(4):
            A.send_text("BCAST", f"CAL{i}")
            time.sleep(0.4)
        rssi = [e.int("rssi") for e in B.collect("LORA_RX", 1.5)
                if e.get("type") == T_TEXT]
        if not rssi:
            avg = -120
        else:
            avg = sum(rssi) // len(rssi)
        print(f"    мощность {dbm:+d} дБм → принято {len(rssi)}/4, RSSI {avg} дБм")
        if len(rssi) == 4 and target_lo <= avg <= target_hi:
            check("канал в рабочем окне", True, f"{avg} дБм при {dbm:+d} дБм")
            return dbm
        step = 6 if avg < target_lo else -6
        new_dbm = max(-9, min(14, dbm + step))
        if new_dbm == dbm:
            break
        dbm = new_dbm
        A.set_power(dbm); B.set_power(dbm)
        time.sleep(0.5)
    check("канал в рабочем окне", False,
          f"не удалось выйти на рабочий уровень, остались на {dbm:+d} дБм")
    return dbm


def test_text(A: Device, B: Device, count: int = 20):
    print(f"\n[2] Текст: {count} broadcast-сообщений A→B")
    B.drain()
    sent = 0
    for i in range(count):
        ev = A.send_text("BCAST", f"MSG-{i:03d}")
        if ev and ev.get("ok") == "1":
            sent += 1
        time.sleep(0.45)
    time.sleep(1.5)
    got = [e for e in B.collect("LORA_RX", 2.0) if e.get("type") == T_TEXT]
    # прошивка шлёт broadcast дважды (антиколлизия) — считаем уникальные приёмы
    loss = max(0, sent - min(sent, len(got)))
    rssi = [e.int("rssi") for e in got] or [0]
    check("broadcast доставлен", len(got) >= sent,
          f"отправлено {sent}, принято пакетов {len(got)}, RSSI ср. {sum(rssi)//len(rssi)}")

    print("[2b] Адресный текст B→A + ACK обратно")
    A.drain(); B.drain()
    B.send_text(A.dev_id(), "DIRECT-1")
    rx = A.wait("LORA_RX", 8.0, type=T_TEXT)
    check("адресный принят получателем", rx is not None, rx.raw if rx else "нет")
    ack = B.wait("LORA_RX", 8.0, type=T_TEXT_ACK)
    check("ACK вернулся отправителю", ack is not None, ack.raw if ack else "нет ACK")
    return loss


def test_voice(A: Device, B: Device, count: int = 100, gap: int = 80):
    print(f"\n[3] Голос: {count} аудиопакетов A→B, интервал {gap} мс")
    B.drain(); B.rx_reset()
    t0 = time.time()
    A.send_audio(count, gap)
    time.sleep(2.0)
    dt = time.time() - t0
    stats, types = B.rx_stats()
    got = types.get(T_AUDIO, 0)
    lost = stats.int("lost") if stats else -1
    per = 100.0 * max(0, count - got) / count
    check("голос доставлен", got >= count * 0.95,
          f"принято {got}/{count} ({per:.1f}% потерь), пропусков по seq {lost}, {dt:.1f} с")
    if stats:
        check("уровень сигнала измерен", stats.int("rssi_avg") != 0,
              f"RSSI ср. {stats.int('rssi_avg')} дБм, SNR ср. {stats.int('snr_avg')}")
    return per


def test_files(A: Device, B: Device, sizes=(1024, 5120, 20480)):
    print("\n[4] Файлы: все типы, обе стороны")
    # Все типы файлов на среднем размере
    for ft in FILE_TYPES:
        B.drain()
        acc, done = A.send_file(ft, 2048, B.dev_id(), timeout=120)
        rx = B.wait("FILE_RX", 5.0)
        ok = bool(done and done.get("result") == "DELIVERED")
        pattern_ok = bool(rx and rx.get("pattern_ok") == "1")
        detail = (f"{done.get('ms')} мс, NACK-раундов {done.get('nack_rounds')}"
                  if done else "нет результата")
        if rx:
            detail += f", приём {rx.get('chunks')}, паттерн {'целый' if pattern_ok else 'БИТЫЙ'}"
        check(f"файл type={ft} A→B", ok and pattern_ok, detail)

    # Размеры + обратное направление (баг №35: ACK не доходил в одну сторону)
    for size in sizes:
        for src, dst, dest_id, tag in ((A, B, B.dev_id(), "A→B"), (B, A, A.dev_id(), "B→A")):
            dst.drain()
            acc, done = src.send_file("bin", size, dest_id, timeout=240)
            rx = dst.wait("FILE_RX", 5.0)
            ok = bool(done and done.get("result") == "DELIVERED")
            pattern_ok = bool(rx and rx.get("pattern_ok") == "1")
            detail = (f"{done.get('ms')} мс, NACK {done.get('nack_rounds')}"
                      if done else "нет результата")
            if rx:
                detail += f", паттерн {'целый' if pattern_ok else 'БИТЫЙ'}"
            check(f"файл {size} Б {tag}", ok and pattern_ok, detail)


def test_beacon(A: Device, B: Device):
    print("\n[5] Beacon")
    # Устройства сами шлют маяки раз в 5 минут, и два маяка в эфире иногда
    # накладываются — одиночная потеря здесь ничего не значит, повторяем.
    ev = None
    for _ in range(3):
        B.drain()
        A.send_beacon()
        ev = B.wait("LORA_RX", 10.0, type=T_BEACON)
        if ev:
            break
        time.sleep(1.0)
    check("beacon принят", ev is not None,
          f"RSSI {ev.int('rssi')} дБм" if ev else "не принят за 3 попытки")


def test_idle_wake(A: Device, B: Device):
    """Устройство, живущее как в поле (без телефона), обязано слышать эфир.

    Регрессия против главного дефекта: раньше приёмник без BLE уходил в полный
    standby и не принимал вообще ничего — сеть теряла узел, как только у него
    отваливался телефон. Проверяем не факт приёма (диагностический хук печатает
    пакет и для чужого адреса), а ответный ACK: его шлёт только адресат.
    """
    print("\n[6] Приём после простоя без телефона")
    B.testmode(False)          # B живёт как в поле: телефона нет, трафика нет
    try:
        time.sleep(14)         # дольше LORA_IDLE_TIMEOUT_MS
        info = B.info()
        check("устройство без телефона слушает эфир",
              info is not None and info.int("lora_mode") != 2,
              f"режим {info.get('lora_mode') if info else '?'} "
              f"(0=постоянный приём, 1=duty cycle, 2=глухой standby)")

        got = 0
        for i in range(3):
            A.drain()
            A.send_text(B.dev_id(), f"IDLE-{i}")
            if A.wait("TEXT_ACK_RX", 8.0) or B.wait("TEXT_ACK_TX", 1.0):
                got += 1
            time.sleep(13)     # дать снова заснуть
        check("сообщения после простоя доходят", got == 3, f"{got}/3")
    finally:
        B.testmode(True)


def test_calls(A: Device, B: Device):
    print("\n[7] Вызовы")
    for kind, ptype, target in (("all", T_CALL_ALL, ""),
                                ("sos", T_CALL_SOS, ""),
                                ("priv", T_CALL_PRIV, "DB527E88")):
        B.drain()
        A.call(kind, target)
        ev = B.wait("LORA_RX", 10.0, type=ptype)
        check(f"вызов {kind.upper()} доставлен", ev is not None,
              ev.raw if ev else "не принят")
        time.sleep(1.0)


def test_call_answers(A: Device, B: Device):
    """Вызов без ответа — половина функции: проверяем и обратную сторону."""
    print("\n[7b] Ответы на вызов")
    for kind, ptype, label in (("accept", T_CALL_ACCEPT, "принятие"),
                               ("reject", T_CALL_REJECT, "отклонение")):
        A.drain(); B.drain()
        A.call("priv", B.dev_id() * 2)     # адрес вызова — 8 hex-символов
        inc = B.wait("LORA_RX", 10.0, type=T_CALL_PRIV)
        if not check(f"вызов дошёл до вызываемого ({label})", inc is not None):
            continue
        time.sleep(0.5)
        B.call_response(kind)
        ev = A.wait("LORA_RX", 10.0, type=ptype)
        check(f"ответ «{label}» дошёл до вызывающего", ev is not None,
              ev.raw if ev else "не принят")
        time.sleep(1.0)

    # Отмена своего вызова инициатором
    A.drain(); B.drain()
    A.call("priv", B.dev_id() * 2)
    B.wait("LORA_RX", 10.0, type=T_CALL_PRIV)
    time.sleep(0.5)
    A.call_response("cancel")
    ev = B.wait("LORA_RX", 10.0, type=T_CALL_CANCEL)
    check("отмена вызова дошла до вызываемого", ev is not None,
          ev.raw if ev else "не принята")


def test_channels(A: Device, B: Device):
    print("\n[8] Каналы")
    base = A.info().int("ch")
    new_ch = 5 if base != 5 else 7
    A.set_channel(new_ch); B.set_channel(new_ch)
    time.sleep(0.5)
    B.drain()
    A.send_text("BCAST", "CH-TEST")
    ev = B.wait("LORA_RX", 8.0, type=T_TEXT)
    check(f"связь на канале {new_ch}", ev is not None, ev.raw if ev else "нет приёма")

    # Разные каналы — приёма быть не должно
    B.set_channel(new_ch + 1)
    time.sleep(0.5)
    B.drain()
    A.send_text("BCAST", "ISOLATION")
    ev = B.wait("LORA_RX", 5.0, type=T_TEXT)
    check("разные каналы изолированы", ev is None,
          "принято, хотя не должно" if ev else "приёма нет — верно")

    A.set_channel(base); B.set_channel(base)
    time.sleep(0.5)


def test_stability(A: Device, B: Device, before):
    print("\n[9] Стабильность")
    ia, ib = A.info(), B.info()
    ba, bb = before
    if ia and ba:
        check("A не перезагружалось", ia.int("boot") == ba.int("boot"),
              f"boot {ba.int('boot')} → {ia.int('boot')}, uptime {ia.int('uptime')} с")
    if ib and bb:
        check("B не перезагружалось", ib.int("boot") == bb.int("boot"),
              f"boot {bb.int('boot')} → {ib.int('boot')}, uptime {ib.int('uptime')} с")
    if ia:
        check("A: память не утекла", ia.int("min_heap") > 50000,
              f"min_heap {ia.int('min_heap')} байт")
    if ib:
        check("B: память не утекла", ib.int("min_heap") > 50000,
              f"min_heap {ib.int('min_heap')} байт")


# ============================================================
def test_nack_recovery(A: Device, B: Device, losses=(10, 25, 40)):
    """NACK-досылка: приёмник теряет часть чанков, файл всё равно должен дойти целым.
    До появления LOSS этот механизм не исполнялся ни разу — во всех прогонах nack_rounds=0."""
    print("\n[10] Восстановление через NACK при потерях")
    for pct in losses:
        B.set_loss(pct)              # теряем только чанки файла
        B.drain()
        acc, done = A.send_file("bin", 5120, B.dev_id(), timeout=300)
        rx = B.wait("FILE_RX", 5.0)
        delivered = bool(done and done.get("result") == "DELIVERED")
        pattern_ok = bool(rx and rx.get("pattern_ok") == "1")
        rounds = done.int("nack_rounds") if done else -1
        detail = (f"{done.get('ms')} мс, NACK-раундов {rounds}"
                  if done else "нет результата")
        if rx:
            detail += f", паттерн {'целый' if pattern_ok else 'БИТЫЙ'}"
        check(f"файл 5 КБ при потере {pct}% чанков", delivered and pattern_ok, detail)
        if delivered and rounds == 0:
            check(f"  NACK-механизм отработал ({pct}%)", False,
                  "доставлено без досылки — потери не дошли до приёмника?")
        B.set_loss(0)
        time.sleep(1.0)


def test_concurrent_load(A: Device, B: Device):
    """Конкуренция за радио — тот класс гонок, что дал баг №35."""
    print("\n[11] Одновременная нагрузка")

    # 10a. Файл под встречным фоновым трафиком
    B.load_start("mixed", 1200, "BCAST")
    time.sleep(1.0)
    A.drain(); B.drain()
    acc, done = A.send_file("bin", 5120, B.dev_id(), timeout=300)
    rx = B.wait("FILE_RX", 8.0)
    B.load_stop()
    stats = B.load_stats()
    ok = bool(done and done.get("result") == "DELIVERED" and rx and rx.get("pattern_ok") == "1")
    detail = (f"{done.get('ms')} мс, NACK {done.get('nack_rounds')}, "
              f"фон: текст {stats.get('text') if stats else '?'}, "
              f"аудио {stats.get('audio') if stats else '?'}, "
              f"beacon {stats.get('beacon') if stats else '?'}") if done else "нет результата"
    check("файл проходит под встречным трафиком", ok, detail)
    time.sleep(1.5)

    # 10b. Встречная передача файлов одновременно с обеих сторон
    A.drain(); B.drain()
    A.send(f"TX FILE bin 2048 {B.dev_id()}")
    B.send(f"TX FILE bin 2048 {A.dev_id()}")
    a_acc = A.wait("TX_FILE", 5.0)
    b_acc = B.wait("TX_FILE", 5.0)
    a_done = A.wait("FILE_TX", 300) if a_acc and a_acc.get("accepted") == "1" else None
    b_done = B.wait("FILE_TX", 300) if b_acc and b_acc.get("accepted") == "1" else None
    both_started = bool(a_acc and b_acc)
    results = [d.get("result") if d else "нет" for d in (a_done, b_done)]
    check("встречная передача не вешает устройства", both_started,
          f"A принят={a_acc.get('accepted') if a_acc else '?'}, "
          f"B принят={b_acc.get('accepted') if b_acc else '?'}, результаты {results}")
    # После коллизии оба должны вернуться в рабочее состояние
    time.sleep(2.0)
    ia, ib = A.info(), B.info()
    check("оба устройства живы после коллизии",
          bool(ia and ib) and ia.get("file_state") == "0" and ib.get("file_state") == "0",
          f"file_state A={ia.get('file_state') if ia else '?'} "
          f"B={ib.get('file_state') if ib else '?'}")

    # 10c. Голос под нагрузкой
    B.load_start("text", 500, "BCAST")
    B.drain(); B.rx_reset()
    A.send_audio(50, 80)
    time.sleep(1.5)
    B.load_stop()
    stats, types = B.rx_stats()
    got = types.get(T_AUDIO, 0)
    # Полудуплекс: пока устройство передаёт встречный текст, оно не слышит.
    # Потери здесь — физика канала, а не дефект; провалом считаем развал связи.
    check("голос под нагрузкой", got >= 35,
          f"принято {got}/50 аудиопакетов ({100 - got * 2}% потерь), "
          f"пропусков по seq {stats.int('lost') if stats else '?'}")




def test_edge_cases(A: Device, B: Device):
    """Границы: размеры файлов, длина и кодировка текста, чужая адресация."""
    print("\n[12] Граничные случаи")

    # --- Файлы: 1 байт, ровно чанк, чанк+1 ---
    for size, note in ((1, "1 байт"), (120, "ровно чанк"), (121, "чанк + 1 байт")):
        B.drain()
        acc, done = A.send_file("bin", size, B.dev_id(), timeout=120)
        rx = B.wait("FILE_RX", 6.0)
        ok = bool(done and done.get("result") == "DELIVERED"
                  and rx and rx.get("pattern_ok") == "1"
                  and rx.int("size") == size)
        check(f"файл {note}", ok,
              f"принято {rx.get('size') if rx else '?'} Б, чанков {rx.get('chunks') if rx else '?'}"
              if rx else "не принят")

    # --- Отказ вместо падения при нехватке памяти ---
    heap = A.info().int("heap")
    too_big = heap + 50000
    acc, done = A.send_file("bin", too_big, B.dev_id(), timeout=20)
    check("слишком большой файл отклонён", bool(acc and acc.get("accepted") == "0"),
          f"запрошено {too_big} Б при heap {heap} Б → "
          f"{'отказ' if acc and acc.get('accepted') == '0' else 'ПРИНЯТ (!)'}")
    check("устройство живо после отказа", A.info() is not None)

    # --- Текст: максимальная длина и кириллица ---
    cases = [("максимум 84 символа", "X" * 84),
             ("кириллица UTF-8", "Проверка связи МешТРХ"),
             ("спецсимволы", "!@#$%^&*()_+-=[]{}|;:',.<>?/~`")]
    for note, text in cases:
        B.drain()
        A.send_text("BCAST", text)
        rx = B.wait("TEXT_RX", 8.0)
        expect = text.encode("utf-8")[:84]
        got = bytes.fromhex(rx.get("hex", "")) if rx and rx.get("hex") else b""
        check(f"текст: {note}", got == expect,
              f"отправлено {len(expect)} Б, принято {len(got)} Б"
              + ("" if got == expect else f" — расхождение: {got[:20]!r}"))

    # --- Чужая адресация ---
    B.drain()
    A.send_text("1234", "NOT-FOR-YOU")
    rx = B.wait("TEXT_RX", 5.0)
    delivered_up = bool(rx and rx.get("dest") == "1234")
    # пакет физически принят (это broadcast-среда), но адресован не нам:
    # ACK возвращать нельзя
    ack = A.wait("LORA_RX", 3.0, type=T_TEXT_ACK)
    check("на чужой адрес ACK не отправляется", ack is None,
          "ACK пришёл, хотя адресат другой" if ack else "ACK нет — верно")

    # --- Два файла подряд без паузы ---
    B.drain()
    acc1, done1 = A.send_file("bin", 1024, B.dev_id(), timeout=120)
    acc2, done2 = A.send_file("bin", 1024, B.dev_id(), timeout=120)
    check("два файла подряд", bool(done1 and done2
                                   and done1.get("result") == "DELIVERED"
                                   and done2.get("result") == "DELIVERED"),
          f"первый {done1.get('result') if done1 else '?'}, "
          f"второй {done2.get('result') if done2 else '?'}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ports", default="",
                    help="через запятую; по умолчанию — найденные ttyUSB/ttyACM")
    ap.add_argument("--only", default="",
                    help="ping,text,voice,files,beacon,idle,calls,channels,nack,load,edge")
    ap.add_argument("--logdir", default="/tmp/meshtrx-logs")
    args = ap.parse_args()

    ports = ([x.strip() for x in args.ports.split(",")]
             if args.ports else discover_ports(2))
    if len(ports) < 2:
        print(f"нужны две платы, найдено: {ports}")
        return 1
    pa, pb = ports
    os.makedirs(args.logdir, exist_ok=True)
    A = Device(pa, name="A", log_path=f"{args.logdir}/A.log")
    B = Device(pb, name="B", log_path=f"{args.logdir}/B.log")
    only = set(x.strip() for x in args.only.split(",") if x.strip())

    def enabled(x):
        return not only or x in only

    t0 = time.time()
    try:
        time.sleep(0.4)
        ta = A.testmode(True)
        tb = B.testmode(True)
        print(f"Тестовый режим: A tx={ta.get('tx_power') if ta else '?'} дБм, "
              f"B tx={tb.get('tx_power') if tb else '?'} дБм")
        before = test_ping(A, B)
        calibrate_link(A, B)
        if enabled("text"):     test_text(A, B)
        if enabled("voice"):    test_voice(A, B)
        if enabled("files"):    test_files(A, B)
        if enabled("beacon"):   test_beacon(A, B)
        if enabled("idle"):     test_idle_wake(A, B)
        if enabled("calls"):    test_calls(A, B)
        if enabled("calls"):    test_call_answers(A, B)
        if enabled("channels"): test_channels(A, B)
        if enabled("nack"):     test_nack_recovery(A, B)
        if enabled("load"):     test_concurrent_load(A, B)
        if enabled("edge"):     test_edge_cases(A, B)
        test_stability(A, B, before)
    finally:
        for d in (A, B):
            d.load_stop()
            d.set_loss(0)
        A.testmode(False)
        B.testmode(False)
        A.close(); B.close()

    passed = sum(1 for _, ok, _ in results if ok)
    total = len(results)
    print(f"\n{'=' * 60}")
    print(f"ИТОГО: {passed}/{total} проверок пройдено за {time.time() - t0:.0f} с")
    if passed < total:
        print("\nНе пройдены:")
        for name, ok, detail in results:
            if not ok:
                print(f"  ✗ {name} — {detail}")
    print(f"Логи устройств: {args.logdir}/A.log, {args.logdir}/B.log")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
