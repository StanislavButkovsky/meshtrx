"""Codec2 3200 через ctypes — тот же кодек, что в прошивке и Android-клиенте.

Кодек нативный, и переписывать его на Python бессмысленно: голос идёт кадрами
по 20 мс, и любая интерпретируемая реализация не уложится в реальное время.
Ищем системную библиотеку, а на Windows — DLL, положенную рядом с программой.
"""

from __future__ import annotations

import ctypes
import ctypes.util
import os
import platform
import sys

MODE_3200 = 0          # нумерация самой libcodec2, не JNI-обёртки Android
FRAME_SAMPLES = 160    # 20 мс при 8 кГц
FRAME_BYTES = 8        # 3200 бит/с → 64 бита на кадр
FRAMES_PER_PACKET = 4  # столько кадров прошивка кладёт в один LoRa-пакет
PACKET_SAMPLES = FRAME_SAMPLES * FRAMES_PER_PACKET   # 640
PACKET_BYTES = FRAME_BYTES * FRAMES_PER_PACKET       # 32
SAMPLE_RATE = 8000


class Codec2Error(RuntimeError):
    pass


def _candidates() -> list[str]:
    system = platform.system()
    here = os.path.dirname(os.path.abspath(sys.argv[0] or "."))
    if system == "Windows":
        return [os.path.join(here, "codec2.dll"), "codec2.dll", "libcodec2.dll"]
    if system == "Darwin":
        return [os.path.join(here, "libcodec2.dylib"), "libcodec2.dylib",
                "/opt/homebrew/lib/libcodec2.dylib", "/usr/local/lib/libcodec2.dylib"]
    return ["libcodec2.so", "libcodec2.so.1.2", "libcodec2.so.1.1",
            "libcodec2.so.1.0", "libcodec2.so.1"]


def _load() -> ctypes.CDLL:
    found = ctypes.util.find_library("codec2")
    for name in ([found] if found else []) + _candidates():
        try:
            return ctypes.CDLL(name)
        except OSError:
            continue
    raise Codec2Error(
        "libcodec2 не найдена. Linux: apt install libcodec2-1.2 (или libcodec2-dev); "
        "macOS: brew install codec2; Windows: положите codec2.dll рядом с программой."
    )


class Codec2:
    """Обёртка одного экземпляра кодека. Не потокобезопасна — держите свой
    экземпляр для передачи и свой для приёма, как это делает прошивка."""

    def __init__(self, mode: int = MODE_3200):
        self._lib = _load()
        lib = self._lib
        lib.codec2_create.restype = ctypes.c_void_p
        lib.codec2_create.argtypes = [ctypes.c_int]
        lib.codec2_destroy.argtypes = [ctypes.c_void_p]
        lib.codec2_encode.argtypes = [ctypes.c_void_p,
                                      ctypes.POINTER(ctypes.c_ubyte),
                                      ctypes.POINTER(ctypes.c_short)]
        lib.codec2_decode.argtypes = [ctypes.c_void_p,
                                      ctypes.POINTER(ctypes.c_short),
                                      ctypes.POINTER(ctypes.c_ubyte)]
        lib.codec2_samples_per_frame.restype = ctypes.c_int
        lib.codec2_samples_per_frame.argtypes = [ctypes.c_void_p]
        lib.codec2_bits_per_frame.restype = ctypes.c_int
        lib.codec2_bits_per_frame.argtypes = [ctypes.c_void_p]

        self._c2 = lib.codec2_create(mode)
        if not self._c2:
            raise Codec2Error(f"codec2_create({mode}) вернул NULL")

        self.samples_per_frame = lib.codec2_samples_per_frame(self._c2)
        self.bytes_per_frame = (lib.codec2_bits_per_frame(self._c2) + 7) // 8
        if (self.samples_per_frame, self.bytes_per_frame) != (FRAME_SAMPLES, FRAME_BYTES):
            self.close()
            raise Codec2Error(
                f"библиотека отдаёт кадр {self.samples_per_frame} отсчётов / "
                f"{self.bytes_per_frame} байт, а прошивка ждёт "
                f"{FRAME_SAMPLES}/{FRAME_BYTES} — это другой режим кодека")

    # --- один кадр 20 мс ---
    def encode_frame(self, pcm: bytes) -> bytes:
        buf = (ctypes.c_ubyte * FRAME_BYTES)()
        samples = (ctypes.c_short * FRAME_SAMPLES).from_buffer_copy(pcm)
        self._lib.codec2_encode(self._c2, buf, samples)
        return bytes(buf)

    def decode_frame(self, data: bytes) -> bytes:
        out = (ctypes.c_short * FRAME_SAMPLES)()
        buf = (ctypes.c_ubyte * FRAME_BYTES).from_buffer_copy(data[:FRAME_BYTES])
        self._lib.codec2_decode(self._c2, out, buf)
        return bytes(out)

    # --- пакет из четырёх кадров, как его ждёт устройство ---
    def encode_packet(self, pcm: bytes) -> bytes:
        if len(pcm) != PACKET_SAMPLES * 2:
            raise ValueError(f"нужно {PACKET_SAMPLES} отсчётов, получено {len(pcm) // 2}")
        step = FRAME_SAMPLES * 2
        return b"".join(self.encode_frame(pcm[i * step:(i + 1) * step])
                        for i in range(FRAMES_PER_PACKET))

    def decode_packet(self, data: bytes) -> bytes:
        return b"".join(self.decode_frame(data[i * FRAME_BYTES:(i + 1) * FRAME_BYTES])
                        for i in range(FRAMES_PER_PACKET))

    def close(self):
        if getattr(self, "_c2", None):
            self._lib.codec2_destroy(self._c2)
            self._c2 = None

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
