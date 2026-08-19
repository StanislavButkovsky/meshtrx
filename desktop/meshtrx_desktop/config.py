"""Настройки клиента на диске: устройства, PIN-коды, выбранные звуковые входы.

Хранится в домашнем каталоге, а не рядом с программой: клиент может лежать
где угодно, в том числе в каталоге только для чтения.
"""

from __future__ import annotations

import json
import os
from pathlib import Path


def config_dir() -> Path:
    base = os.environ.get("XDG_CONFIG_HOME") or (Path.home() / ".config")
    path = Path(base) / "meshtrx"
    path.mkdir(parents=True, exist_ok=True)
    return path


class Config:
    def __init__(self, path: Path | None = None):
        self.path = path or (config_dir() / "settings.json")
        self.data: dict = {}
        self.load()

    def load(self):
        try:
            self.data = json.loads(self.path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            self.data = {}

    def save(self):
        try:
            self.path.write_text(json.dumps(self.data, ensure_ascii=False, indent=2),
                                 encoding="utf-8")
        except OSError:
            pass

    # --- устройства ---
    def known_pin(self, address: str) -> int | None:
        """PIN устройства, если его уже вводили. Телефон ведёт себя так же:
        спрашивает один раз, дальше подключается молча."""
        value = self.data.get("device_pins", {}).get(address)
        return int(value) if value is not None else None

    def remember_pin(self, address: str, pin: int):
        self.data.setdefault("device_pins", {})[address] = int(pin)
        self.save()

    def forget_device(self, address: str):
        self.data.get("device_pins", {}).pop(address, None)
        self.save()

    @property
    def last_device(self) -> str | None:
        return self.data.get("last_device")

    @last_device.setter
    def last_device(self, address: str | None):
        self.data["last_device"] = address
        self.save()

    def get(self, key: str, default=None):
        return self.data.get(key, default)

    def set(self, key: str, value):
        self.data[key] = value
        self.save()
