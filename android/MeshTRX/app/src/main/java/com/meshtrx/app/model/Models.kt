package com.meshtrx.app.model

import android.bluetooth.BluetoothDevice

enum class BleState { DISCONNECTED, SCANNING, CONNECTING, CONNECTED }
enum class TxMode { PTT, VOX }
enum class ListenMode { ALL, PRIVATE_ONLY }
enum class MessageStatus { SENDING, SENT, DELIVERED, FAILED }

data class ChatMessage(
    val id: Long,
    val text: String,
    val isOutgoing: Boolean,
    val senderId: String,      // кто отправил (deviceId или "me")
    val senderName: String = "",// позывной отправителя
    val destId: String? = null, // кому (deviceId), null = broadcast
    val destName: String? = null,// позывной получателя
    val rssi: Int?,
    val status: MessageStatus,
    val time: String,
    val timeMs: Long = System.currentTimeMillis(),
    val voicePath: String? = null  // путь к .c2 файлу голосового сообщения
)

data class Peer(
    val deviceId: String,
    val callSign: String,
    val rssi: Int,
    val snr: Int,
    val txPower: Int,
    val batteryPct: Int?,
    val lastSeenMs: Long,
    val lat: Double? = null,  // null если GPS недоступен
    val lon: Double? = null
)

/** Haversine — расстояние в км между двумя координатами */
fun distanceKm(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
    val R = 6371.0
    val dLat = Math.toRadians(lat2 - lat1)
    val dLon = Math.toRadians(lon2 - lon1)
    val a = Math.sin(dLat / 2) * Math.sin(dLat / 2) +
            Math.cos(Math.toRadians(lat1)) * Math.cos(Math.toRadians(lat2)) *
            Math.sin(dLon / 2) * Math.sin(dLon / 2)
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a))
}

data class RecentCall(
    val deviceId: String,
    val callSign: String,
    val isOutgoing: Boolean,
    val callType: String,      // "ALL", "PRIVATE", "GROUP", "SOS"
    val groupName: String? = null,
    val rssi: Int? = null,
    val timeMs: Long = System.currentTimeMillis()
)

enum class CallType(val code: Int) {
    ALL(0), PRIVATE(1), GROUP(2), EMERGENCY(3);
    companion object {
        fun fromCode(code: Int) = entries.firstOrNull { it.code == code } ?: ALL
    }
}

data class IncomingCall(
    val callType: CallType,
    val senderId: String,
    val callSign: String,
    val lat: Double? = null,
    val lon: Double? = null,
    val callSeq: Int,
    val rssi: Int = 0,
    val timeMs: Long = System.currentTimeMillis()
)

enum class FileStatus { PENDING, TRANSFERRING, DONE, FAILED }

data class FileTransfer(
    val sessionId: Int,
    val fileName: String,
    val fileType: Int,      // 0x01=photo, 0x02=text, 0x03=binary
    val totalSize: Int,
    val chunksTotal: Int,
    val chunksDone: Int,
    val isOutgoing: Boolean,
    val status: FileStatus,
    val timeMs: Long = System.currentTimeMillis(),
    val data: ByteArray? = null, // в памяти (при передаче)
    val localPath: String? = null, // путь к файлу на диске
    val destMac: ByteArray? = null, // адресат (2 байта MAC), null = broadcast
    val destName: String? = null // имя адресата для отображения
) {
    /** Загрузить данные: из RAM или с диска */
    fun loadData(): ByteArray? {
        if (data != null) return data
        val path = localPath ?: return null
        return try { java.io.File(path).readBytes() } catch (_: Exception) { null }
    }
}

data class ScanDevice(
    val device: BluetoothDevice,
    val rssi: Int
)
