package com.meshtrx.app

import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.util.Log
import kotlin.math.sqrt

/**
 * Записывает голос до 10 сек, кодирует Codec2 3200, возвращает сжатые данные.
 */
class VoiceRecorder(private val maxDurationSec: Int = 10) {
    companion object {
        private const val TARGET_RMS = 4000f   // как agcTargetRms в AudioEngine
        private const val MAX_GAIN = 10f
        private const val TAG = "VoiceRecorder"
        const val SAMPLE_RATE = 8000
    }

    val maxSamples = SAMPLE_RATE * maxDurationSec

    private var audioRecord: AudioRecord? = null
    private var recording = false
    private var recordThread: Thread? = null
    private val pcmBuffer = ShortArray(maxSamples)
    private var pcmOffset = 0
    private var startTimeMs = 0L

    var onRmsLevel: ((Int) -> Unit)? = null
    var onTimeUpdate: ((Int) -> Unit)? = null // секунды записи
    var onMaxReached: (() -> Unit)? = null

    fun startRecording() {
        if (recording) return
        pcmOffset = 0

        val bufSize = AudioRecord.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT
        )
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.MIC, SAMPLE_RATE,
            AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT,
            maxOf(bufSize, SAMPLE_RATE) // минимум 1 сек буфер
        )
        audioRecord?.startRecording()
        recording = true
        startTimeMs = System.currentTimeMillis()

        recordThread = Thread {
            val buf = ShortArray(Codec2Wrapper.FRAME_SAMPLES) // 160 сэмплов = 20мс
            var lastSecond = 0
            while (recording && pcmOffset + buf.size <= maxSamples) {
                val read = audioRecord?.read(buf, 0, buf.size) ?: -1
                if (read > 0) {
                    System.arraycopy(buf, 0, pcmBuffer, pcmOffset, read)
                    pcmOffset += read

                    // RMS
                    var sum = 0L
                    for (i in 0 until read) sum += buf[i].toLong() * buf[i]
                    val rms = sqrt(sum.toDouble() / read).toInt()
                    onRmsLevel?.invoke(rms)

                    // Таймер
                    val sec = ((System.currentTimeMillis() - startTimeMs) / 1000).toInt()
                    if (sec != lastSecond) {
                        lastSecond = sec
                        onTimeUpdate?.invoke(sec)
                    }
                }
            }
            if (recording && pcmOffset >= maxSamples) {
                recording = false
                onMaxReached?.invoke()
            }
        }
        recordThread?.start()
        Log.d(TAG, "Recording started")
    }

    /**
     * Остановить запись и вернуть данные, закодированные Codec2.
     * @return ByteArray с Codec2 данными, или null если слишком короткая запись (<0.5 сек)
     */
    fun stopAndEncode(): ByteArray? {
        recording = false
        recordThread?.join(500)
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null

        val durationMs = System.currentTimeMillis() - startTimeMs
        Log.d(TAG, "Recording stopped: ${pcmOffset} samples, ${durationMs}ms")

        // Минимум 1 сек
        if (pcmOffset < SAMPLE_RATE) {
            Log.d(TAG, "Too short (${pcmOffset} samples), discarding")
            return null
        }

        // Проверить RMS — если тишина, не отправлять
        var rmsSum = 0L
        for (i in 0 until pcmOffset) rmsSum += pcmBuffer[i].toLong() * pcmBuffer[i]
        val avgRms = kotlin.math.sqrt(rmsSum.toDouble() / pcmOffset).toInt()
        if (avgRms < 200) {
            Log.d(TAG, "Too quiet (RMS=$avgRms), discarding")
            return null
        }

        // Выровнять громкость, как AGC живой передачи: без этого личная запись
        // шла в 5–7 раз тише обычной речи (RMS ~1500 против ~9000 у живой),
        // и на той стороне её было еле слышно даже на полной громкости.
        // Запись уже целиком в памяти, поэтому усиление общее на всю фразу и
        // ограничено пиком — чтобы не перегрузить.
        var peak = 1
        for (i in 0 until pcmOffset) peak = maxOf(peak, kotlin.math.abs(pcmBuffer[i].toInt()))
        val gain = minOf(TARGET_RMS / avgRms, MAX_GAIN, 30000f / peak).coerceAtLeast(1f)
        if (gain > 1.05f) {
            for (i in 0 until pcmOffset) {
                pcmBuffer[i] = (pcmBuffer[i] * gain).toInt().coerceIn(-32768, 32767).toShort()
            }
            Log.d(TAG, "Level: RMS $avgRms, peak $peak, gain ${"%.1f".format(gain)}")
        }

        // Кодировать Codec2
        val codec2 = Codec2Wrapper()
        codec2.init(Codec2Wrapper.MODE_3200)

        val totalFrames = pcmOffset / Codec2Wrapper.FRAME_SAMPLES
        val encoded = ByteArray(totalFrames * Codec2Wrapper.FRAME_BYTES)

        for (i in 0 until totalFrames) {
            val frame = ShortArray(Codec2Wrapper.FRAME_SAMPLES)
            System.arraycopy(pcmBuffer, i * Codec2Wrapper.FRAME_SAMPLES, frame, 0, Codec2Wrapper.FRAME_SAMPLES)
            val enc = codec2.encode(frame) ?: ByteArray(Codec2Wrapper.FRAME_BYTES)
            System.arraycopy(enc, 0, encoded, i * Codec2Wrapper.FRAME_BYTES, Codec2Wrapper.FRAME_BYTES)
        }

        codec2.destroy()
        Log.d(TAG, "Encoded: $totalFrames frames, ${encoded.size} bytes")
        return encoded
    }

    fun cancel() {
        recording = false
        recordThread?.join(500)
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        pcmOffset = 0
    }

    fun isRecording() = recording

    /** Длительность записи в секундах */
    fun getDurationSec(): Int = pcmOffset / SAMPLE_RATE
}
