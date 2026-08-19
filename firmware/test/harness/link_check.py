import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from device import Device

A = Device("/dev/ttyUSB0", name="A-6770")
B = Device("/dev/ttyUSB1", name="B-7E88")
try:
    time.sleep(0.3)
    print("TESTMODE A:", A.testmode(True).fields)
    print("TESTMODE B:", B.testmode(True).fields)
    A.rx_reset(); B.rx_reset()
    B.drain()

    print("\n--- A → B: broadcast текст ---")
    print("TX:", A.send_text("BCAST", "PROBE-1").fields)
    ev = B.wait("LORA_RX", 8.0)
    print("B RX:", ev.fields if ev else "НЕ ПРИНЯТО")

    print("\n--- B → A: адресный текст (dest=6770) ---")
    A.drain()
    print("TX:", B.send_text("7067", "PROBE-2").fields)
    ev = A.wait("LORA_RX", 8.0)
    print("A RX:", ev.fields if ev else "НЕ ПРИНЯТО")

    print("\n--- beacon A → B ---")
    B.drain()
    print("TX:", A.send_beacon().fields)
    ev = B.wait("LORA_RX", 8.0)
    print("B RX:", ev.fields if ev else "НЕ ПРИНЯТО")
finally:
    A.close(); B.close()
