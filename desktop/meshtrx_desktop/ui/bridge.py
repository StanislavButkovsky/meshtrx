"""Мост между ядром клиента и Qt.

События приходят из потока BLE, а трогать виджеты можно только из потока
интерфейса. Сигналы Qt делают этот переход сами — поэтому всё, что приходит
из ядра, проходит через один объект.
"""

from __future__ import annotations

from PySide6.QtCore import QObject, Signal

from ..client import Client


class Bridge(QObject):
    state_changed = Signal(str, str)
    status_changed = Signal(object)
    message = Signal(object)
    peers_changed = Signal(object)
    call_changed = Signal(object)
    transfer = Signal(object)
    file_done = Signal(object)
    settings_changed = Signal(object)
    pin_result = Signal(bool)
    ptt_changed = Signal(bool)
    ptt_limit = Signal(int)
    mic_level = Signal(float)
    audio_rx = Signal(object)
    any_event = Signal(str, object)      # для журнала диагностики

    def __init__(self, client: Client):
        super().__init__()
        self.client = client
        client.subscribe(self._dispatch)

    def _dispatch(self, event: str, payload: object):
        # Всё, что приходит из потока BLE, обязано попасть в интерфейс через
        # сигналы: прямой вызов трогал бы виджеты из чужого потока и рано или
        # поздно вешал окно.
        self.any_event.emit(event, payload)
        match event:
            case "state":
                self.state_changed.emit(payload[0], payload[1])
            case "status":
                self.status_changed.emit(payload)
            case "message":
                self.message.emit(payload)
            case "peers":
                self.peers_changed.emit(payload)
            case "call":
                self.call_changed.emit(payload)
            case "transfer":
                self.transfer.emit(payload)
            case "file_done":
                self.file_done.emit(payload)
            case "settings":
                self.settings_changed.emit(payload)
            case "pin":
                self.pin_result.emit(bool(payload))
            case "ptt":
                self.ptt_changed.emit(bool(payload))
            case "ptt_limit":
                self.ptt_limit.emit(int(payload))
            case "mic_level":
                self.mic_level.emit(float(payload))
            case "audio_rx":
                self.audio_rx.emit(payload)
