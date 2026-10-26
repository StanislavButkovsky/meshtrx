package com.meshtrx.app

import android.app.Activity
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.appcompat.app.AlertDialog
import java.io.File

/**
 * Запись сбоев: при падении приложения текст ошибки сохраняется в файл, а при
 * следующем запуске человеку предлагается его скопировать или отправить.
 *
 * Без этого о падении у человека на руках было только «снова схлопнулось»:
 * логов с чужого телефона не достать, и место ошибки приходилось угадывать.
 */
object CrashLog {
    private const val FILE = "last_crash.txt"
    @Volatile private var installed = false

    fun install(context: Context) {
        if (installed) return
        installed = true
        val app = context.applicationContext
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, e ->
            try {
                val text = buildString {
                    append("MeshTRX ").append(BuildConfig.VERSION_NAME)
                    append(" (").append(BuildConfig.VERSION_CODE).append(")\n")
                    append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
                    append(", Android ").append(Build.VERSION.RELEASE)
                    append(" (API ").append(Build.VERSION.SDK_INT).append(")\n")
                    append("Прошивка рации: ").append(ServiceState.firmwareVersion.value ?: "").append('\n')
                    append("Поток: ").append(thread.name).append("\n\n")
                    append(android.util.Log.getStackTraceString(e))
                }
                File(app.filesDir, FILE).writeText(text.take(16_000))
            } catch (_: Throwable) {}
            previous?.uncaughtException(thread, e)
        }
    }

    /** Если в прошлый раз приложение упало — показать текст ошибки один раз. */
    fun offerReport(activity: Activity) {
        val f = File(activity.filesDir, FILE)
        if (!f.exists()) return
        val text = try { f.readText() } catch (_: Throwable) { "" }
        f.delete()
        if (text.isBlank()) return
        AlertDialog.Builder(activity)
            .setTitle(activity.getString(R.string.crash_title))
            .setMessage(activity.getString(R.string.crash_message))
            .setPositiveButton(activity.getString(R.string.crash_share)) { _, _ ->
                val send = Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(Intent.EXTRA_TEXT, text)
                }
                activity.startActivity(Intent.createChooser(send, activity.getString(R.string.crash_share)))
            }
            .setNeutralButton(activity.getString(R.string.crash_copy)) { _, _ ->
                (activity.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager)
                    .setPrimaryClip(ClipData.newPlainText("MeshTRX crash", text))
            }
            .setNegativeButton(activity.getString(R.string.cancel), null)
            .show()
    }
}
