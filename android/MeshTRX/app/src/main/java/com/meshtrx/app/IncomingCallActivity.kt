package com.meshtrx.app

import android.content.*
import android.os.*
import android.view.View
import android.view.WindowManager
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import com.meshtrx.app.model.CallType
import com.meshtrx.app.model.IncomingCall
import com.meshtrx.app.ui.Colors

class IncomingCallActivity : AppCompatActivity() {

    companion object {
        const val EXTRA_CALL_TYPE = "call_type"
        const val EXTRA_SENDER_ID = "sender_id"
        const val EXTRA_CALL_SIGN = "call_sign"
        const val EXTRA_CALL_SEQ = "call_seq"
        const val EXTRA_RSSI = "rssi"
        const val EXTRA_LAT = "lat"
        const val EXTRA_LON = "lon"
        private const val TIMEOUT_SEC = 10
    }

    private var service: MeshTRXService? = null
    private var bound = false
    private lateinit var ringer: CallRinger
    private val handler = Handler(Looper.getMainLooper())
    private var timeoutSec = TIMEOUT_SEC

    private lateinit var callType: CallType
    private lateinit var senderId: String
    private lateinit var callSign: String
    private var callSeq = 0
    private var rssi = 0

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            service = (binder as MeshTRXService.LocalBinder).getService()
            bound = true
        }
        override fun onServiceDisconnected(name: ComponentName) {
            service = null; bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Показать поверх экрана блокировки
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
            setShowWhenLocked(true)
            setTurnScreenOn(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
            )
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        setContentView(R.layout.activity_incoming_call)
        bindService(Intent(this, MeshTRXService::class.java), connection, Context.BIND_AUTO_CREATE)

        // Парсинг intent
        callType = CallType.fromCode(intent.getIntExtra(EXTRA_CALL_TYPE, 0))
        senderId = intent.getStringExtra(EXTRA_SENDER_ID) ?: "????"
        callSign = intent.getStringExtra(EXTRA_CALL_SIGN) ?: "???"
        callSeq = intent.getIntExtra(EXTRA_CALL_SEQ, 0)
        rssi = intent.getIntExtra(EXTRA_RSSI, 0)
        val lat = intent.getDoubleExtra(EXTRA_LAT, 0.0)
        val lon = intent.getDoubleExtra(EXTRA_LON, 0.0)

        val rootLayout = findViewById<View>(R.id.rootLayout)
        val tvCallType = findViewById<TextView>(R.id.tvCallType)
        val tvCallerName = findViewById<TextView>(R.id.tvCallerName)
        val tvCallerId = findViewById<TextView>(R.id.tvCallerId)
        val tvCallRssi = findViewById<TextView>(R.id.tvCallRssi)
        val tvCallCoords = findViewById<TextView>(R.id.tvCallCoords)
        val tvCallTimer = findViewById<TextView>(R.id.tvCallTimer)
        val btnOk = findViewById<Button>(R.id.btnClose)
        // Скрыть Accept/Reject, показать только OK
        findViewById<Button>(R.id.btnAccept).visibility = View.GONE
        findViewById<Button>(R.id.btnReject).visibility = View.GONE
        btnOk.visibility = View.VISIBLE
        btnOk.text = "OK"

        // UI по типу вызова
        tvCallerName.text = callSign
        tvCallerId.text = "ID: $senderId"
        tvCallRssi.text = "${rssi} dBm"
        tvCallRssi.setTextColor(Colors.rssiColor(rssi))

        when (callType) {
            CallType.ALL -> {
                tvCallType.text = "ОБЩИЙ ВЫЗОВ"
                tvCallType.setTextColor(Colors.blueAccent)
            }
            CallType.PRIVATE -> {
                tvCallType.text = "ЛИЧНЫЙ ВЫЗОВ"
                tvCallType.setTextColor(Colors.greenAccent)
            }
            CallType.GROUP -> {
                tvCallType.text = "ГРУППОВОЙ ВЫЗОВ"
                tvCallType.setTextColor(Colors.amberAccent)
            }
            CallType.EMERGENCY -> {
                tvCallType.text = "!!! SOS !!!"
                tvCallType.setTextColor(Colors.redAccent)
                tvCallType.textSize = 20f
                rootLayout.setBackgroundColor(0xFF1a0000.toInt())
                if (lat != 0.0 || lon != 0.0) {
                    tvCallCoords.text = "%.4f°N  %.4f°E".format(lat, lon)
                    tvCallCoords.visibility = View.VISIBLE
                }
            }
        }

        // Кнопка OK — закрыть экран, остановить зумер
        btnOk.setOnClickListener {
            dismissCall()
        }

        // Зумер/вибрация
        ringer = CallRinger(this)
        ringer.start(callType)

        // Таймер 30 сек
        handler.post(object : Runnable {
            override fun run() {
                timeoutSec--
                tvCallTimer.text = "${timeoutSec}с"
                if (timeoutSec <= 0) {
                    dismissCall()
                } else {
                    handler.postDelayed(this, 1000)
                }
            }
        })

        // Наблюдать отмену вызова
        ServiceState.incomingCall.observe(this) { call ->
            if (call == null) {
                ringer.stop()
                finish()
            }
        }
    }

    private fun dismissCall() {
        ringer.stop()
        ServiceState.incomingCall.postValue(null)
        finish()
    }

    override fun onDestroy() {
        ringer.stop()
        handler.removeCallbacksAndMessages(null)
        if (bound) { unbindService(connection); bound = false }
        super.onDestroy()
    }

    @Deprecated("Deprecated in Java")
    override fun onBackPressed() {
        // Запретить закрытие кнопкой назад для EMERGENCY
        if (callType != CallType.EMERGENCY) {
            dismissCall()
        }
    }
}
