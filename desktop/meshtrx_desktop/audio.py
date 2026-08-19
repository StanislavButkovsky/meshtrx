"""Захват и воспроизведение голоса: микрофон → Codec2 → устройство и обратно.

Голос идёт пакетами по 80 мс (четыре кадра Codec2 по 20 мс) — так же, как в
прошивке и Android-клиенте. Между приёмом и звуком стоит небольшой буфер:
пакеты приходят из эфира неравномерно, и без него речь рвётся на каждом
опоздании.
"""

from __future__ import annotations

import queue
import threading
from collections.abc import Callable

import numpy as np
import sounddevice as sd

from .codec2 import Codec2, PACKET_BYTES, PACKET_SAMPLES, SAMPLE_RATE

JITTER_PACKETS = 3          # ~240 мс запаса перед началом воспроизведения
MAX_QUEUED_PACKETS = 50     # дальше поток явно сломан, старое уже неактуально


class Playback:
    """Очередь принятых пакетов → звук в динамике."""

    def __init__(self, device: int | str | None = None):
        self._codec = Codec2()
        self._pcm = queue.Queue(maxsize=MAX_QUEUED_PACKETS)
        self._tail = np.zeros(0, dtype=np.int16)
        self._priming = True
        self._stream: sd.OutputStream | None = None
        self._device = device
        self._lock = threading.Lock()

    def start(self):
        if self._stream:
            return
        self._stream = sd.OutputStream(
            samplerate=SAMPLE_RATE, channels=1, dtype="int16",
            blocksize=0, device=self._device, callback=self._callback)
        self._stream.start()

    def stop(self):
        stream, self._stream = self._stream, None
        if stream:
            stream.stop(); stream.close()
        with self._lock:
            self._tail = np.zeros(0, dtype=np.int16)
        while not self._pcm.empty():
            self._pcm.get_nowait()
        self._priming = True

    def push(self, encoded: bytes, is_last: bool = False):
        """Принятый из эфира пакет Codec2. Декодируем сразу: это дешевле, чем
        держать очередь кодированных кадров и гадать, успеем ли к сроку."""
        if len(encoded) < PACKET_BYTES:
            return
        pcm = np.frombuffer(self._codec.decode_packet(encoded), dtype=np.int16)
        try:
            self._pcm.put_nowait(pcm)
        except queue.Full:
            try:                      # освободить место, потеряв самое старое
                self._pcm.get_nowait()
                self._pcm.put_nowait(pcm)
            except queue.Empty:
                pass
        if is_last:
            self._priming = True      # следующая посылка снова копит буфер

    def _callback(self, outdata, frames, _time, _status):
        with self._lock:
            buf = self._tail
            if self._priming and self._pcm.qsize() < JITTER_PACKETS:
                outdata[:] = 0
                return
            self._priming = False
            while len(buf) < frames:
                try:
                    buf = np.concatenate([buf, self._pcm.get_nowait()])
                except queue.Empty:
                    break
            if len(buf) < frames:     # поток кончился — доводим тишиной
                take = np.concatenate([buf, np.zeros(frames - len(buf), dtype=np.int16)])
                self._tail = np.zeros(0, dtype=np.int16)
                self._priming = True
            else:
                take, self._tail = buf[:frames], buf[frames:]
        outdata[:, 0] = take

    def close(self):
        self.stop()
        self._codec.close()


class Capture:
    """Микрофон → Codec2 → колбэк с готовым пакетом для отправки."""

    def __init__(self, on_packet: Callable[[bytes], None],
                 on_level: Callable[[float], None] | None = None,
                 device: int | str | None = None):
        self._on_packet = on_packet
        self._on_level = on_level or (lambda level: None)
        self._codec = Codec2()
        self._buf = np.zeros(0, dtype=np.int16)
        self._stream: sd.InputStream | None = None
        self._device = device

    def start(self):
        if self._stream:
            return
        self._buf = np.zeros(0, dtype=np.int16)
        self._stream = sd.InputStream(
            samplerate=SAMPLE_RATE, channels=1, dtype="int16",
            blocksize=PACKET_SAMPLES, device=self._device, callback=self._callback)
        self._stream.start()

    def stop(self):
        stream, self._stream = self._stream, None
        if stream:
            stream.stop(); stream.close()

    def _callback(self, indata, _frames, _time, _status):
        samples = indata[:, 0].copy()
        self._on_level(float(np.sqrt(np.mean(samples.astype(np.float32) ** 2)) / 32768.0))
        self._buf = np.concatenate([self._buf, samples])
        while len(self._buf) >= PACKET_SAMPLES:
            chunk, self._buf = self._buf[:PACKET_SAMPLES], self._buf[PACKET_SAMPLES:]
            self._on_packet(self._codec.encode_packet(chunk.tobytes()))

    def close(self):
        self.stop()
        self._codec.close()


def list_devices() -> tuple[list[tuple[int, str]], list[tuple[int, str]]]:
    """(входы, выходы) — идентификаторы и имена для выбора в настройках."""
    inputs, outputs = [], []
    for idx, dev in enumerate(sd.query_devices()):
        if dev["max_input_channels"] > 0:
            inputs.append((idx, dev["name"]))
        if dev["max_output_channels"] > 0:
            outputs.append((idx, dev["name"]))
    return inputs, outputs
