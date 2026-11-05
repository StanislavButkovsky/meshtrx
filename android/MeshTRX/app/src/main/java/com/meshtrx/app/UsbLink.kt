package com.meshtrx.app

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbConstants
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.os.Build
import android.util.Log
import java.io.ByteArrayOutputStream

/**
 * Связь с рацией по кабелю (OTG). По проводу идут те же пакеты, что по
 * Bluetooth, в рамках `0xC1 SLIP(пакет + CRC-16) 0xC0` — так их не спутать со
 * служебным журналом, который прошивка пишет в тот же порт.
 *
 * Драйверы свои и маленькие: на платах стоит всего два вида USB — мост CP2102
 * на V3 и встроенный в ESP32-S3 порт (CDC-ACM) на V4. Сторонняя библиотека
 * ради двух случаев не нужна.
 */
class UsbLink(private val context: Context) {
    companion object {
        private const val TAG = "UsbLink"
        private const val ACTION_PERMISSION = "com.meshtrx.app.USB_PERMISSION"
        private const val BAUD = 115200
        private const val KEEPALIVE_MS = 5000L
        private const val FRAME_START = 0xC1
        private const val FRAME_END = 0xC0
        private const val ESC = 0xDB

        private const val VID_SILABS = 0x10C4
        private const val PID_CP210X = 0xEA60
        private const val VID_ESPRESSIF = 0x303A
        private const val PID_ESP32S3 = 0x1001

        fun isRadio(d: UsbDevice) =
            (d.vendorId == VID_SILABS && d.productId == PID_CP210X) ||
                (d.vendorId == VID_ESPRESSIF && d.productId == PID_ESP32S3)

        fun crc16(data: ByteArray, len: Int = data.size): Int {
            var crc = 0xFFFF
            for (i in 0 until len) {
                crc = crc xor ((data[i].toInt() and 0xFF) shl 8)
                repeat(8) {
                    crc = if (crc and 0x8000 != 0) (crc shl 1) xor 0x1021 else crc shl 1
                    crc = crc and 0xFFFF
                }
            }
            return crc
        }

        fun encode(payload: ByteArray): ByteArray {
            val crc = crc16(payload)
            val out = ByteArrayOutputStream(payload.size * 2 + 6)
            out.write(FRAME_START)
            fun put(b: Int) {
                if (b == FRAME_START || b == FRAME_END || b == ESC) {
                    out.write(ESC); out.write(b xor 0x20)
                } else out.write(b)
            }
            payload.forEach { put(it.toInt() and 0xFF) }
            put(crc shr 8); put(crc and 0xFF)
            out.write(FRAME_END)
            return out.toByteArray()
        }
    }

    var onOpened: ((String) -> Unit)? = null
    var onClosed: (() -> Unit)? = null
    var onData: ((ByteArray) -> Unit)? = null

    private val manager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    @Volatile private var conn: UsbDeviceConnection? = null
    private var iface: UsbInterface? = null
    private var epIn: UsbEndpoint? = null
    private var epOut: UsbEndpoint? = null
    private var device: UsbDevice? = null
    @Volatile private var running = false
    private val writeLock = Any()

    val isOpen: Boolean get() = conn != null

    private val receiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, intent: Intent) {
            val dev = deviceFrom(intent)
            when (intent.action) {
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> dev?.let { if (isRadio(it)) request(it) }
                UsbManager.ACTION_USB_DEVICE_DETACHED ->
                    if (dev != null && dev.deviceName == device?.deviceName) close()
                ACTION_PERMISSION ->
                    if (dev != null && intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) open(dev)
                    else Log.d(TAG, "разрешение на USB не дано")
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun deviceFrom(intent: Intent): UsbDevice? =
        if (Build.VERSION.SDK_INT >= 33) intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
        else intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)

    fun start() {
        val f = IntentFilter().apply {
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
            addAction(ACTION_PERMISSION)
        }
        if (Build.VERSION.SDK_INT >= 33) context.registerReceiver(receiver, f, Context.RECEIVER_NOT_EXPORTED)
        else context.registerReceiver(receiver, f)
        Log.i(TAG, "start: USB devices " + manager.deviceList.values.joinToString { "%04x:%04x".format(it.vendorId, it.productId) })
        // Рация могла быть подключена ещё до запуска приложения
        manager.deviceList.values.firstOrNull { isRadio(it) }?.let { request(it) }
    }

    fun stop() {
        try { context.unregisterReceiver(receiver) } catch (_: Exception) {}
        close()
    }

    /** Открыть рацию, если её подключили: окно приложения зовёт это при
     *  запуске по USB_DEVICE_ATTACHED — система тогда уже дала разрешение. */
    fun probe() {
        if (isOpen) return
        manager.deviceList.values.firstOrNull { isRadio(it) }?.let { request(it) }
    }

    private fun request(d: UsbDevice) {
        if (isOpen) return
        Log.d(TAG, "рация на кабеле: ${d.deviceName}, разрешение ${manager.hasPermission(d)}")
        if (manager.hasPermission(d)) { open(d); return }
        val flags = if (Build.VERSION.SDK_INT >= 31) PendingIntent.FLAG_MUTABLE else 0
        val pi = PendingIntent.getBroadcast(context, 0,
            Intent(ACTION_PERMISSION).setPackage(context.packageName), flags)
        manager.requestPermission(d, pi)
    }

    @Synchronized
    private fun open(d: UsbDevice) {
        if (isOpen) return
        val c = manager.openDevice(d) ?: run { Log.w(TAG, "openDevice вернул null"); return }
        val ok = if (d.vendorId == VID_SILABS) setupCp210x(c, d) else setupCdcAcm(c, d)
        if (!ok) { Log.w(TAG, "не удалось настроить порт ${d.deviceName}"); c.close(); return }
        conn = c; device = d; running = true
        Thread(::readLoop, "meshtrx-usb-rx").start()
        Thread(::keepAliveLoop, "meshtrx-usb-ka").start()
        send(ByteArray(0))
        Log.d(TAG, "открыт ${d.deviceName} (${"%04x:%04x".format(d.vendorId, d.productId)})")
        onOpened?.invoke(if (d.vendorId == VID_SILABS) "Heltec V3" else "Heltec V4")
    }

    private fun claimBulk(c: UsbDeviceConnection, i: UsbInterface): Boolean {
        if (!c.claimInterface(i, true)) return false
        for (k in 0 until i.endpointCount) {
            val e = i.getEndpoint(k)
            if (e.type != UsbConstants.USB_ENDPOINT_XFER_BULK) continue
            if (e.direction == UsbConstants.USB_DIR_IN) epIn = e else epOut = e
        }
        iface = i
        return epIn != null && epOut != null
    }

    // CP2102: включить порт, 115200 8N1, и поднять DTR и RTS одним запросом.
    // Поднимать по очереди нельзя: эти линии подключены к сбросу платы, и
    // промежуточное сочетание её перезагружает.
    private fun setupCp210x(c: UsbDeviceConnection, d: UsbDevice): Boolean {
        val i = d.getInterface(0)
        if (!claimBulk(c, i)) return false
        val t = 0x41
        if (c.controlTransfer(t, 0x00, 0x0001, 0, null, 0, 1000) < 0) return false
        val baud = byteArrayOf((BAUD and 0xFF).toByte(), (BAUD shr 8 and 0xFF).toByte(),
            (BAUD shr 16 and 0xFF).toByte(), 0)
        c.controlTransfer(t, 0x1E, 0, 0, baud, 4, 1000)
        c.controlTransfer(t, 0x03, 0x0800, 0, null, 0, 1000)
        c.controlTransfer(t, 0x07, 0x0303, 0, null, 0, 1000)
        return true
    }

    // CDC-ACM (встроенный USB ESP32-S3): интерфейс управления и интерфейс данных
    private fun setupCdcAcm(c: UsbDeviceConnection, d: UsbDevice): Boolean {
        var comm: UsbInterface? = null
        var data: UsbInterface? = null
        for (k in 0 until d.interfaceCount) {
            val i = d.getInterface(k)
            when (i.interfaceClass) {
                UsbConstants.USB_CLASS_COMM -> if (comm == null) comm = i
                UsbConstants.USB_CLASS_CDC_DATA -> if (data == null) data = i
            }
        }
        if (comm == null || data == null) return false
        c.claimInterface(comm, true)
        if (!claimBulk(c, data)) return false
        val coding = byteArrayOf((BAUD and 0xFF).toByte(), (BAUD shr 8 and 0xFF).toByte(),
            (BAUD shr 16 and 0xFF).toByte(), 0, 0, 0, 8)
        c.controlTransfer(0x21, 0x20, 0, comm.id, coding, coding.size, 1000)
        c.controlTransfer(0x21, 0x22, 0x0003, comm.id, null, 0, 1000)
        return true
    }

    fun send(payload: ByteArray): Boolean {
        val c = conn ?: return false
        val e = epOut ?: return false
        val frame = encode(payload)
        synchronized(writeLock) {
            var off = 0
            while (off < frame.size) {
                val n = c.bulkTransfer(e, frame, off, frame.size - off, 1000)
                if (n <= 0) return false
                off += n
            }
        }
        return true
    }

    private fun keepAliveLoop() {
        // Кабель не сообщает рации, слушают ли её: без пустой рамки раз в
        // несколько секунд она через 15 с решит, что хоста нет
        while (running) {
            try { Thread.sleep(KEEPALIVE_MS) } catch (_: InterruptedException) { return }
            if (running) send(ByteArray(0))
        }
    }

    private fun readLoop() {
        val buf = ByteArray(512)
        val frame = ByteArray(1100)
        var len = 0
        var inFrame = false
        var esc = false
        while (running) {
            val c = conn ?: break
            val e = epIn ?: break
            val n = c.bulkTransfer(e, buf, buf.size, 200)
            if (n < 0) {
                if (device != null && manager.deviceList.values.none { it.deviceName == device?.deviceName }) break
                continue
            }
            for (k in 0 until n) {
                var b = buf[k].toInt() and 0xFF
                when {
                    b == FRAME_START -> { inFrame = true; esc = false; len = 0 }
                    !inFrame -> {}
                    b == FRAME_END -> {
                        inFrame = false
                        if (len > 2) {
                            val got = ((frame[len - 2].toInt() and 0xFF) shl 8) or (frame[len - 1].toInt() and 0xFF)
                            if (crc16(frame, len - 2) == got) onData?.invoke(frame.copyOf(len - 2))
                        }
                    }
                    esc -> { if (len < frame.size) frame[len++] = (b xor 0x20).toByte(); esc = false }
                    b == ESC -> esc = true
                    len < frame.size -> frame[len++] = b.toByte()
                    else -> inFrame = false
                }
            }
        }
        if (running) close()
    }

    @Synchronized
    fun close() {
        val c = conn ?: return
        running = false
        conn = null
        try { iface?.let { c.releaseInterface(it) } } catch (_: Exception) {}
        try { c.close() } catch (_: Exception) {}
        epIn = null; epOut = null; iface = null; device = null
        Log.d(TAG, "закрыт")
        onClosed?.invoke()
    }
}
