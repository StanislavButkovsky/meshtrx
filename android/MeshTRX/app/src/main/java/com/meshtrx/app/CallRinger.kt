package com.meshtrx.app

import android.content.Context
import android.media.*
import android.os.*
import com.meshtrx.app.model.CallType

class CallRinger(private val context: Context) {

    private var mediaPlayer: MediaPlayer? = null
    private var vibrator: Vibrator? = null

    fun start(callType: CallType) {
        val audioManager = context.getSystemService(Context.AUDIO_SERVICE) as AudioManager
        val ringerMode = audioManager.ringerMode

        // Звук: только в нормальном режиме
        if (ringerMode == AudioManager.RINGER_MODE_NORMAL) {
            try {
                val uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
                mediaPlayer = MediaPlayer().apply {
                    setDataSource(context, uri)
                    setAudioAttributes(AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_NOTIFICATION_RINGTONE)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build())
                    isLooping = true
                    prepare()
                    start()
                }
            } catch (_: Exception) {}
        }

        // Вибрация: EMERGENCY/PRIVATE всегда, ALL только в vibrate/normal
        val shouldVibrate = when {
            callType == CallType.EMERGENCY -> true
            callType == CallType.PRIVATE -> true
            ringerMode != AudioManager.RINGER_MODE_SILENT -> true
            else -> false
        }

        if (shouldVibrate) {
            vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                (context.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager).defaultVibrator
            } else {
                @Suppress("DEPRECATION")
                context.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            }

            val pattern = when (callType) {
                CallType.EMERGENCY -> longArrayOf(0, 200, 100, 200, 100, 200, 100) // тревога
                CallType.PRIVATE -> longArrayOf(0, 500, 200, 500, 200, 500) // три импульса
                CallType.GROUP -> longArrayOf(0, 300, 200, 300) // два импульса
                CallType.ALL -> longArrayOf(0, 200) // один короткий
            }
            vibrator?.vibrate(VibrationEffect.createWaveform(pattern, 0))
        }
    }

    fun stop() {
        mediaPlayer?.stop()
        mediaPlayer?.release()
        mediaPlayer = null
        vibrator?.cancel()
        vibrator = null
    }
}
