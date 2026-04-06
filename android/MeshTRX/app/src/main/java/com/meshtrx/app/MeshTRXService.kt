package com.meshtrx.app

import android.annotation.SuppressLint
import android.app.*
import android.bluetooth.BluetoothDevice
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.*
import android.util.Log
import androidx.core.app.NotificationCompat
import com.meshtrx.app.model.*

class MeshTRXService : Service() {

    companion object {
        private const val TAG = "MeshTRXService"
        private const val NOTIF_CHANNEL = "meshtrx_service"
        private const val NOTIF_ID = 1
    }

    inner class LocalBinder : Binder() {
        fun getService(): MeshTRXService = this@MeshTRXService
    }

    private val binder = LocalBinder()
    lateinit var bleManager: BleManager
    lateinit var audioEngine: AudioEngine
    lateinit var voxEngine: VoxEngine

    private val handler = Handler(Looper.getMainLooper())
    private val prefs by lazy { getSharedPreferences("meshtrx", 0) }
    private var pendingDeviceAddr: String? = null
    private var msgSeq = 0
    private var reconnectAttempts = 0
    private val maxReconnectAttempts = 10
    private var wasConnected = false  // был ли подключён (для авто-реконнекта)

    override fun onCreate() {
        super.onCreate()
        Log.d(TAG, "Service created")

        createNotificationChannel()
        val notif = buildNotification("Запуск...")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            startForeground(NOTIF_ID, notif, ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE)
        } else {
            startForeground(NOTIF_ID, notif)
        }

        bleManager = BleManager(applicationContext)
        audioEngine = AudioEngine()
        voxEngine = VoxEngine()

        audioEngine.init()
        setupCallbacks()
        setupGpsForwarding()
        loadSavedPeers()
        loadRecentCalls()
        loadFileTransfers()
        loadMessages()
        startPeerCleanup()

        // Загрузить позывной
        val saved = prefs.getString("my_callsign", null)
        if (!saved.isNullOrEmpty()) {
            ServiceState.callSign.value = saved
        }
    }

    private fun setupCallbacks() {
        bleManager.onConnected = {
            ServiceState.connectionState.postValue(BleState.CONNECTED)
            ServiceState.statusMessage.postValue("Подключено")
            audioEngine.startPlayback()
            bleManager.requestSettings()
            wasConnected = true
            reconnectAttempts = 0
            handler.removeCallbacks(reconnectRunnable)
            // Отправить GPS на ESP32 сразу при подключении
            sendGpsToDevice()
            pendingDeviceAddr?.let { addr ->
                prefs.edit()
                    .putString("last_device_addr", addr)
                    .putString("last_device_name", ServiceState.deviceName.value ?: "")
                    .apply()
            }
            updateNotification("Подключено — ${ServiceState.deviceName.value}")
        }

        bleManager.onDisconnected = {
            ServiceState.connectionState.postValue(BleState.DISCONNECTED)
            ServiceState.statusMessage.postValue("Отключено")
            audioEngine.stopPlayback()
            if (ServiceState.txMode.value == TxMode.VOX) {
                audioEngine.stopVoxMonitoring()
                voxEngine.reset()
            }
            updateNotification("Отключено")
            // Авто-реконнект если связь была установлена ранее
            if (wasConnected) {
                scheduleReconnect()
            }
        }

        bleManager.onScanResult = { device, rssi ->
            val current = ServiceState.scanResults.value?.toMutableList() ?: mutableListOf()
            if (current.none { it.device.address == device.address }) {
                current.add(ScanDevice(device, rssi))
                ServiceState.scanResults.postValue(current)
            }
        }

        bleManager.onNeedPin = {
            val addr = pendingDeviceAddr ?: ""
            if (isDeviceAuthorized(addr)) {
                Log.d(TAG, "Device $addr already authorized")
                bleManager.onConnected?.invoke()
            } else {
                Log.d(TAG, "Need PIN for $addr")
                ServiceState.connectionState.postValue(BleState.CONNECTING)
                handler.post { ServiceState.showPinDialog.value = true }
            }
        }

        bleManager.onDataReceived = { data -> handleBleData(data) }

        audioEngine.onAudioEncoded = { encoded ->
            if (ServiceState.isPttActive.value == true && bleManager.isConnected()) {
                bleManager.sendAudioData(encoded)
            }
        }

        audioEngine.onRmsLevel = { rms ->
            ServiceState.rmsLevel.postValue(rms)
            if (ServiceState.txMode.value == TxMode.VOX) {
                voxEngine.process(rms)
            }
        }

        voxEngine.onTxActivated = {
            Log.d(TAG, "VOX: TX activated")
            ServiceState.isPttActive.postValue(true)
            audioEngine.sendAudio = true
            bleManager.sendPttStart()
        }

        voxEngine.onTxDeactivated = {
            Log.d(TAG, "VOX: TX deactivated")
            ServiceState.isPttActive.postValue(false)
            audioEngine.sendAudio = false
            Thread { Thread.sleep(100); bleManager.sendPttEnd() }.start()
        }

        voxEngine.onStateChanged = { state ->
            ServiceState.voxState.postValue(state)
        }
    }

    // === Public commands (called by fragments via binding) ===

    fun startScan(showPicker: Boolean = false) {
        ServiceState.scanResults.value = emptyList()
        ServiceState.connectionState.value = BleState.SCANNING
        if (showPicker) {
            ServiceState.showDevicePicker.value = true
        }
        bleManager.startScan()
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        ServiceState.connectionState.postValue(BleState.CONNECTING)
        ServiceState.deviceName.postValue(device.name ?: "MeshTRX")
        pendingDeviceAddr = device.address
        bleManager.connect(device)
    }

    fun disconnect() {
        stopReconnect()
        if (ServiceState.txMode.value == TxMode.VOX) {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
        }
        bleManager.disconnect()
        ServiceState.connectionState.postValue(BleState.DISCONNECTED)
    }

    fun forgetAndScan() {
        disconnect()
        prefs.edit().clear().apply()
        ServiceState.deviceName.value = ""
        startScan(showPicker = true)
    }

    private val reconnectRunnable = Runnable { autoConnect() }

    private fun scheduleReconnect() {
        if (reconnectAttempts >= maxReconnectAttempts) {
            Log.d(TAG, "Max reconnect attempts reached ($maxReconnectAttempts)")
            ServiceState.statusMessage.postValue("Нет связи")
            updateNotification("Нет связи")
            return
        }
        reconnectAttempts++
        // Нарастающая задержка: 3, 5, 7, 10, 10, 10... сек
        val delaySec = when {
            reconnectAttempts <= 1 -> 3L
            reconnectAttempts <= 2 -> 5L
            reconnectAttempts <= 3 -> 7L
            else -> 10L
        }
        Log.d(TAG, "Reconnect #$reconnectAttempts in ${delaySec}s")
        ServiceState.statusMessage.postValue("Переподключение #$reconnectAttempts через ${delaySec}с...")
        handler.removeCallbacks(reconnectRunnable)
        handler.postDelayed(reconnectRunnable, delaySec * 1000)
    }

    fun stopReconnect() {
        handler.removeCallbacks(reconnectRunnable)
        reconnectAttempts = 0
        wasConnected = false
    }

    @SuppressLint("MissingPermission")
    fun autoConnect() {
        val addr = prefs.getString("last_device_addr", null) ?: return
        val name = prefs.getString("last_device_name", "") ?: ""
        if (!isDeviceAuthorized(addr)) return
        if (ServiceState.connectionState.value != BleState.DISCONNECTED) return

        Log.d(TAG, "Auto-connecting to $name ($addr)")
        ServiceState.deviceName.postValue(name)
        ServiceState.connectionState.postValue(BleState.CONNECTING)
        ServiceState.statusMessage.postValue("Подключение к $name...")
        pendingDeviceAddr = addr

        val adapter = (applicationContext.getSystemService(Context.BLUETOOTH_SERVICE)
                as android.bluetooth.BluetoothManager).adapter
        val device = adapter?.getRemoteDevice(addr) ?: return
        bleManager.connect(device)
    }

    fun submitPin(pin: Int) {
        bleManager.sendPinCheck(pin)
    }

    fun pttDown() {
        if (ServiceState.txMode.value != TxMode.PTT) return
        ServiceState.isPttActive.value = true
        bleManager.sendPttStart()
        audioEngine.startRecording()
    }

    fun pttUp() {
        if (ServiceState.txMode.value != TxMode.PTT) return
        ServiceState.isPttActive.value = false
        audioEngine.stopRecording()
        Thread { Thread.sleep(100); bleManager.sendPttEnd() }.start()
    }

    fun setTxMode(mode: TxMode) {
        val prev = ServiceState.txMode.value
        ServiceState.txMode.value = mode

        if (prev == TxMode.VOX && mode == TxMode.PTT) {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
            ServiceState.isPttActive.value = false
            ServiceState.voxState.value = VoxEngine.State.IDLE
        }
        if (mode == TxMode.VOX && ServiceState.connectionState.value == BleState.CONNECTED) {
            voxEngine.reset()
            audioEngine.startVoxMonitoring()
        }
    }

    fun setChannel(ch: Int) {
        bleManager.setChannel(ch)
        ServiceState.currentChannel.value = ch
    }

    fun setListenMode(mode: ListenMode) {
        ServiceState.listenMode.value = mode
    }

    fun acceptCall(call: IncomingCall) {
        val pkt = ByteArray(2)
        pkt[0] = BleManager.CMD_CALL_ACCEPT.toByte()
        pkt[1] = call.callSeq.toByte()
        bleManager.send(pkt)
        ServiceState.incomingCall.postValue(null)
        ServiceState.callActive.postValue(true)
        audioEngine.startPlayback()
        Log.d(TAG, "Call ACCEPTED: ${call.callSign}")
    }

    fun rejectCall(call: IncomingCall) {
        val pkt = ByteArray(2)
        pkt[0] = BleManager.CMD_CALL_REJECT.toByte()
        pkt[1] = call.callSeq.toByte()
        bleManager.send(pkt)
        ServiceState.incomingCall.postValue(null)
        Log.d(TAG, "Call REJECTED: ${call.callSign}")
    }

    private fun launchIncomingCallScreen(call: IncomingCall) {
        val intent = Intent(this, IncomingCallActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP
            putExtra(IncomingCallActivity.EXTRA_CALL_TYPE, call.callType.code)
            putExtra(IncomingCallActivity.EXTRA_SENDER_ID, call.senderId)
            putExtra(IncomingCallActivity.EXTRA_CALL_SIGN, call.callSign)
            putExtra(IncomingCallActivity.EXTRA_CALL_SEQ, call.callSeq)
            putExtra(IncomingCallActivity.EXTRA_RSSI, call.rssi)
            call.lat?.let { putExtra(IncomingCallActivity.EXTRA_LAT, it) }
            call.lon?.let { putExtra(IncomingCallActivity.EXTRA_LON, it) }
        }
        startActivity(intent)
    }

    // === File transfer ===

    private var fileSessionCounter = 0
    private var fileDest = byteArrayOf(0, 0)

    // Буфер для приёма файла по BLE чанкам
    private var incomingFileBuffer: ByteArray? = null
    private var incomingFileSize = 0
    private var incomingFileOffset = 0
    private var incomingFileType = 0
    private var incomingFileName = ""
    private var incomingFileSender = ""

    /** Директория для хранения файлов передач */
    private val transfersDir: java.io.File by lazy {
        java.io.File(filesDir, "transfers").also { it.mkdirs() }
    }

    /** Сохранить данные файла на диск, вернуть путь */
    private fun saveFileData(name: String, timeMs: Long, data: ByteArray): String {
        val safe = name.replace(Regex("[^a-zA-Z0-9._-]"), "_")
        val file = java.io.File(transfersDir, "${timeMs}_$safe")
        file.writeBytes(data)
        return file.absolutePath
    }

    /** Удалить файл данных с диска */
    fun deleteFileData(localPath: String?) {
        if (localPath == null) return
        try { java.io.File(localPath).delete() } catch (_: Exception) {}
    }

    /** destMac: последние 2 байта MAC получателя, или null для broadcast */
    /** File Transfer v2: загрузить файл в ESP32 RAM → ESP32 сам отправит по LoRa */
    fun sendFile(fileName: String, fileType: Int, data: ByteArray, destMac: ByteArray? = null, destName: String? = null) {
        val dest = destMac ?: byteArrayOf(0, 0)
        val timeMs = System.currentTimeMillis()
        val localPath = saveFileData(fileName, timeMs, data)

        // Имя файла — обрезать с сохранением расширения (макс 19 байт)
        val shortName = if (fileName.length > 19) {
            val ext = fileName.substringAfterLast('.', "")
            if (ext.isNotEmpty() && ext.length < 6) {
                fileName.take(19 - ext.length - 1) + "." + ext
            } else fileName.take(19)
        } else fileName

        // FILE_UPLOAD_START: cmd(1) + type(1) + dest(2) + size(4) + name(20) = 28
        val header = ByteArray(28)
        header[0] = BleManager.CMD_FILE_UPLOAD_START.toByte()
        header[1] = fileType.toByte()
        header[2] = dest[0]; header[3] = dest[1]
        header[4] = (data.size and 0xFF).toByte()
        header[5] = ((data.size shr 8) and 0xFF).toByte()
        header[6] = ((data.size shr 16) and 0xFF).toByte()
        header[7] = ((data.size shr 24) and 0xFF).toByte()
        val nameBytes = shortName.toByteArray(Charsets.UTF_8)
        System.arraycopy(nameBytes, 0, header, 8, nameBytes.size.coerceAtMost(20))
        bleManager.send(header)

        // Запись в список
        val sessionId = (++fileSessionCounter) and 0xFF
        val totalChunks = (data.size + 120 - 1) / 120
        val transfer = FileTransfer(sessionId, fileName, fileType, data.size,
            totalChunks, 0, true, FileStatus.TRANSFERRING, timeMs = timeMs,
            data = data, localPath = localPath, destMac = destMac, destName = destName)
        val initList = ArrayList(ServiceState.fileTransfers.value ?: emptyList())
        val existIdx = initList.indexOfFirst { it.fileName == fileName && it.isOutgoing }
        if (existIdx >= 0) initList[existIdx] = transfer else initList.add(0, transfer)
        ServiceState.fileTransfers.value = initList

        // Загрузить данные в ESP32 по BLE чанками (120 байт)
        Thread {
            Thread.sleep(50) // дать время ESP32 аллоцировать буфер
            val bleChunkSize = 120
            var offset = 0
            while (offset < data.size) {
                val len = minOf(bleChunkSize, data.size - offset)
                val chunk = ByteArray(1 + len)
                chunk[0] = BleManager.CMD_FILE_UPLOAD_DATA.toByte()
                System.arraycopy(data, offset, chunk, 1, len)
                bleManager.send(chunk)
                offset += len
                Thread.sleep(50) // BLE пропускная способность (NimBLE MTU 128)
            }
            Log.d(TAG, "File uploaded to ESP32: $fileName (${data.size} bytes)")

            // Ждать UPLOAD_STATUS = DELIVERED(3) или FAILED(4) или таймаут
            val timeoutMs = prefs.getInt("file_timeout", 120) * 1000L  // 120 сек по умолчанию
            val start = System.currentTimeMillis()
            while (System.currentTimeMillis() - start < timeoutMs) {
                Thread.sleep(500)
                val current = ServiceState.fileTransfers.value?.find {
                    it.fileName == fileName && it.isOutgoing
                }
                if (current?.status == FileStatus.DONE || current?.status == FileStatus.FAILED) {
                    break
                }
            }
            // Если таймаут и статус не обновился — ошибка
            val current = ServiceState.fileTransfers.value?.find {
                it.fileName == fileName && it.isOutgoing
            }
            if (current?.status != FileStatus.DONE) {
                updateFileProgress(sessionId, 0, FileStatus.FAILED)
            }
            saveFileTransfers()
        }.start()
    }

    /** Сохранить позывной локально (без отправки на девайс) */
    fun saveCallSignLocal(cs: String) {
        prefs.edit().putString("my_callsign", cs).apply()
        ServiceState.callSign.postValue(cs)
    }

    fun setCallSign(cs: String) {
        saveCallSignLocal(cs)
        // Отправить на девайс через настройки
        val json = """{"callsign":"$cs"}"""
        bleManager.sendSettings(json)
    }

    fun setVoxThreshold(value: Int) { voxEngine.threshold = value }
    fun setVoxHangtime(ms: Long) { voxEngine.hangtimeMs = ms }

    fun sendTextMessage(text: String, destId: String? = null, destName: String? = null) {
        val seq = msgSeq++
        bleManager.sendMessage(seq, destId, text)
        val now = System.currentTimeMillis()
        val isAddressed = destId != null && destId.isNotEmpty()
        val msg = ChatMessage(
            id = now,
            text = text, isOutgoing = true, senderId = "me",
            senderName = ServiceState.callSign.value ?: "",
            destId = destId, destName = destName,
            rssi = null,
            status = if (isAddressed) MessageStatus.SENDING else MessageStatus.SENT,
            time = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault())
                .format(java.util.Date()),
            timeMs = now
        )
        val list = ServiceState.messages.value?.toMutableList() ?: mutableListOf()
        list.add(msg)
        ServiceState.messages.postValue(list)
        saveMessages()

        // Адресное: retry до 3 раз с таймаутом 2 сек
        if (isAddressed) {
            val msgId = now
            Thread {
                for (attempt in 1..3) {
                    Thread.sleep(2000)
                    // Проверить получен ли ACK
                    val current = ServiceState.messages.value?.find { it.id == msgId }
                    if (current?.status == MessageStatus.DELIVERED) return@Thread
                    // Повтор
                    Log.d(TAG, "[Text] Retry #$attempt for seq=$seq")
                    bleManager.sendMessage(seq, destId, text)
                }
                // После 3 попыток без ACK — пометить как FAILED
                val current = ServiceState.messages.value?.find { it.id == msgId }
                if (current?.status != MessageStatus.DELIVERED) {
                    updateMessageStatus(msgId, MessageStatus.FAILED)
                }
            }.start()
        }
    }

    fun saveVoiceData(name: String, timeMs: Long, data: ByteArray): String {
        return saveFileData(name, timeMs, data)
    }

    fun playVoiceMessage(data: ByteArray) {
        ServiceState.isPlayingVoice.postValue(true)
        Thread {
            val codec2 = Codec2Wrapper()
            codec2.init(Codec2Wrapper.MODE_3200)
            val totalFrames = data.size / Codec2Wrapper.FRAME_BYTES
            val pcm = ShortArray(totalFrames * Codec2Wrapper.FRAME_SAMPLES)
            for (i in 0 until totalFrames) {
                val frame = ByteArray(Codec2Wrapper.FRAME_BYTES)
                System.arraycopy(data, i * Codec2Wrapper.FRAME_BYTES, frame, 0, Codec2Wrapper.FRAME_BYTES)
                val decoded = codec2.decode(frame) ?: ShortArray(Codec2Wrapper.FRAME_SAMPLES)
                System.arraycopy(decoded, 0, pcm, i * Codec2Wrapper.FRAME_SAMPLES, Codec2Wrapper.FRAME_SAMPLES)
            }
            codec2.destroy()
            audioEngine.playPcmDirect(pcm)
            // Подождать окончания воспроизведения
            Thread.sleep((pcm.size * 1000L / 8000) + 200)
            ServiceState.isPlayingVoice.postValue(false)
        }.start()
    }

    fun playVoiceFile(path: String) {
        val data = java.io.File(path).readBytes()
        playVoiceMessage(data)
    }

    private fun updateMessageStatus(msgId: Long, status: MessageStatus) {
        val list = ServiceState.messages.value?.toMutableList() ?: return
        val idx = list.indexOfFirst { it.id == msgId }
        if (idx >= 0) {
            list[idx] = list[idx].copy(status = status)
            ServiceState.messages.postValue(list)
            saveMessages()
        }
    }

    // === BLE data handling ===

    private fun handleBleData(data: ByteArray) {
        if (data.isEmpty()) return
        when (data[0].toInt() and 0xFF) {
            BleManager.CMD_AUDIO_RX -> {
                // cmd(1) + flags(1) + sender(2) + payload(32) = 36 байт
                val pktSize = 4 + Codec2Wrapper.PACKET_BYTES // 36
                if (data.size >= pktSize) {
                    val flags = data[1].toInt() and 0xFF
                    val senderId = String.format("%02X%02X", data[2].toInt() and 0xFF, data[3].toInt() and 0xFF)
                    val audio = data.copyOfRange(4, pktSize)
                    val isLast = (flags and 0x02) != 0 // PKT_FLAG_PTT_END
                    // Воспроизводить если: слушаем всех ИЛИ вызов принят
                    val listen = ServiceState.listenMode.value == ListenMode.ALL
                    val active = ServiceState.callActive.value == true
                    if (listen || active) {
                        audioEngine.playEncodedPacket(audio, isLast)
                    }
                    // Обновить инфо абонента на PTT экране
                    if (senderId != "0000") {
                        val currentRssi = ServiceState.rssi.value ?: 0
                        val currentSnr = ServiceState.snr.value ?: 0
                        val peerName = ServiceState.peers.value
                            ?.find { it.deviceId.endsWith(senderId) }?.callSign ?: "TX-$senderId"
                        ServiceState.lastRxCallSign.postValue(peerName)
                        ServiceState.lastRxDeviceId.postValue(senderId)
                        ServiceState.lastRxRssi.postValue(currentRssi)
                        ServiceState.lastRxSnr.postValue(currentSnr)
                        ServiceState.isReceiving.postValue(!isLast)
                    }
                    // При PTT_END обновить peer
                    if (isLast && senderId != "0000") {
                        val currentRssi = ServiceState.rssi.value ?: 0
                        addMinimalPeer(senderId, currentRssi)
                    }
                }
            }
            BleManager.CMD_STATUS_UPDATE -> {
                if (data.size >= 5) {
                    ServiceState.currentChannel.postValue(data[1].toInt() and 0xFF)
                    val r = (data[2].toInt() and 0xFF) or ((data[3].toInt() and 0xFF) shl 8)
                    ServiceState.rssi.postValue(r.toShort().toInt())
                    ServiceState.snr.postValue(data[4].toInt())
                    if (data.size >= 6) {
                        val batRaw = data[5].toInt() and 0xFF
                        if (batRaw > 0) {
                            ServiceState.batteryVoltage.postValue(batRaw / 10.0f)
                        }
                    }
                }
            }
            BleManager.CMD_RECV_MESSAGE -> {
                if (data.size >= 4) {
                    val rssiVal = data[1].toInt()
                    val textEnd = data.indexOfFirst { it == 0.toByte() }.let { if (it < 2) data.size else it }
                    val text = String(data, 2, textEnd - 2, Charsets.UTF_8)
                    var senderId = "??"
                    if (textEnd + 2 < data.size) {
                        senderId = String.format("%02X%02X", data[textEnd + 1].toInt() and 0xFF, data[textEnd + 2].toInt() and 0xFF)
                    }
                    // Найти позывной отправителя из peers
                    val senderName = ServiceState.peers.value
                        ?.find { it.deviceId.endsWith(senderId) }?.callSign ?: "TX-$senderId"
                    val now = System.currentTimeMillis()
                    val msg = ChatMessage(
                        id = now,
                        text = text, isOutgoing = false,
                        senderId = senderId, senderName = senderName,
                        rssi = rssiVal, status = MessageStatus.DELIVERED,
                        time = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault())
                            .format(java.util.Date()),
                        timeMs = now
                    )
                    val list = ServiceState.messages.value?.toMutableList() ?: mutableListOf()
                    list.add(msg)
                    ServiceState.messages.postValue(list)
                    saveMessages()

                    if (senderId != "??") {
                        addMinimalPeer(senderId, rssiVal)
                    }
                }
            }
            BleManager.CMD_MESSAGE_ACK -> {
                val ackSeq = data[1].toInt() and 0xFF
                Log.d(TAG, "Message ACK seq=$ackSeq")
                // Найти сообщение с этим seq и пометить как доставленное
                val list = ServiceState.messages.value?.toMutableList() ?: return
                val idx = list.indexOfLast { it.isOutgoing && it.status == MessageStatus.SENDING &&
                    ((it.id % 256).toInt() == ackSeq || (it.timeMs % 256).toInt() == ackSeq) }
                if (idx >= 0) {
                    list[idx] = list[idx].copy(status = MessageStatus.DELIVERED)
                    ServiceState.messages.postValue(list)
                    saveMessages()
                }
            }
            BleManager.CMD_PEER_SEEN -> {
                if (data.size >= 28) {
                    val id = String.format("%02X%02X%02X%02X",
                        data[1].toInt() and 0xFF, data[2].toInt() and 0xFF,
                        data[3].toInt() and 0xFF, data[4].toInt() and 0xFF)
                    val cs = String(data, 5, 9, Charsets.UTF_8).trimEnd('\u0000')
                    // Координаты: байты 14-17=lat_e7, 18-21=lon_e7 (int32 LE)
                    val latE7 = getInt(data, 14)
                    val lonE7 = getInt(data, 18)
                    val pRssi = ((data[22].toInt() and 0xFF) or
                            ((data[23].toInt() and 0xFF) shl 8)).toShort().toInt()
                    val pSnr = data[24].toInt()
                    val txPwr = data[25].toInt() and 0xFF
                    val batt = data[26].toInt() and 0xFF

                    val lat = if (latE7 != 0 || lonE7 != 0) latE7 / 1e7 else null
                    val lon = if (latE7 != 0 || lonE7 != 0) lonE7 / 1e7 else null

                    Log.d(TAG, "PEER_SEEN: id=$id cs='$cs' rssi=$pRssi snr=$pSnr")

                    val list = ServiceState.peers.value?.toMutableList() ?: mutableListOf()
                    // Найти существующий peer (по точному ID или по последним 4 символам)
                    val shortId = id.takeLast(4)
                    val existing = list.find { it.deviceId == id || it.deviceId.endsWith(shortId) }
                    // Сохранить позывной если в beacon пусто, но ранее был известен
                    val finalCs = if (cs.isNotEmpty()) cs
                        else existing?.callSign?.takeIf { it != "TX-$shortId" }
                        ?: "TX-$shortId"
                    list.removeAll { it.deviceId == id || it.deviceId.endsWith(shortId) }
                    val peer = Peer(id, finalCs, pRssi, pSnr, txPwr,
                        if (batt == 0xFF) null else batt,
                        System.currentTimeMillis(), lat, lon)
                    list.add(peer)
                    ServiceState.peers.postValue(list)
                    savePeers(list)
                }
            }
            BleManager.CMD_GET_LOCATION -> {
                // ESP32 запрашивает координаты для beacon
                val lat = ServiceState.myLat.value
                val lon = ServiceState.myLon.value
                if (lat != null && lon != null) {
                    val latE7 = (lat * 1e7).toInt()
                    val lonE7 = (lon * 1e7).toInt()
                    bleManager.sendLocationUpdate(latE7, lonE7, 0)
                    Log.d(TAG, "Location sent: $lat, $lon")
                } else {
                    bleManager.sendLocationUpdate(0, 0, 0)
                    Log.d(TAG, "Location requested but GPS not available")
                }
            }
            BleManager.CMD_SETTINGS_RESP -> {
                val json = String(data, 1, data.size - 1, Charsets.UTF_8)
                Log.d(TAG, "Settings: $json")
                // Позывной: приоритет — локально сохранённый, иначе с девайса
                val saved = prefs.getString("my_callsign", null)
                if (!saved.isNullOrEmpty()) {
                    ServiceState.callSign.postValue(saved)
                    // Синхронизировать на девайс если отличается
                    try {
                        val doc = org.json.JSONObject(json)
                        val deviceCs = doc.optString("callsign", "")
                        if (deviceCs != saved) {
                            Log.d(TAG, "CallSign mismatch: device='$deviceCs' local='$saved', syncing to device")
                            bleManager.sendSettings("""{"callsign":"$saved"}""")
                        }
                    } catch (_: Exception) {}
                } else {
                    // Если локально пусто — взять с девайса
                    try {
                        val doc = org.json.JSONObject(json)
                        val deviceCs = doc.optString("callsign", "")
                        if (deviceCs.isNotEmpty() && !deviceCs.startsWith("TX-")) {
                            prefs.edit().putString("my_callsign", deviceCs).apply()
                            ServiceState.callSign.postValue(deviceCs)
                            Log.d(TAG, "CallSign loaded from device: $deviceCs")
                        }
                    } catch (_: Exception) {}
                }
            }
            BleManager.CMD_PIN_RESULT -> {
                if (data.size >= 2) {
                    val ok = data[1].toInt() == 1
                    if (ok) {
                        Log.d(TAG, "PIN accepted!")
                        pendingDeviceAddr?.let { authorizeDevice(it) }
                        bleManager.onConnected?.invoke()
                    } else {
                        Log.d(TAG, "PIN rejected!")
                        ServiceState.statusMessage.postValue("Неверный PIN")
                        bleManager.disconnect()
                    }
                }
            }
            BleManager.CMD_FILE_PROGRESS -> {
                if (data.size >= 6) {
                    val sid = data[1].toInt() and 0xFF
                    val done = (data[2].toInt() and 0xFF) or ((data[3].toInt() and 0xFF) shl 8)
                    val total = (data[4].toInt() and 0xFF) or ((data[5].toInt() and 0xFF) shl 8)
                    val list = ServiceState.fileTransfers.value?.toMutableList() ?: mutableListOf()
                    val idx = list.indexOfFirst { it.sessionId == sid }
                    if (idx >= 0) {
                        list[idx] = list[idx].copy(chunksDone = done, status = FileStatus.TRANSFERRING)
                        ServiceState.fileTransfers.postValue(list)
                    }
                }
            }
            BleManager.CMD_FILE_UPLOAD_STATUS -> {
                // File v2: статус от ESP32 — 0=ACCEPTED, 1=BUSY, 2=SENDING, 3=DELIVERED, 4=FAILED, 5=NO_MEMORY
                if (data.size >= 3) {
                    val status = data[1].toInt() and 0xFF
                    val sid = data[2].toInt() and 0xFF
                    Log.d(TAG, "UPLOAD_STATUS: status=$status session=$sid")
                    val list = ServiceState.fileTransfers.value?.toMutableList() ?: mutableListOf()
                    // Найти по последнему исходящему файлу
                    val idx = list.indexOfLast { it.isOutgoing && it.status == FileStatus.TRANSFERRING }
                    if (idx >= 0) {
                        when (status) {
                            1 -> { // BUSY
                                list[idx] = list[idx].copy(status = FileStatus.FAILED)
                                ServiceState.statusMessage.postValue("Устройство занято")
                            }
                            3 -> { // DELIVERED
                                list[idx] = list[idx].copy(status = FileStatus.DONE)
                            }
                            4 -> { // FAILED
                                list[idx] = list[idx].copy(status = FileStatus.FAILED)
                            }
                            5 -> { // NO_MEMORY
                                list[idx] = list[idx].copy(status = FileStatus.FAILED)
                                ServiceState.statusMessage.postValue("Файл слишком большой")
                            }
                        }
                        ServiceState.fileTransfers.postValue(list)
                    }
                }
            }
            BleManager.CMD_FILE_RECV -> {
                // Новый формат: cmd(1)+type(1)+size(4)+chunks(1)+sender(2)+name(20)=29
                // Старый формат: cmd(1)+type(1)+size(4)+chunks(1)+name(20)=27
                if (data.size >= 7) {
                    incomingFileType = data[1].toInt() and 0xFF
                    incomingFileSize = (data[2].toInt() and 0xFF) or
                        ((data[3].toInt() and 0xFF) shl 8) or
                        ((data[4].toInt() and 0xFF) shl 16) or
                        ((data[5].toInt() and 0xFF) shl 24)
                    if (data.size >= 29) {
                        // Новый формат с sender
                        val senderId = String.format("%02X%02X", data[7].toInt() and 0xFF, data[8].toInt() and 0xFF)
                        incomingFileName = String(data, 9, 20.coerceAtMost(data.size - 9), Charsets.UTF_8).trimEnd('\u0000')
                        incomingFileSender = senderId
                        val currentRssi = ServiceState.rssi.value ?: 0
                        addMinimalPeer(senderId, currentRssi)
                    } else if (data.size >= 27) {
                        // Старый формат без sender
                        incomingFileName = String(data, 7, 20, Charsets.UTF_8).trimEnd('\u0000')
                        incomingFileSender = ""
                    } else {
                        incomingFileName = ""
                        incomingFileSender = ""
                    }
                    incomingFileBuffer = ByteArray(incomingFileSize)
                    incomingFileOffset = 0
                    if (incomingFileType == 0x05) {
                        ServiceState.isReceivingFile.postValue(true)
                    }
                    Log.d(TAG, "File recv header: type=$incomingFileType size=$incomingFileSize name=$incomingFileName sender=$incomingFileSender")
                }
            }
            BleManager.CMD_FILE_DATA -> {
                // Чанк данных: cmd(1) + data
                if (data.size > 1 && incomingFileBuffer != null) {
                    val chunk = data.copyOfRange(1, data.size)
                    val remaining = incomingFileSize - incomingFileOffset
                    val toCopy = chunk.size.coerceAtMost(remaining)
                    System.arraycopy(chunk, 0, incomingFileBuffer!!, incomingFileOffset, toCopy)
                    incomingFileOffset += toCopy

                    // Файл полностью получен
                    if (incomingFileOffset >= incomingFileSize) {
                        val fileData = incomingFileBuffer!!
                        val fileType = incomingFileType

                        var recvFileName = if (incomingFileName.isNotEmpty()) {
                            // Добавить расширение если отсутствует
                            val n = incomingFileName
                            if (!n.contains('.')) {
                                when (fileType) { 0x01 -> "$n.jpg"; 0x02 -> "$n.txt"; else -> "$n.bin" }
                            } else n
                        } else when (fileType) {
                            0x01 -> "photo_received.jpg"
                            0x02 -> "text_received.txt"
                            else -> "file_received.bin"
                        }

                        var senderName = "??"
                        if (incomingFileSender.isNotEmpty()) {
                            senderName = ServiceState.peers.value
                                ?.find { it.deviceId.endsWith(incomingFileSender) }?.callSign
                                ?: "TX-$incomingFileSender"
                        } else {
                            val peers = ServiceState.peers.value
                            if (!peers.isNullOrEmpty()) {
                                val recent = peers.maxByOrNull { it.lastSeenMs }
                                if (recent != null) senderName = recent.callSign
                            }
                        }

                        if (fileType == 0x05) {
                            ServiceState.isReceivingFile.postValue(false)
                        }
                        Log.d(TAG, "File complete: type=$fileType size=${fileData.size} from=$senderName")
                        val timeMs = System.currentTimeMillis()
                        val localPath = saveFileData(recvFileName, timeMs, fileData)

                        if (fileType == 0x05) {
                            // PTT адресный голос — автовоспроизведение, не хранить
                            val expectedSize = incomingFileSize
                            val isPartial = fileData.size < expectedSize
                            if (isPartial) {
                                Log.w(TAG, "PTT voice partial: ${fileData.size}/$expectedSize bytes from $senderName")
                                // Показать "ошибка" но попытаться воспроизвести
                                ServiceState.statusMessage.postValue("ошибка приёма")
                            } else {
                                Log.d(TAG, "PTT voice received: ${fileData.size} bytes from $senderName")
                            }
                            // Воспроизвести в любом случае (даже битый)
                            playVoiceMessage(fileData)
                            deleteFileData(localPath)
                        } else if (fileType == 0x04) {
                            // Голосовое сообщение из чата — добавить в чат, не в файлы
                            val durationSec = fileData.size / Codec2Wrapper.FRAME_BYTES *
                                Codec2Wrapper.FRAME_SAMPLES / 8000
                            val msg = ChatMessage(
                                id = timeMs, text = "\uD83C\uDFA4 ${durationSec}s",
                                isOutgoing = false, senderId = incomingFileSender.ifEmpty { "??" },
                                senderName = senderName, destId = null, destName = null,
                                rssi = ServiceState.rssi.value,
                                status = MessageStatus.DELIVERED,
                                time = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault())
                                    .format(java.util.Date()),
                                timeMs = timeMs,
                                voicePath = localPath
                            )
                            val msgList = ServiceState.messages.value?.toMutableList() ?: mutableListOf()
                            msgList.add(msg)
                            ServiceState.messages.postValue(msgList)
                            saveMessages()
                        } else {
                            val transfer = FileTransfer(
                                sessionId = 0, fileName = "$recvFileName от $senderName", fileType = fileType,
                                totalSize = fileData.size, chunksTotal = 0, chunksDone = 0,
                                isOutgoing = false, status = FileStatus.DONE, timeMs = timeMs,
                                data = fileData, localPath = localPath
                            )
                            val list = ServiceState.fileTransfers.value?.toMutableList() ?: mutableListOf()
                            list.add(0, transfer)
                            ServiceState.fileTransfers.postValue(list)
                            saveFileTransfers()
                        }

                        incomingFileBuffer = null
                    }
                }
            }
            BleManager.CMD_INCOMING_CALL -> {
                if (data.size >= 24) {
                    val callTypeCode = data[1].toInt() and 0xFF
                    val ct = CallType.fromCode(callTypeCode)
                    val sid = String.format("%02X%02X%02X%02X",
                        data[2].toInt() and 0xFF, data[3].toInt() and 0xFF,
                        data[4].toInt() and 0xFF, data[5].toInt() and 0xFF)
                    val cs = String(data, 6, 9, Charsets.UTF_8).trimEnd('\u0000')
                    val latE7 = getInt(data, 15)
                    val lonE7 = getInt(data, 19)
                    val lat = if (latE7 != 0 || lonE7 != 0) latE7 / 1e7 else null
                    val lon = if (latE7 != 0 || lonE7 != 0) lonE7 / 1e7 else null
                    val seq = data[23].toInt() and 0xFF
                    val currentRssi = ServiceState.rssi.value ?: 0

                    // Обновить/добавить peer из вызова
                    val peerList = ServiceState.peers.value?.toMutableList() ?: mutableListOf()
                    val existingPeer = peerList.find { it.deviceId == sid || it.deviceId.endsWith(sid.takeLast(4)) }
                    if (existingPeer != null) {
                        peerList.remove(existingPeer)
                        peerList.add(existingPeer.copy(
                            deviceId = sid, // обновить на полный ID
                            callSign = if (cs.isNotEmpty()) cs else existingPeer.callSign,
                            rssi = currentRssi,
                            lastSeenMs = System.currentTimeMillis(),
                            lat = lat ?: existingPeer.lat,
                            lon = lon ?: existingPeer.lon
                        ))
                    } else {
                        peerList.add(Peer(
                            deviceId = sid,
                            callSign = if (cs.isNotEmpty()) cs else "TX-${sid.takeLast(4)}",
                            rssi = currentRssi, snr = 0, txPower = 0,
                            batteryPct = null,
                            lastSeenMs = System.currentTimeMillis(),
                            lat = lat, lon = lon
                        ))
                    }
                    ServiceState.peers.postValue(peerList)
                    savePeers(peerList)

                    val listenMode = ServiceState.listenMode.value ?: ListenMode.ALL
                    val shouldShow = when {
                        ct == CallType.EMERGENCY -> true
                        ct == CallType.PRIVATE -> true
                        listenMode == ListenMode.ALL -> true
                        else -> false
                    }

                    if (shouldShow && ServiceState.incomingCall.value == null) {
                        val call = IncomingCall(ct, sid, cs, lat, lon, seq, currentRssi)
                        ServiceState.incomingCall.postValue(call)
                        addRecentCall(RecentCall(sid, cs, false, ct.name, rssi = currentRssi))
                        launchIncomingCallScreen(call)
                    }
                }
            }
            BleManager.CMD_CALL_STATUS -> {
                if (data.size >= 2) {
                    val status = data[1].toInt() and 0xFF
                    // 3=TIMEOUT, 4=CANCELLED → очистить
                    if (status >= 3) {
                        ServiceState.incomingCall.postValue(null)
                        ServiceState.callActive.postValue(false)
                    }
                }
            }
        }
    }

    // === Peers persistence ===

    fun savePeers(peerList: List<Peer>? = null) {
        val peers = peerList ?: ServiceState.peers.value ?: return
        val json = peers.joinToString(";") { p ->
            "${p.deviceId},${p.callSign},${p.rssi},${p.snr},${p.txPower}," +
            "${p.batteryPct ?: -1},${p.lastSeenMs},${p.lat ?: ""},${p.lon ?: ""}"
        }
        prefs.edit().putString("saved_peers", json).commit()
    }

    private fun loadSavedPeers() {
        val json = prefs.getString("saved_peers", null) ?: return
        val timeoutMs = (ServiceState.peerTimeoutMin.value ?: 60) * 60_000L
        val now = System.currentTimeMillis()
        val peers = json.split(";").mapNotNull { entry ->
            val p = entry.split(",")
            if (p.size < 7) return@mapNotNull null
            val lastSeen = p[6].toLongOrNull() ?: return@mapNotNull null
            if (now - lastSeen > timeoutMs) return@mapNotNull null // устарел
            // Отбросить peers с испорченными ID (от старого бага String.format без and 0xFF)
            if (p[0].contains("FFFFFF")) return@mapNotNull null
            Peer(
                deviceId = p[0], callSign = p[1],
                rssi = p[2].toIntOrNull() ?: 0,
                snr = p[3].toIntOrNull() ?: 0,
                txPower = p[4].toIntOrNull() ?: 0,
                batteryPct = p[5].toIntOrNull()?.let { if (it < 0) null else it },
                lastSeenMs = lastSeen,
                lat = p.getOrNull(7)?.toDoubleOrNull(),
                lon = p.getOrNull(8)?.toDoubleOrNull()
            )
        }
        if (peers.isNotEmpty()) {
            ServiceState.peers.value = peers
            Log.d(TAG, "Loaded ${peers.size} saved peers")
        }
    }

    private fun startPeerCleanup() {
        // Каждые 60 сек — удалить устаревших, сохранить
        handler.postDelayed(object : Runnable {
            override fun run() {
                cleanupPeers()
                savePeers()
                handler.postDelayed(this, 60_000)
            }
        }, 60_000)
    }

    private fun cleanupPeers() {
        val timeoutMs = (ServiceState.peerTimeoutMin.value ?: 60) * 60_000L
        val now = System.currentTimeMillis()
        val list = ServiceState.peers.value?.toMutableList() ?: return
        val groupIds = getGroupMemberIds()
        val before = list.size
        list.removeAll { peer ->
            (now - peer.lastSeenMs > timeoutMs) && !groupIds.contains(peer.deviceId)
        }
        if (list.size != before) {
            ServiceState.peers.postValue(list)
            savePeers()
            Log.d(TAG, "Peers cleanup: $before → ${list.size}")
        }
    }

    // === Recent calls ===

    fun addRecentCall(call: RecentCall) {
        val list = ServiceState.recentCalls.value?.toMutableList() ?: mutableListOf()
        // Убрать устаревшие типы
        list.removeAll { it.callType == "SOS" }
        // Дедупликация: ALL — по callType, остальные — по deviceId (endsWith)
        if (call.callType == "ALL") {
            list.removeAll { it.callType == "ALL" }
        } else {
            val shortId = call.deviceId.takeLast(4)
            list.removeAll { it.deviceId.endsWith(shortId) && it.callType == call.callType }
        }
        list.add(0, call) // новый сверху
        // Максимум 20
        while (list.size > 20) list.removeAt(list.size - 1)
        ServiceState.recentCalls.postValue(list)
        saveRecentCalls(list)
    }

    private fun saveRecentCalls(calls: List<RecentCall>) {
        val json = calls.joinToString("\n") { c ->
            "${c.deviceId}\t${c.callSign}\t${c.isOutgoing}\t${c.callType}\t${c.groupName ?: ""}\t${c.rssi ?: ""}\t${c.timeMs}"
        }
        prefs.edit().putString("recent_calls", json).commit()
    }

    private fun loadRecentCalls() {
        val json = prefs.getString("recent_calls", null) ?: return
        val calls = json.split("\n").mapNotNull { line ->
            val p = line.split("\t")
            if (p.size < 7) return@mapNotNull null
            if (p[3] == "SOS") return@mapNotNull null // убрать устаревшие
            RecentCall(
                deviceId = p[0], callSign = p[1],
                isOutgoing = p[2] == "true", callType = p[3],
                groupName = p[4].ifEmpty { null },
                rssi = p[5].toIntOrNull(),
                timeMs = p[6].toLongOrNull() ?: 0
            )
        }.toMutableList()
        // Дедупликация: оставить только последний ALL и по одному PRIVATE на deviceId
        val deduped = mutableListOf<RecentCall>()
        var hasAll = false
        val seenPrivate = mutableSetOf<String>()
        for (c in calls) {
            if (c.callType == "ALL") {
                if (!hasAll) { deduped.add(c); hasAll = true }
            } else {
                val key = c.deviceId.takeLast(4)
                if (key !in seenPrivate) { deduped.add(c); seenPrivate.add(key) }
            }
        }
        ServiceState.recentCalls.value = deduped
        if (deduped.size != calls.size) saveRecentCalls(deduped)
    }

    // === File transfers persistence ===

    private fun updateFileProgress(sessionId: Int, chunksDone: Int, newStatus: FileStatus?) {
        val list = ArrayList(ServiceState.fileTransfers.value ?: emptyList())
        val idx = list.indexOfFirst { it.sessionId == sessionId && it.isOutgoing }
        if (idx >= 0) {
            list[idx] = list[idx].copy(
                chunksDone = chunksDone,
                status = newStatus ?: list[idx].status
            )
            ServiceState.fileTransfers.postValue(list)
            if (newStatus == FileStatus.DONE || newStatus == FileStatus.FAILED) saveFileTransfers()
        }
    }

    fun saveFileTransfers() {
        val transfers = ServiceState.fileTransfers.value ?: return
        val json = transfers.filter { it.status == FileStatus.DONE || it.status == FileStatus.FAILED }
            .joinToString("\n") { t ->
                "${t.sessionId}\t${t.fileName}\t${t.fileType}\t${t.totalSize}\t${t.chunksTotal}\t" +
                "${t.chunksDone}\t${t.isOutgoing}\t${t.status.name}\t${t.timeMs}\t${t.localPath ?: ""}\t" +
                "${t.destMac?.joinToString(",") { (it.toInt() and 0xFF).toString() } ?: ""}\t${t.destName ?: ""}"
            }
        prefs.edit().putString("file_transfers", json).commit() // commit вместо apply — гарантирует запись
    }

    private fun loadFileTransfers() {
        val json = prefs.getString("file_transfers", null) ?: return
        val maxAgeMs = (ServiceState.fileHistoryDays.value ?: 30) * 86_400_000L
        val now = System.currentTimeMillis()
        val transfers = json.split("\n").mapNotNull { line ->
            val p = line.split("\t")
            if (p.size < 9) return@mapNotNull null
            val timeMs = p[8].toLongOrNull() ?: return@mapNotNull null
            if (now - timeMs > maxAgeMs) {
                // Устарел — удалить файл с диска
                val oldPath = p.getOrNull(9)
                if (!oldPath.isNullOrEmpty()) deleteFileData(oldPath)
                return@mapNotNull null
            }
            val localPath = p.getOrNull(9)?.ifEmpty { null }
            // Проверить что файл существует
            val validPath = if (localPath != null && java.io.File(localPath).exists()) localPath else null
            val destMacStr = p.getOrNull(10)?.ifEmpty { null }
            val destMac = destMacStr?.split(",")?.let { parts ->
                if (parts.size == 2) byteArrayOf(parts[0].toInt().toByte(), parts[1].toInt().toByte()) else null
            }
            val destName = p.getOrNull(11)?.ifEmpty { null }
            FileTransfer(
                sessionId = p[0].toIntOrNull() ?: 0,
                fileName = p[1],
                fileType = p[2].toIntOrNull() ?: 0,
                totalSize = p[3].toIntOrNull() ?: 0,
                chunksTotal = p[4].toIntOrNull() ?: 0,
                chunksDone = p[5].toIntOrNull() ?: 0,
                isOutgoing = p[6] == "true",
                status = try { FileStatus.valueOf(p[7]) } catch (_: Exception) { FileStatus.DONE },
                timeMs = timeMs,
                localPath = validPath,
                destMac = destMac,
                destName = destName
            )
        }
        if (transfers.isNotEmpty()) {
            ServiceState.fileTransfers.value = transfers
            Log.d(TAG, "Loaded ${transfers.size} file transfers")
        }
    }

    /** Device IDs привязанные к группам — не удалять при cleanup */
    private fun getGroupMemberIds(): Set<String> {
        val json = prefs.getString("saved_groups", null) ?: return emptySet()
        val ids = mutableSetOf<String>()
        json.split(";").forEach { entry ->
            val parts = entry.split(":")
            if (parts.size == 2) {
                parts[1].split(",").forEach { ids.add(it.trim()) }
            }
        }
        return ids
    }

    /** Добавить peer с минимальной информацией (из текста/аудио, не beacon) */
    private fun addMinimalPeer(shortId: String, rssi: Int) {
        val list = ServiceState.peers.value?.toMutableList() ?: mutableListOf()
        val existing = list.find { it.deviceId.endsWith(shortId) }
        if (existing != null) {
            // Обновить RSSI, SNR и время (callSign и deviceId сохраняются от beacon)
            val snr = ServiceState.snr.value ?: 0
            list.remove(existing)
            list.add(existing.copy(rssi = rssi, snr = snr, lastSeenMs = System.currentTimeMillis()))
        } else {
            // Новый peer — знаем только последние 2 байта MAC
            list.add(Peer(
                deviceId = "0000$shortId",
                callSign = "TX-$shortId",
                rssi = rssi, snr = 0, txPower = 0,
                batteryPct = null,
                lastSeenMs = System.currentTimeMillis()
            ))
        }
        ServiceState.peers.postValue(list)
        savePeers(list)
    }

    private fun getInt(data: ByteArray, offset: Int): Int {
        return (data[offset].toInt() and 0xFF) or
               ((data[offset + 1].toInt() and 0xFF) shl 8) or
               ((data[offset + 2].toInt() and 0xFF) shl 16) or
               ((data[offset + 3].toInt() and 0xFF) shl 24)
    }

    // === GPS forwarding to ESP32 ===

    private var lastGpsSentMs = 0L

    private fun setupGpsForwarding() {
        ServiceState.myLat.observeForever { sendGpsToDevice() }
    }

    private fun sendGpsToDevice() {
        val now = System.currentTimeMillis()
        if (now - lastGpsSentMs < 10_000) return // не чаще раз в 10 сек
        val lat = ServiceState.myLat.value ?: return
        val lon = ServiceState.myLon.value ?: return
        if (ServiceState.connectionState.value != BleState.CONNECTED) return
        val latE7 = (lat * 1e7).toInt()
        val lonE7 = (lon * 1e7).toInt()
        bleManager.sendLocationUpdate(latE7, lonE7, 0)
        lastGpsSentMs = now
        Log.d(TAG, "GPS sent to device: $lat, $lon")
    }

    // === Messages persistence ===

    fun saveMessages() {
        val msgs = ServiceState.messages.value ?: return
        val json = msgs.joinToString("\n") { m ->
            "${m.id}\t${m.text.replace("\t", " ").replace("\n", " ")}\t${m.isOutgoing}\t${m.senderId}\t" +
            "${m.senderName}\t${m.destId ?: ""}\t${m.destName ?: ""}\t${m.rssi ?: ""}\t" +
            "${m.status.name}\t${m.time}\t${m.timeMs}\t${m.voicePath ?: ""}"
        }
        prefs.edit().putString("saved_messages", json).commit()
    }

    private fun loadMessages() {
        val json = prefs.getString("saved_messages", null) ?: return
        val maxAgeMs = (ServiceState.messageHistoryDays.value ?: 30) * 86_400_000L
        val now = System.currentTimeMillis()
        val msgs = json.split("\n").mapNotNull { line ->
            val p = line.split("\t")
            if (p.size < 11) return@mapNotNull null
            val timeMs = p[10].toLongOrNull() ?: return@mapNotNull null
            if (now - timeMs > maxAgeMs) return@mapNotNull null
            val voicePath = p.getOrNull(11)?.ifEmpty { null }
            // Проверить что voice файл существует
            val validVoice = if (voicePath != null && java.io.File(voicePath).exists()) voicePath else null
            ChatMessage(
                id = p[0].toLongOrNull() ?: 0,
                text = p[1],
                isOutgoing = p[2] == "true",
                senderId = p[3],
                senderName = p[4],
                destId = p[5].ifEmpty { null },
                destName = p[6].ifEmpty { null },
                rssi = p[7].toIntOrNull(),
                status = try { MessageStatus.valueOf(p[8]) } catch (_: Exception) { MessageStatus.DELIVERED },
                time = p[9],
                timeMs = timeMs,
                voicePath = validVoice
            )
        }
        if (msgs.isNotEmpty()) {
            ServiceState.messages.value = msgs
            Log.d(TAG, "Loaded ${msgs.size} messages")
        }
    }

    // === Auth ===

    private fun isDeviceAuthorized(addr: String) = prefs.getBoolean("auth_$addr", false)
    private fun authorizeDevice(addr: String) = prefs.edit().putBoolean("auth_$addr", true).apply()

    // === Notifications ===

    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(NOTIF_CHANNEL, "MeshTRX", NotificationManager.IMPORTANCE_LOW)
            channel.description = "Фоновая связь MeshTRX"
            (getSystemService(NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(channel)
        }
    }

    private fun buildNotification(text: String): Notification {
        val intent = Intent(this, MainActivity::class.java).apply {
            flags = Intent.FLAG_ACTIVITY_SINGLE_TOP
        }
        val pi = PendingIntent.getActivity(this, 0, intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)

        return NotificationCompat.Builder(this, NOTIF_CHANNEL)
            .setContentTitle("MeshTRX")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentIntent(pi)
            .setOngoing(true)
            .build()
    }

    private fun updateNotification(text: String) {
        val notif = buildNotification(text)
        (getSystemService(NOTIFICATION_SERVICE) as NotificationManager).notify(NOTIF_ID, notif)
    }

    override fun onBind(intent: Intent): IBinder = binder

    override fun onDestroy() {
        stopReconnect()
        savePeers()
        saveFileTransfers()
        saveMessages()
        audioEngine.destroy()
        bleManager.disconnect()
        Log.d(TAG, "Service destroyed")
        super.onDestroy()
    }
}
