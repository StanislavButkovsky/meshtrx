package com.meshtrx.app

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import java.util.UUID

@SuppressLint("MissingPermission")
class BleManager(private val context: Context) {

    companion object {
        private const val TAG = "BleManager"
        val SERVICE_UUID: UUID = UUID.fromString("6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
        val RX_CHAR_UUID: UUID = UUID.fromString("6E400002-B5A3-F393-E0A9-E50E24DCCA9E")
        val TX_CHAR_UUID: UUID = UUID.fromString("6E400003-B5A3-F393-E0A9-E50E24DCCA9E")

        const val CMD_AUDIO_TX = 0x01
        const val CMD_AUDIO_RX = 0x02
        const val CMD_PTT_START = 0x03
        const val CMD_PTT_END = 0x04
        const val CMD_SET_CHANNEL = 0x05
        const val CMD_STATUS_UPDATE = 0x06
        const val CMD_SEND_MESSAGE = 0x07
        const val CMD_RECV_MESSAGE = 0x08
        const val CMD_MESSAGE_ACK = 0x09
        const val CMD_SET_SETTINGS = 0x0A
        const val CMD_GET_SETTINGS = 0x0B
        const val CMD_SETTINGS_RESP = 0x0C
        const val CMD_FILE_START = 0x0D
        const val CMD_FILE_CHUNK = 0x0E
        const val CMD_FILE_RECV = 0x0F
        const val CMD_FILE_PROGRESS = 0x10
        const val CMD_SET_TX_MODE = 0x11
        const val CMD_VOX_STATUS = 0x12
        const val CMD_VOX_LEVEL = 0x13
        const val CMD_GET_LOCATION = 0x14
        const val CMD_LOCATION_UPD = 0x15
        const val CMD_BEACON_SENT = 0x16
        const val CMD_PEER_SEEN = 0x17
        const val CMD_CALL_ALL = 0x18
        const val CMD_CALL_PRIVATE = 0x19
        const val CMD_CALL_GROUP = 0x1A
        const val CMD_CALL_EMERGENCY = 0x1B
        const val CMD_CALL_ACCEPT = 0x1C
        const val CMD_CALL_REJECT = 0x1D
        const val CMD_CALL_CANCEL = 0x1E
        const val CMD_INCOMING_CALL = 0x1F
        const val CMD_CALL_STATUS = 0x20
        const val CMD_PIN_CHECK = 0x25
        const val CMD_PIN_RESULT = 0x26
        const val CMD_FILE_DATA = 0x27
        const val CMD_SET_REPEATER = 0x28
        const val CMD_FILE_END = 0x29
        // File Transfer v2
        const val CMD_FILE_UPLOAD_START = 0x30
        const val CMD_FILE_UPLOAD_DATA = 0x31
        const val CMD_FILE_UPLOAD_STATUS = 0x32
    }

    private val bluetoothAdapter: BluetoothAdapter? =
        (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
    private var bluetoothGatt: BluetoothGatt? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private val handler = Handler(Looper.getMainLooper())
    private var scanning = false

    var onConnected: (() -> Unit)? = null
    var onDisconnected: (() -> Unit)? = null
    var onDataReceived: ((ByteArray) -> Unit)? = null
    var onScanResult: ((BluetoothDevice, Int) -> Unit)? = null
    var onNeedPin: (() -> Unit)? = null  // запрос PIN от пользователя

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name ?: return
            if (name.startsWith("MeshTRX-")) {
                onScanResult?.invoke(result.device, result.rssi)
            }
        }
    }

    fun startScan() {
        if (scanning) return
        val scanner = bluetoothAdapter?.bluetoothLeScanner ?: return
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SERVICE_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()
        scanner.startScan(listOf(filter), settings, scanCallback)
        scanning = true
        Log.d(TAG, "Scan started")
        handler.postDelayed({ stopScan() }, 10000)
    }

    fun stopScan() {
        if (!scanning) return
        try { bluetoothAdapter?.bluetoothLeScanner?.stopScan(scanCallback) } catch (_: Exception) {}
        scanning = false
        Log.d(TAG, "Scan stopped")
    }

    private val connectTimeoutRunnable = Runnable {
        if (bluetoothGatt != null && rxCharacteristic == null) {
            Log.w(TAG, "Connect timeout, closing GATT")
            try { bluetoothGatt?.close() } catch (_: Exception) {}
            bluetoothGatt = null
            handler.post { onDisconnected?.invoke() }
        }
    }

    fun connect(device: BluetoothDevice) {
        stopScan()
        handler.removeCallbacks(connectTimeoutRunnable)
        bluetoothGatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
        Log.d(TAG, "Connecting to ${device.name}")
        handler.postDelayed(connectTimeoutRunnable, 10_000)  // 10 сек таймаут
    }

    fun disconnect() {
        bluetoothGatt?.disconnect()
        bluetoothGatt?.close()
        bluetoothGatt = null
        rxCharacteristic = null
        txCharacteristic = null
    }

    fun isConnected(): Boolean = bluetoothGatt != null && rxCharacteristic != null

    @Synchronized
    fun send(data: ByteArray): Boolean {
        val rx = rxCharacteristic ?: return false
        val gatt = bluetoothGatt ?: return false
        return try {
            rx.value = data
            rx.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            gatt.writeCharacteristic(rx)
        } catch (e: Exception) {
            Log.e(TAG, "send failed: ${e.message}")
            false
        }
    }

    fun sendAudioData(codec2Data: ByteArray) {
        val pkt = ByteArray(1 + codec2Data.size)
        pkt[0] = CMD_AUDIO_TX.toByte()
        System.arraycopy(codec2Data, 0, pkt, 1, codec2Data.size)
        send(pkt)
    }

    fun sendPttStart() = send(byteArrayOf(CMD_PTT_START.toByte()))
    fun sendPttEnd() = send(byteArrayOf(CMD_PTT_END.toByte()))
    fun setChannel(ch: Int) = send(byteArrayOf(CMD_SET_CHANNEL.toByte(), ch.toByte()))

    fun sendMessage(seq: Int, destId: String?, text: String) {
        val textBytes = text.toByteArray(Charsets.UTF_8)
        val pkt = ByteArray(4 + textBytes.size)
        pkt[0] = CMD_SEND_MESSAGE.toByte()
        pkt[1] = seq.toByte()
        // dest: 2 байта (0x0000 = broadcast)
        if (destId != null && destId.length >= 4) {
            pkt[2] = destId.substring(0, 2).toInt(16).toByte()
            pkt[3] = destId.substring(2, 4).toInt(16).toByte()
        } else {
            pkt[2] = 0; pkt[3] = 0 // broadcast
        }
        System.arraycopy(textBytes, 0, pkt, 4, textBytes.size)
        send(pkt)
    }

    fun sendSettings(json: String) {
        val jsonBytes = json.toByteArray(Charsets.UTF_8)
        val pkt = ByteArray(1 + jsonBytes.size)
        pkt[0] = CMD_SET_SETTINGS.toByte()
        System.arraycopy(jsonBytes, 0, pkt, 1, jsonBytes.size)
        send(pkt)
    }

    fun requestSettings() = send(byteArrayOf(CMD_GET_SETTINGS.toByte()))

    fun sendRepeaterConfig(enable: Boolean, ssid: String = "", password: String = "", ip: String = "") {
        val ssidBytes = ssid.toByteArray(Charsets.UTF_8)
        val passBytes = password.toByteArray(Charsets.UTF_8)
        val ipBytes = ip.toByteArray(Charsets.UTF_8)
        val pkt = ByteArray(2 + ssidBytes.size + 1 + passBytes.size + 1 + ipBytes.size + 1)
        var offset = 0
        pkt[offset++] = CMD_SET_REPEATER.toByte()
        pkt[offset++] = if (enable) 1 else 0
        System.arraycopy(ssidBytes, 0, pkt, offset, ssidBytes.size); offset += ssidBytes.size
        pkt[offset++] = 0
        System.arraycopy(passBytes, 0, pkt, offset, passBytes.size); offset += passBytes.size
        pkt[offset++] = 0
        System.arraycopy(ipBytes, 0, pkt, offset, ipBytes.size); offset += ipBytes.size
        pkt[offset] = 0
        send(pkt)
    }

    /** Отправить PIN для проверки */
    fun sendPinCheck(pin: Int) {
        val pkt = ByteArray(5)
        pkt[0] = CMD_PIN_CHECK.toByte()
        pkt[1] = (pin and 0xFF).toByte()
        pkt[2] = ((pin shr 8) and 0xFF).toByte()
        pkt[3] = ((pin shr 16) and 0xFF).toByte()
        pkt[4] = ((pin shr 24) and 0xFF).toByte()
        send(pkt)
    }

    fun sendLocationUpdate(latE7: Int, lonE7: Int, altM: Short) {
        val pkt = ByteArray(11)
        pkt[0] = CMD_LOCATION_UPD.toByte()
        putInt(pkt, 1, latE7)
        putInt(pkt, 5, lonE7)
        putShort(pkt, 9, altM)
        send(pkt)
    }

    private fun putInt(arr: ByteArray, offset: Int, value: Int) {
        arr[offset] = (value and 0xFF).toByte()
        arr[offset + 1] = ((value shr 8) and 0xFF).toByte()
        arr[offset + 2] = ((value shr 16) and 0xFF).toByte()
        arr[offset + 3] = ((value shr 24) and 0xFF).toByte()
    }

    private fun putShort(arr: ByteArray, offset: Int, value: Short) {
        arr[offset] = (value.toInt() and 0xFF).toByte()
        arr[offset + 1] = ((value.toInt() shr 8) and 0xFF).toByte()
    }

    fun removeBond(device: BluetoothDevice) {
        try {
            val method = device.javaClass.getMethod("removeBond")
            method.invoke(device)
            Log.d(TAG, "Bond removed for ${device.name}")
        } catch (e: Exception) {
            Log.e(TAG, "removeBond failed: ${e.message}")
        }
    }

    fun getBondedDevices(): List<BluetoothDevice> {
        return bluetoothAdapter?.bondedDevices
            ?.filter { it.name?.startsWith("MeshTRX-") == true }
            ?.toList() ?: emptyList()
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "Connected, discovering services")
                handler.removeCallbacks(connectTimeoutRunnable)
                gatt.requestMtu(128)
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.d(TAG, "Disconnected (status=$status)")
                try { gatt.close() } catch (_: Exception) {}
                bluetoothGatt = null
                rxCharacteristic = null
                txCharacteristic = null
                handler.post { onDisconnected?.invoke() }
            }
        }

        override fun onMtuChanged(gatt: BluetoothGatt, mtu: Int, status: Int) {
            Log.d(TAG, "MTU changed to $mtu")
            gatt.discoverServices()
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) return
            val service = gatt.getService(SERVICE_UUID) ?: return
            rxCharacteristic = service.getCharacteristic(RX_CHAR_UUID)
            txCharacteristic = service.getCharacteristic(TX_CHAR_UUID)

            txCharacteristic?.let { tx ->
                gatt.setCharacteristicNotification(tx, true)
                val desc = tx.getDescriptor(UUID.fromString("00002902-0000-1000-8000-00805f9b34fb"))
                desc?.let {
                    it.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    gatt.writeDescriptor(it)
                }
            }
            Log.d(TAG, "Services discovered, requesting PIN")
            // Сообщить приложению что нужно ввести PIN
            handler.post { onNeedPin?.invoke() }
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (characteristic.uuid == TX_CHAR_UUID) {
                val data = characteristic.value
                if (data != null && data.isNotEmpty()) {
                    onDataReceived?.invoke(data)
                }
            }
        }
    }
}
