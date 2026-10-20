package com.meshtrx.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Handler
import android.os.Looper
import com.meshtrx.app.model.*
import java.text.SimpleDateFormat
import java.util.*

/**
 * Демо-данные для снимков экрана и роликов: подключённая рация, станции
 * вокруг (одна — ретранслятор), переписка, файлы, шум каналов. Только в
 * отладочной сборке; трогает лишь ServiceState, в эфир ничего не уходит.
 */
class DemoReceiver : BroadcastReceiver() {
    override fun onReceive(ctx: Context, intent: Intent) {
        val lang = intent.getStringExtra("lang") ?: "ru"
        Handler(Looper.getMainLooper()).post { fill(ctx, lang == "en") }
    }

    private fun fill(ctx: Context, en: Boolean) {
        val now = System.currentTimeMillis()
        val lat = intent0Lat; val lon = intent0Lon
        val hm = SimpleDateFormat("HH:mm", Locale.US)
        fun t(minAgo: Int) = hm.format(Date(now - minAgo * 60_000L))
        val s = ServiceState
        s.connectionState.value = BleState.CONNECTED
        s.deviceName.value = "MeshTRX-7E88"
        s.callSign.value = "BSV"
        s.currentChannel.value = 12
        s.batteryVoltage.value = 4.05f
        s.firmwareVersion.value = "4.4.33"
        s.keyFingerprint.value = "7F3A"
        s.myLat.value = lat; s.myLon.value = lon
        s.statusMessage.value = if (en) "Connected" else "Подключено"
        s.peers.value = listOf(
            Peer("C4C8", "ALEX", -71, 9, 22, 82, now - 8_000, lat + 0.0105, lon + 0.0060),
            Peer("A1F2", "RPT-HILL", -88, 4, 22, null, now - 20_000, lat + 0.0035, lon - 0.0165, isRepeater = true),
            Peer("5D0E", "MARINA", -97, 1, 17, 64, now - 75_000, lat - 0.0120, lon - 0.0040),
            Peer("9B33", "OLEG", -103, -3, 22, 41, now - 240_000, lat - 0.0045, lon + 0.0215),
        )
        val msgs = if (en) listOf(
            ChatMessage(1, "Hi all! I'm at the spot, signal is great", false, "C4C8", "ALEX", null, null, -71, MessageStatus.DELIVERED, t(14), encrypted = true),
            ChatMessage(2, "Meet at the bridge in 20 minutes", false, "5D0E", "MARINA", null, null, -97, MessageStatus.DELIVERED, t(11), encrypted = true),
            ChatMessage(3, "OK", true, "me", "BSV", "5D0E", "MARINA", null, MessageStatus.ACCEPTED, t(10)),
            ChatMessage(4, "Repeater on the hill is up, try channel 12", false, "A1F2", "RPT-HILL", null, null, -88, MessageStatus.DELIVERED, t(6), encrypted = true),
            ChatMessage(5, "Hear you loud and clear 👍", true, "me", "BSV", "C4C8", "ALEX", null, MessageStatus.DELIVERED, t(2)),
        ) else listOf(
            ChatMessage(1, "Всем привет! Я на точке, связь отличная", false, "C4C8", "ALEX", null, null, -71, MessageStatus.DELIVERED, t(14), encrypted = true),
            ChatMessage(2, "Встречаемся у моста через 20 минут", false, "5D0E", "MARINA", null, null, -97, MessageStatus.DELIVERED, t(11), encrypted = true),
            ChatMessage(3, "OK", true, "me", "BSV", "5D0E", "MARINA", null, MessageStatus.ACCEPTED, t(10)),
            ChatMessage(4, "Ретранслятор на холме работает, канал 12", false, "A1F2", "RPT-HILL", null, null, -88, MessageStatus.DELIVERED, t(6), encrypted = true),
            ChatMessage(5, "Слышу тебя отлично 👍", true, "me", "BSV", "C4C8", "ALEX", null, MessageStatus.DELIVERED, t(2)),
        )
        s.messages.value = msgs
        s.recentCalls.value = listOf(
            RecentCall("C4C8", "ALEX", false, "PRIVATE", rssi = -71, timeMs = now - 120_000),
            RecentCall("", if (en) "General channel" else "Общий канал", false, "ALL", rssi = -88, timeMs = now - 600_000),
            RecentCall("5D0E", "MARINA", true, "PRIVATE", timeMs = now - 900_000),
        )
        val photo = java.io.File(ctx.filesDir, "demo_photo.jpg")
        s.fileTransfers.value = listOf(
            FileTransfer(11, if (en) "camp.jpg" else "лагерь.jpg", 0x01, 18_400, 92, 92, false, FileStatus.DONE, now - 300_000,
                localPath = if (photo.exists()) photo.path else null),
            FileTransfer(12, "route.gpx", 0x03, 6_200, 31, 19, true, FileStatus.TRANSFERRING, now - 30_000, destName = "ALEX"),
            FileTransfer(13, if (en) "note.txt" else "заметка.txt", 0x02, 900, 5, 5, true, FileStatus.DONE, now - 1_800_000, destName = "MARINA"),
        )
        s.channelNoise.value = listOf(-118, -112, -96, -115, -120, -109, -101, -117, -119, -93, -114, -116, -121, -108, -99, -113, -118, -110, -104, -116, -119, -107, -111)
        s.channelBest.value = 12
        s.lastRxCallSign.value = "ALEX"; s.lastRxDeviceId.value = "C4C8"
        s.lastRxRssi.value = -71; s.lastRxSnr.value = 9
    }

    companion object {
        // Окрестности условного полигона; координаты выдуманы для ролика
        var intent0Lat = 55.7512
        var intent0Lon = 37.6184
    }
}
