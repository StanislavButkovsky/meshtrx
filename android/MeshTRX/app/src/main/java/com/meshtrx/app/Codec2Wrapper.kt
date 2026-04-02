package com.meshtrx.app

class Codec2Wrapper {
    private var handle: Long = 0

    companion object {
        const val MODE_1200 = 0
        const val MODE_3200 = 1
        const val FRAME_SAMPLES = 160   // 20ms @ 8000Hz (Codec2 3200)
        const val FRAME_BYTES = 8       // Codec2 3200bps = 64 bits = 8 bytes
        const val FRAMES_PER_PACKET = 4
        const val PACKET_BYTES = FRAME_BYTES * FRAMES_PER_PACKET // 32

        init {
            System.loadLibrary("codec2jni")
        }
    }

    fun init(mode: Int = MODE_3200) {
        handle = nativeInit(mode)
    }

    fun encode(pcm: ShortArray): ByteArray? {
        if (handle == 0L) return null
        return nativeEncode(handle, pcm)
    }

    fun decode(encoded: ByteArray): ShortArray? {
        if (handle == 0L) return null
        return nativeDecode(handle, encoded)
    }

    fun encodePacket(pcm: ShortArray): ByteArray {
        val result = ByteArray(PACKET_BYTES)
        for (i in 0 until FRAMES_PER_PACKET) {
            val frame = ShortArray(FRAME_SAMPLES)
            System.arraycopy(pcm, i * FRAME_SAMPLES, frame, 0, FRAME_SAMPLES)
            val encoded = encode(frame) ?: ByteArray(FRAME_BYTES)
            System.arraycopy(encoded, 0, result, i * FRAME_BYTES, FRAME_BYTES)
        }
        return result
    }

    fun decodePacket(encoded: ByteArray): ShortArray {
        val result = ShortArray(FRAME_SAMPLES * FRAMES_PER_PACKET)
        for (i in 0 until FRAMES_PER_PACKET) {
            val frame = ByteArray(FRAME_BYTES)
            System.arraycopy(encoded, i * FRAME_BYTES, frame, 0, FRAME_BYTES)
            val pcm = decode(frame) ?: ShortArray(FRAME_SAMPLES)
            System.arraycopy(pcm, 0, result, i * FRAME_SAMPLES, FRAME_SAMPLES)
        }
        return result
    }

    fun destroy() {
        if (handle != 0L) {
            nativeFree(handle)
            handle = 0
        }
    }

    private external fun nativeInit(mode: Int): Long
    private external fun nativeEncode(handle: Long, pcm: ShortArray): ByteArray
    private external fun nativeDecode(handle: Long, encoded: ByteArray): ShortArray
    private external fun nativeFree(handle: Long)
}
