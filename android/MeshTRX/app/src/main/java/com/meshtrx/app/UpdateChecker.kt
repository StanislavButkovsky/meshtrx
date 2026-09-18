package com.meshtrx.app

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL

/** Что лежит на сайте прямо сейчас. */
data class LatestVersions(
    val appVersion: String,
    val appCode: Int,
    val appUrl: String,
    val firmwareVersion: String,
)

/**
 * Проверка обновлений.
 *
 * Из группы: «добавьте функцию обновления в приложении, чтобы не бегать каждый
 * раз на сайт». Выпуски выходят по нескольку раз в неделю, и человек узнавал о
 * них, только если заходил на сайт сам.
 *
 * Установку APK Android всё равно подтверждает у человека — это системное
 * правило, обойти его нельзя. Поэтому приложение делает то, что может: само
 * узнаёт, что вышло новое, и открывает ссылку.
 */
object UpdateChecker {
    private const val TAG = "UpdateChecker"
    private const val URL_LATEST = "https://meshtrx.ru/latest.json"
    private const val TIMEOUT_MS = 8000

    /** Раз в сутки: чаще незачем, а без сети проверка молча ничего не делает. */
    const val CHECK_INTERVAL_MS = 24L * 60 * 60 * 1000

    suspend fun fetch(): LatestVersions? = withContext(Dispatchers.IO) {
        try {
            val conn = (URL(URL_LATEST).openConnection() as HttpURLConnection).apply {
                connectTimeout = TIMEOUT_MS
                readTimeout = TIMEOUT_MS
                requestMethod = "GET"
            }
            val body = conn.inputStream.bufferedReader().use { it.readText() }
            conn.disconnect()

            val json = JSONObject(body)
            val app = json.getJSONObject("app")
            LatestVersions(
                appVersion = app.getString("version"),
                appCode = app.getInt("code"),
                appUrl = app.getString("url"),
                firmwareVersion = json.getJSONObject("firmware").getString("version"),
            )
        } catch (e: Exception) {
            // Нет сети — обычное дело для рации: молчим, а не пугаем ошибкой.
            Log.d(TAG, "проверка обновлений не удалась: ${e.message}")
            null
        }
    }

    /** Версия прошивки приходит строкой вида «4.4.21»; сравниваем по числам,
     *  иначе «4.4.9» окажется больше «4.4.13». */
    fun firmwareOlder(installed: String?, latest: String): Boolean {
        if (installed.isNullOrBlank()) return false
        val a = installed.trim().removePrefix("v").split(".").mapNotNull { it.toIntOrNull() }
        val b = latest.trim().removePrefix("v").split(".").mapNotNull { it.toIntOrNull() }
        if (a.isEmpty() || b.isEmpty()) return false
        for (i in 0 until maxOf(a.size, b.size)) {
            val x = a.getOrElse(i) { 0 }
            val y = b.getOrElse(i) { 0 }
            if (x != y) return x < y
        }
        return false
    }
}
