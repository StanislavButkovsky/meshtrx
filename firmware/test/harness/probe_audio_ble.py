"""Где теряется голос: на радио или на пути BLE."""
import asyncio, sys, os, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from device import Device
from ble_phone import Phone, CMD_AUDIO_RX

async def main():
    target = Device("/dev/ttyUSB1", name="target")
    peer = Device("/dev/ttyUSB0", name="peer")
    try:
        await asyncio.sleep(0.3)
        st = target.ble_state(); pin = int(st.get("pin")); name = st.get("name")
        addrs = await Phone.find()
        phone = Phone(addrs[name])
        await phone.connect(); await phone.submit_pin(pin)
        peer.testmode(True)

        for gap in (80, 150, 300):
            target.rx_reset(); phone.rx.clear()
            peer.send_audio(30, gap)
            await asyncio.sleep(3.0)
            stats, types = target.rx_stats()
            radio = types.get("A0", 0)
            ble = phone.count_cmd(CMD_AUDIO_RX)
            print(f"интервал {gap:3d} мс: радио принято {radio}/30, "
                  f"до телефона дошло {ble}/30, потеря на BLE {radio - ble}")
        s = target.ble_stats()
        print("BLE:", s.fields if s else "нет")
    finally:
        peer.testmode(False)
        await phone.disconnect()
        target.close(); peer.close()

asyncio.run(main())
