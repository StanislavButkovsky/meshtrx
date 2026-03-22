package com.meshtrx.app

import android.media.*
import android.util.Log
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit
import kotlin.math.sqrt

class AudioEngine {
    companion object {
        private const val TAG = "AudioEngine"
        const val SAMPLE_RATE = 8000
        const val FRAME_SAMPLES = 160 // 20ms (Codec2 3200)
        const val FRAMES_PER_PACKET = 8
        const val PACKET_SAMPLES = FRAME_SAMPLES * FRAMES_PER_PACKET // 1280
    }

    private val codec2 = Codec2Wrapper()
    private var audioRecord: AudioRecord? = null
    private var audioTrack: AudioTrack? = null
    private var recording = false
    private var monitoring = false // VOX мониторинг (микрофон слушает, но не кодирует)
    private var playing = false
    @Volatile var sendAudio = false // true = кодировать и отправлять (PTT/VOX active)
    private var recordThread: Thread? = null
    private var playThread: Thread? = null
    private val playbackQueue = LinkedBlockingQueue<ShortArray>(10)

    var onAudioEncoded: ((ByteArray) -> Unit)? = null
    var onRmsLevel: ((Int) -> Unit)? = null
    var volumeBoost = 2.0f // усиление приёма (1.0 = норма, 3.0 = макс)
    var squelchThreshold = 0 // порог шумоподавления RMS (0 = отключён)

    fun init() {
        codec2.init(Codec2Wrapper.MODE_3200)
        initAudioTrack()
        Log.d(TAG, "AudioEngine initialized")
    }

    private fun initAudioTrack() {
        val minBuf = AudioTrack.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT
        )
        audioTrack = AudioTrack.Builder()
            .setAudioAttributes(AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build())
            .setAudioFormat(AudioFormat.Builder()
                .setSampleRate(SAMPLE_RATE)
                .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .build())
            .setBufferSizeInBytes(maxOf(minBuf, PACKET_SAMPLES * 2 * 3))
            .setTransferMode(AudioTrack.MODE_STREAM)
            .build()
    }

    private fun ensureAudioRecord() {
        if (audioRecord != null) return
        val minBuf = AudioRecord.getMinBufferSize(
            SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT
        )
        audioRecord = AudioRecord(
            MediaRecorder.AudioSource.VOICE_COMMUNICATION,
            SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT,
            maxOf(minBuf, FRAME_SAMPLES * 2 * FRAMES_PER_PACKET)
        )
    }

    /**
     * Запуск записи для PTT — кодирует и отправляет сразу.
     */
    fun startRecording() {
        if (recording) return
        ensureAudioRecord()
        audioRecord?.startRecording()
        recording = true
        sendAudio = true
        startRecordThread()
        Log.d(TAG, "Recording started (PTT)")
    }

    fun stopRecording() {
        sendAudio = false
        if (!monitoring) {
            recording = false
            recordThread?.join(1000)
            audioRecord?.stop()
            audioRecord?.release()
            audioRecord = null
            recordThread = null
        }
        Log.d(TAG, "Recording stopped (PTT)")
    }

    /**
     * Запуск мониторинга микрофона для VOX — слушает RMS, но кодирует только когда sendAudio=true.
     */
    fun startVoxMonitoring() {
        if (monitoring || recording) return
        ensureAudioRecord()
        audioRecord?.startRecording()
        monitoring = true
        recording = true
        sendAudio = false
        startRecordThread()
        Log.d(TAG, "VOX monitoring started")
    }

    fun stopVoxMonitoring() {
        monitoring = false
        sendAudio = false
        recording = false
        recordThread?.join(1000)
        audioRecord?.stop()
        audioRecord?.release()
        audioRecord = null
        recordThread = null
        Log.d(TAG, "VOX monitoring stopped")
    }

    private fun startRecordThread() {
        if (recordThread?.isAlive == true) return
        recordThread = Thread {
            val packetBuf = ShortArray(PACKET_SAMPLES)
            var offset = 0
            while (recording) {
                val frameBuf = ShortArray(FRAME_SAMPLES)
                val read = audioRecord?.read(frameBuf, 0, FRAME_SAMPLES) ?: 0
                if (read > 0) {
                    // RMS — всегда считаем (для VOX и VU-meter)
                    var sum = 0L
                    for (s in frameBuf) sum += s.toLong() * s
                    val rms = sqrt(sum.toDouble() / read).toInt()
                    onRmsLevel?.invoke(rms)

                    // Кодировать только если sendAudio=true и есть реальный звук
                    if (sendAudio) {
                        if (squelchThreshold == 0 || rms > squelchThreshold) {
                            System.arraycopy(frameBuf, 0, packetBuf, offset, read)
                            offset += read
                            if (offset >= PACKET_SAMPLES) {
                                val encoded = codec2.encodePacket(packetBuf)
                                onAudioEncoded?.invoke(encoded)
                                offset = 0
                            }
                        } else {
                            // Тишина — не отправлять, сбросить буфер
                            offset = 0
                        }
                    } else {
                        offset = 0 // сброс буфера если TX выключен
                    }
                }
            }
        }.apply {
            name = "AudioRecord"
            start()
        }
    }

    fun startPlayback() {
        if (playing) return
        audioTrack?.play()
        playing = true
        playThread = Thread {
            while (playing) {
                val pcm = playbackQueue.poll(100, TimeUnit.MILLISECONDS)
                if (pcm != null) {
                    audioTrack?.write(pcm, 0, pcm.size)
                }
            }
        }.apply {
            name = "AudioPlay"
            start()
        }
        Log.d(TAG, "Playback started")
    }

    fun stopPlayback() {
        playing = false
        playThread?.join(1000)
        audioTrack?.stop()
        playbackQueue.clear()
    }

    fun playEncodedPacket(codec2Data: ByteArray, isLastPacket: Boolean = false) {
        if (isLastPacket) {
            // PTT_END — payload нулевой, не декодировать (иначе Codec2 выдаёт шум)
            playRogerBeep()
            return
        }
        val pcm = codec2.decodePacket(codec2Data)
        // Усиление громкости
        if (volumeBoost != 1.0f) {
            for (i in pcm.indices) {
                val amplified = (pcm[i] * volumeBoost).toInt().coerceIn(-32768, 32767)
                pcm[i] = amplified.toShort()
            }
        }
        playbackQueue.offer(pcm)
    }

    private fun playRogerBeep() {
        val duration = 0.08 // 80мс
        val samples = (SAMPLE_RATE * duration).toInt()
        val beep = ShortArray(samples)
        val freq1 = 1200.0
        val freq2 = 1600.0
        for (i in beep.indices) {
            val t = i.toDouble() / SAMPLE_RATE
            val envelope = if (i < samples / 10) i.toFloat() / (samples / 10) // fade in
                else if (i > samples * 9 / 10) (samples - i).toFloat() / (samples / 10) // fade out
                else 1.0f
            val sample = (Math.sin(2 * Math.PI * freq1 * t) * 0.5 +
                         Math.sin(2 * Math.PI * freq2 * t) * 0.5) * envelope * 16000 * volumeBoost
            beep[i] = sample.toInt().coerceIn(-32768, 32767).toShort()
        }
        playbackQueue.offer(beep)
    }

    fun destroy() {
        stopVoxMonitoring()
        stopRecording()
        stopPlayback()
        audioTrack?.release()
        codec2.destroy()
    }
}
