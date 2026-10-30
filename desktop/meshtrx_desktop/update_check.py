"""Проверка новой версии: тот же latest.json, что спрашивают телефоны."""

from __future__ import annotations

import json
import urllib.request

from . import __version__

LATEST_URLS = ("https://meshtrx.ru/latest.json", "https://meshtrx.com/latest.json")


def _ver(v: str) -> tuple[int, ...]:
    return tuple(int(x) for x in v.split(".") if x.isdigit())


def check(timeout: float = 8.0) -> dict:
    """Вернуть {'current', 'latest', 'newer', 'url', 'firmware'}; при ошибке — 'error'."""
    last_error = ""
    for url in LATEST_URLS:
        try:
            with urllib.request.urlopen(url, timeout=timeout) as r:
                data = json.loads(r.read().decode())
            desk = data.get("desktop") or {}
            latest = desk.get("version", "")
            return {"current": __version__, "latest": latest,
                    "newer": bool(latest) and _ver(latest) > _ver(__version__),
                    "url": desk.get("page", "https://meshtrx.ru/download/"),
                    "firmware": (data.get("firmware") or {}).get("version", "")}
        except Exception as e:                                   # noqa: BLE001
            last_error = str(e)
    return {"current": __version__, "error": last_error}
