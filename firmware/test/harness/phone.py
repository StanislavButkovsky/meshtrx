"""Телефон в стенде: тот кусок пути, который радио не покрывает.

Стенд проверяет эфир между двумя платами и всегда говорил «связь работает».
Но между рацией и человеком есть ещё приложение, и ломается оно отдельно: в
сентябре 2026 подтверждение доставки исправно доходило до телефона, а на
экране висел крестик «не доставлено» — приложение искало своё сообщение по
остатку от времени отправки и не находило. Радио было ни при чём, поэтому
стенд молчал.

Телефон подключается по adb, приложением управляем через uiautomator: ищем
элементы по идентификаторам, а не по координатам — иначе набор проверок
развалится на первом же телефоне с другим экраном.
"""

from __future__ import annotations

import re
import subprocess
import time

PKG = "com.meshtrx.app"


def _adb(*args: str, timeout: float = 30) -> str:
    try:
        r = subprocess.run(("adb", *args), capture_output=True, text=True, timeout=timeout)
        return r.stdout
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return ""


def available() -> bool:
    """Телефон есть и виден adb. Без него блок проверок пропускается: стенд
    обязан работать и на голых платах."""
    out = _adb("devices")
    return any(l.strip().endswith("device") for l in out.splitlines()[1:])


class Phone:
    def __init__(self):
        self.w, self.h = self._screen()

    # ------------------------------------------------------------------ низ
    def _screen(self) -> tuple[int, int]:
        m = re.search(r"(\d+)x(\d+)", _adb("shell", "wm", "size"))
        return (int(m.group(1)), int(m.group(2))) if m else (720, 1440)

    def dump(self) -> str:
        """Снимок экрана в виде разметки. Файл, а не exec-out: на Android 10
        uiautomator пишет в stdout мусор вперемешку с XML."""
        _adb("shell", "uiautomator", "dump", "/sdcard/ui.xml", timeout=20)
        return _adb("shell", "cat", "/sdcard/ui.xml", timeout=20)

    @staticmethod
    def _nodes(xml: str):
        for m in re.finditer(r"<node[^>]*/?>", xml):
            node = m.group(0)
            rid = re.search(r'resource-id="([^"]*)"', node)
            txt = re.search(r'text="([^"]*)"', node)
            box = re.search(r'bounds="\[(\d+),(\d+)\]\[(\d+),(\d+)\]"', node)
            if not box:
                continue
            x1, y1, x2, y2 = map(int, box.groups())
            yield {
                "id": (rid.group(1).split("/")[-1] if rid else ""),
                "text": (txt.group(1) if txt else ""),
                "center": ((x1 + x2) // 2, (y1 + y2) // 2),
            }

    def find(self, xml: str, *, id: str = "", text: str = "", contains: str = ""):
        for n in self._nodes(xml):
            if id and n["id"] == id:
                return n
            if text and n["text"] == text:
                return n
            if contains and contains in n["text"]:
                return n
        return None

    def tap(self, node) -> bool:
        if not node:
            return False
        x, y = node["center"]
        _adb("shell", "input", "tap", str(x), str(y))
        return True

    def tap_id(self, rid: str, wait: float = 1.5) -> bool:
        ok = self.tap(self.find(self.dump(), id=rid))
        time.sleep(wait)
        return ok

    def type(self, text: str) -> None:
        _adb("shell", "input", "text", text)

    def logcat_clear(self) -> None:
        _adb("logcat", "-c")

    def logcat(self, pattern: str = "") -> list[str]:
        out = _adb("logcat", "-d", "-s", "MeshTRXService:D", "BleManager:D", timeout=20)
        lines = out.splitlines()
        return [l for l in lines if re.search(pattern, l)] if pattern else lines

    # ---------------------------------------------------------------- верх
    def restart_app(self) -> None:
        _adb("shell", "am", "force-stop", PKG)
        time.sleep(1)
        _adb("shell", "monkey", "-p", PKG, "-c", "android.intent.category.LAUNCHER", "1")
        time.sleep(6)

    def open_tab(self, rid: str) -> None:
        self.tap_id(rid, wait=2.5)

    def connect(self, pins: dict[str, str], attempts: int = 3) -> str | None:
        """Подключиться к рации и вернуть её имя.

        PIN зависит от того, какая плата отозвалась первой, поэтому передаём
        словарь «предпоследний октет MAC → PIN» и выбираем по журналу
        приложения. Октет именно предпоследний: последний у BLE-адреса на
        единицу больше, чем в имени устройства, и сравнивать по нему нельзя.

        Всё это нужно успеть за 45 секунд: рация сама рвёт соединение с
        клиентом, который столько молчит, — и вводимый по шагам PIN в этот
        срок не укладывался, пока шаги не свернули в одну цепочку.
        """
        for _ in range(attempts):
            self.open_tab("nav_settings")
            xml = self.dump()
            if self.find(xml, text="ОТКЛЮЧИТЬ"):
                name = self.find(xml, contains="MeshTRX-")
                return name["text"] if name else "?"
            self.logcat_clear()
            if not self.tap(self.find(xml, text="ПОДКЛЮЧИТЬ")):
                return None
            time.sleep(7)

            need = self.logcat(r"Need PIN for")
            already = self.logcat(r"already authorized")
            if need:
                mac = need[-1].split()[-1]
                octets = mac.split(":")
                key = octets[-2].upper() if len(octets) >= 2 else ""
                pin = pins.get(key)
                if pin:
                    xml = self.dump()
                    field = self.find(xml, contains="Введите PIN")
                    if field:
                        self.tap(field)
                        self.type(pin)
                        time.sleep(1)
                        _adb("shell", "input", "keyevent", "66")
                        time.sleep(1)
                        ok = self.find(self.dump(), text="OK")
                        self.tap(ok)
            time.sleep(8)
            if self.find(self.dump(), text="ОТКЛЮЧИТЬ") or already:
                xml = self.dump()
                name = self.find(xml, contains="MeshTRX-")
                return name["text"] if name else "?"
        return None

    def send_text(self, dest_callsign: str, text: str) -> bool:
        """Отправить адресное сообщение выбранному абоненту."""
        self.open_tab("nav_messages")
        if not self.tap_id("btnDest", wait=2.5):
            return False
        target = self.find(self.dump(), text=dest_callsign)
        if not target:
            return False
        self.tap(target)
        time.sleep(1.5)
        if not self.tap_id("etMessage", wait=1.0):
            return False
        self.type(text)
        time.sleep(1)
        # Клавиатура сдвигает панель ввода, поэтому кнопку ищем заново
        return self.tap(self.find(self.dump(), id="btnSend"))

    def last_status(self) -> str:
        """Значок последнего исходящего сообщения: ✓ доставлено, ✗ нет,
        ⏳ ждём подтверждения."""
        marks = [n["text"] for n in self._nodes(self.dump())
                 if re.match(r"^\d\d:\d\d ", n["text"] or "")]
        if not marks:
            return ""
        tail = marks[-1]
        for mark in ("\u2713", "\u2717", "\u23f3"):
            if mark in tail:
                return mark
        return ""
