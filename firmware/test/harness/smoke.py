"""Быстрая проверка стенда: обе платы отвечают, видят друг друга, гоняют текст."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from device import Device

A = Device(sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0", name="A")
B = Device(sys.argv[2] if len(sys.argv) > 2 else "/dev/ttyUSB1", name="B")

def show(tag, ev):
    print(f"{tag}: {ev.fields if ev else 'НЕТ ОТВЕТА'}")

try:
    time.sleep(0.5)
    show("A PING", A.ping())
    show("B PING", B.ping())
    show("A INFO", A.info())
    show("B INFO", B.info())
finally:
    A.close(); B.close()
