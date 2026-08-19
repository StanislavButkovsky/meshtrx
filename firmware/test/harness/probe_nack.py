"""Что с радио отправителя в момент ожидания ACK/NACK."""
import sys, time
sys.path.insert(0, __file__.rsplit('/', 1)[0])
from device import Device

A = Device("/dev/ttyUSB0", name="A")
B = Device("/dev/ttyUSB1", name="B")
try:
    time.sleep(0.3)
    A.testmode(True); B.testmode(True)
    B.set_loss(40)
    A.drain(); B.drain()
    print("состояние A до передачи:")
    A.send("RADIO"); ev = A.wait("RADIO_STAT", 3); print("  ", ev.fields if ev else "нет")

    A.send("TX FILE bin 3072 887E")
    print("передача пошла:", (A.wait("TX_FILE", 5) or {}).fields if A.wait else "")
    t0 = time.time()
    # опрашиваем состояние радио отправителя во время ожидания ответа
    for i in range(14):
        time.sleep(3)
        A.send("RADIO")
        ev = A.wait("RADIO_STAT", 3)
        resp = B.wait("FILE_RESP", 0.1)
        print(f"  t+{time.time()-t0:5.1f}s A: {ev.fields if ev else 'нет ответа'}"
              + (f"   | B отправил {resp.fields}" if resp else ""))
        done = A.wait("FILE_TX", 0.1)
        if done:
            print("  результат:", done.fields)
            break
    print("\nвсе FILE_RESP на B:", [e.fields for e in B.collect("FILE_RESP", 1.0)])
finally:
    B.set_loss(0)
    A.testmode(False); B.testmode(False)
    A.close(); B.close()
