"""Локализация: после чего именно у передающего устройства пропадает приём."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from device import Device

A = Device("/dev/ttyUSB0", name="A")
B = Device("/dev/ttyUSB1", name="B")

def probe(tag):
    A.drain()
    B.send_text("7067", f"P-{tag}")
    ev = A.wait("LORA_RX", 6.0, type="B0")
    print(f"  приём A после «{tag}»: {'ЕСТЬ' if ev else 'НЕТ'}")
    return ev is not None

try:
    time.sleep(0.3)
    A.testmode(True); B.testmode(True)
    time.sleep(0.5)
    print("контроль (A ничего не передавал):")
    probe("старт")

    print("\nпосле одиночной передачи текста:")
    A.send_text("BCAST", "one")
    time.sleep(1.0)
    probe("1 текст")

    print("\nпосле серии из 30 аудиопакетов:")
    A.send_audio(30, 60)
    time.sleep(1.5)
    probe("30 аудио")

    print("\nпосле файла 2 КБ (18 чанков):")
    A.send_file("bin", 2048, "887E", timeout=90)
    time.sleep(1.5)
    probe("файл")

    print("\nсостояние A:", A.info().fields)
finally:
    A.testmode(False); B.testmode(False)
    A.close(); B.close()
