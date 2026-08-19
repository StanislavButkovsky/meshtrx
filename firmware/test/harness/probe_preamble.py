"""Короткая преамбула (текст) против длинной (beacon/ACK) в обе стороны."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from device import Device

A = Device("/dev/ttyUSB0", name="A")
B = Device("/dev/ttyUSB1", name="B")
try:
    time.sleep(0.3)
    A.testmode(True); B.testmode(True)
    time.sleep(0.5)
    for src, dst, tag, dest in ((A, B, "A→B", "887E"), (B, A, "B→A", "7067")):
        dst.drain()
        src.send_text(dest, "short")
        ev = dst.wait("LORA_RX", 6.0, type="B0")
        print(f"{tag} короткая преамбула (текст):  {'ЕСТЬ' if ev else 'НЕТ'}")

        dst.drain()
        src.send_beacon()
        ev = dst.wait("LORA_RX", 8.0, type="D0")
        print(f"{tag} длинная преамбула (beacon):  {'ЕСТЬ' if ev else 'НЕТ'}")
finally:
    A.testmode(False); B.testmode(False)
    A.close(); B.close()
