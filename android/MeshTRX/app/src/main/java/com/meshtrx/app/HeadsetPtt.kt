package com.meshtrx.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioDeviceInfo
import android.media.AudioManager
import android.media.ToneGenerator
import android.media.session.MediaSession
import android.media.session.PlaybackState
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.util.Log
import android.view.KeyEvent

/**
 * Кнопка Bluetooth-гарнитуры как PTT.
 *
 * Гарнитура не умеет «держать»: на нажатие она шлёт короткое событие, а не
 * пару «нажали / отпустили». Поэтому кнопка переключает: нажал — передача
 * пошла, нажал ещё раз — конец; предел 10 с гасит передачу сам, как всегда.
 *
 * Говорить в гарнитуру, а не в телефон в кармане: на время передачи звук
 * идёт через голосовой канал гарнитуры (SCO). Он и так 8 кГц — ровно столько,
 * сколько берёт Codec2, так что качество не теряется.
 */
class HeadsetPtt(private val context: Context, private val onToggle: () -> Unit) {

    companion object {
        private const val TAG = "HeadsetPtt"
        private const val SCO_WAIT_MS = 2000L
        // Что шлют гарнитуры и селфи-кнопки на нажатие
        private val KEYS = setOf(
            KeyEvent.KEYCODE_HEADSETHOOK, KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE,
            KeyEvent.KEYCODE_MEDIA_PLAY, KeyEvent.KEYCODE_MEDIA_PAUSE
        )
    }

    private val am = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
    private val handler = Handler(Looper.getMainLooper())
    private var session: MediaSession? = null
    private var scoOn = false
    private var scoReceiver: BroadcastReceiver? = null
    private var savedMode = AudioManager.MODE_NORMAL

    val isEnabled: Boolean get() = session != null

    /** Ловить кнопку. Android отдаёт её сессии, которая последней играла звук, —
     *  поэтому сессия помечена «играет»: приём голоса в MeshTRX и так идёт всё время. */
    fun enable() {
        if (session != null) return
        session = MediaSession(context, "MeshTRX PTT").apply {
            setCallback(object : MediaSession.Callback() {
                override fun onMediaButtonEvent(intent: Intent): Boolean {
                    @Suppress("DEPRECATION")
                    val ev = (if (Build.VERSION.SDK_INT >= 33)
                        intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT, KeyEvent::class.java)
                    else intent.getParcelableExtra(Intent.EXTRA_KEY_EVENT)) ?: return false
                    if (ev.keyCode !in KEYS) return false
                    if (ev.action == KeyEvent.ACTION_DOWN && ev.repeatCount == 0) {
                        Log.d(TAG, "кнопка гарнитуры: ${KeyEvent.keyCodeToString(ev.keyCode)}")
                        onToggle()
                    }
                    return true
                }
            })
            setPlaybackState(PlaybackState.Builder()
                .setActions(PlaybackState.ACTION_PLAY_PAUSE or PlaybackState.ACTION_PLAY or PlaybackState.ACTION_PAUSE)
                .setState(PlaybackState.STATE_PLAYING, 0, 1f)
                .build())
            isActive = true
        }
        Log.d(TAG, "кнопка гарнитуры включена")
    }

    fun disable() {
        stopSco()
        session?.release()
        session = null
    }

    /** Короткий сигнал «передача пошла»: экран погашен, иначе не понять, сработало ли. */
    fun beep() {
        try {
            val tg = ToneGenerator(AudioManager.STREAM_MUSIC, 60)
            tg.startTone(ToneGenerator.TONE_PROP_BEEP, 120)
            handler.postDelayed({ tg.release() }, 300)
        } catch (e: Exception) {
            Log.w(TAG, "сигнал не прозвучал: ${e.message}")
        }
    }

    /** Микрофон гарнитуры, если она подключена; null — писать с телефона. */
    fun scoInput(): AudioDeviceInfo? =
        am.getDevices(AudioManager.GET_DEVICES_INPUTS).firstOrNull { it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO }

    /** Включить голосовой канал гарнитуры и затем вызвать [then]. Нет гарнитуры
     *  или она не ответила за 2 с — [then] всё равно вызывается: говорим в телефон. */
    fun startSco(then: () -> Unit) {
        if (scoOn || scoInput() == null) { then(); return }
        savedMode = am.mode
        am.mode = AudioManager.MODE_IN_COMMUNICATION
        scoOn = true
        if (Build.VERSION.SDK_INT >= 31) {
            val dev = am.availableCommunicationDevices.firstOrNull { it.type == AudioDeviceInfo.TYPE_BLUETOOTH_SCO }
            if (dev != null && am.setCommunicationDevice(dev)) { then(); return }
        }
        var done = false
        val finish = { if (!done) { done = true; then() } }
        val r = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) {
                if (i.getIntExtra(AudioManager.EXTRA_SCO_AUDIO_STATE, -1) == AudioManager.SCO_AUDIO_STATE_CONNECTED) finish()
            }
        }
        scoReceiver = r
        context.registerReceiver(r, IntentFilter(AudioManager.ACTION_SCO_AUDIO_STATE_UPDATED))
        @Suppress("DEPRECATION")
        run { am.startBluetoothSco(); am.isBluetoothScoOn = true }
        handler.postDelayed({ finish() }, SCO_WAIT_MS)
    }

    fun stopSco() {
        if (!scoOn) return
        scoOn = false
        scoReceiver?.let { try { context.unregisterReceiver(it) } catch (_: Exception) {} }
        scoReceiver = null
        if (Build.VERSION.SDK_INT >= 31) am.clearCommunicationDevice()
        @Suppress("DEPRECATION")
        run { am.isBluetoothScoOn = false; am.stopBluetoothSco() }
        am.mode = savedMode
    }
}
