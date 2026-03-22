package com.meshtrx.app

import android.Manifest
import android.annotation.SuppressLint
import android.content.*
import android.content.pm.PackageManager
import android.os.*
import android.text.InputType
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.fragment.app.Fragment
import com.google.android.material.bottomnavigation.BottomNavigationView
import com.meshtrx.app.model.*
import com.meshtrx.app.ui.*

class MainActivity : AppCompatActivity() {

    override fun attachBaseContext(newBase: Context) {
        super.attachBaseContext(LocaleHelper.applyLocale(newBase))
    }

    var service: MeshTRXService? = null
    private var bound = false

    private val voiceFragment = VoiceFragment()
    private val messagesFragment = MessagesFragment()
    private val filesFragment = FilesFragment()
    private val mapFragment = MapFragment()
    private val settingsFragment = SettingsFragment()
    private var activeFragment: Fragment = voiceFragment
    private var locationHelper: LocationHelper? = null

    private val connection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName, binder: IBinder) {
            service = (binder as MeshTRXService.LocalBinder).getService()
            bound = true
            // Авто-подключение
            service?.autoConnect()
        }
        override fun onServiceDisconnected(name: ComponentName) {
            service = null
            bound = false
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        requestPermissions()

        // Запустить foreground service
        val serviceIntent = Intent(this, MeshTRXService::class.java)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }
        bindService(serviceIntent, connection, Context.BIND_AUTO_CREATE)

        // Fragments
        supportFragmentManager.beginTransaction()
            .add(R.id.fragmentContainer, settingsFragment, "settings").hide(settingsFragment)
            .add(R.id.fragmentContainer, mapFragment, "map").hide(mapFragment)
            .add(R.id.fragmentContainer, filesFragment, "files").hide(filesFragment)
            .add(R.id.fragmentContainer, messagesFragment, "messages").hide(messagesFragment)
            .add(R.id.fragmentContainer, voiceFragment, "voice")
            .commit()

        // GPS — стартует после получения разрешений
        locationHelper = LocationHelper(this)
        tryStartLocation()

        // BottomNav
        findViewById<BottomNavigationView>(R.id.bottomNav).setOnItemSelectedListener { item ->
            val target = when (item.itemId) {
                R.id.nav_voice -> voiceFragment
                R.id.nav_messages -> messagesFragment
                R.id.nav_files -> filesFragment
                R.id.nav_map -> mapFragment
                R.id.nav_settings -> settingsFragment
                else -> voiceFragment
            }
            supportFragmentManager.beginTransaction()
                .hide(activeFragment).show(target).commit()
            activeFragment = target
            true
        }

        // === Хедер observers ===
        val tvCallSign = findViewById<android.widget.TextView>(R.id.tvCallSign)
        val tvDeviceName = findViewById<android.widget.TextView>(R.id.tvDeviceName)
        val tvStatus = findViewById<android.widget.TextView>(R.id.tvStatus)

        ServiceState.callSign.observe(this) { cs ->
            tvCallSign.text = if (cs.isNotEmpty()) cs else "MeshTRX"
        }
        ServiceState.deviceName.observe(this) { tvDeviceName.text = it }
        val tvChannelInfo = findViewById<android.widget.TextView>(R.id.tvChannelInfo)
        ServiceState.currentChannel.observe(this) { ch ->
            tvChannelInfo.text = "CH $ch · ${"%.2f".format(863.15 + ch * 0.3)} MHz"
        }
        ServiceState.connectionState.observe(this) { state ->
            tvStatus.text = when (state) {
                com.meshtrx.app.model.BleState.DISCONNECTED -> getString(R.string.status_disconnected)
                com.meshtrx.app.model.BleState.SCANNING -> getString(R.string.status_scanning)
                com.meshtrx.app.model.BleState.CONNECTING -> getString(R.string.status_connecting)
                com.meshtrx.app.model.BleState.CONNECTED -> getString(R.string.status_connected)
            }
            tvStatus.setTextColor(when (state) {
                com.meshtrx.app.model.BleState.CONNECTED -> com.meshtrx.app.ui.Colors.greenAccent
                com.meshtrx.app.model.BleState.SCANNING,
                com.meshtrx.app.model.BleState.CONNECTING -> com.meshtrx.app.ui.Colors.amberAccent
                else -> com.meshtrx.app.ui.Colors.textDim
            })
        }

        // Observers для диалогов (нужен Activity context)
        ServiceState.showPinDialog.observe(this) { show ->
            if (show) {
                ServiceState.showPinDialog.value = false
                showPinDialog()
            }
        }

        ServiceState.showDevicePicker.observe(this) { show ->
            if (show) {
                ServiceState.showDevicePicker.value = false
                // Подождать 5 сек для сбора всех устройств, потом показать
                Handler(Looper.getMainLooper()).postDelayed({
                    val results = ServiceState.scanResults.value ?: emptyList()
                    service?.bleManager?.stopScan()
                    if (results.isNotEmpty()) {
                        showDevicePicker(results)
                    } else {
                        Toast.makeText(this, "Устройства не найдены", Toast.LENGTH_SHORT).show()
                        service?.disconnect()
                    }
                }, 5000)
            }
        }

        // Автоподключение при обычном скане (не picker)
        ServiceState.scanResults.observe(this) { results ->
            if (results.isNotEmpty() &&
                ServiceState.connectionState.value == BleState.SCANNING &&
                ServiceState.showDevicePicker.value != true) {
                service?.bleManager?.stopScan()
                if (results.size == 1) {
                    service?.connect(results[0].device)
                } else {
                    showDevicePicker(results)
                }
            }
        }
    }

    override fun onResume() {
        super.onResume()
        if (bound && ServiceState.connectionState.value == BleState.DISCONNECTED) {
            service?.autoConnect()
        }
        tryStartLocation()
    }

    private fun tryStartLocation() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.ACCESS_FINE_LOCATION)
            == PackageManager.PERMISSION_GRANTED) {
            locationHelper?.start()
        }
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        tryStartLocation()
    }

    override fun onDestroy() {
        locationHelper?.stop()
        if (bound) {
            unbindService(connection)
            bound = false
        }
        super.onDestroy()
    }

    // === Dialogs ===

    fun showConnectionMenu() {
        AlertDialog.Builder(this)
            .setTitle("Подключение")
            .setItems(arrayOf("Найти устройство", "Новое устройство (забыть текущее)")) { _, which ->
                when (which) {
                    0 -> service?.startScan()
                    1 -> service?.forgetAndScan()
                }
            }
            .setNegativeButton("Отмена", null)
            .show()
    }

    private fun showPinDialog() {
        val input = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_NUMBER
            hint = "Введите PIN с экрана устройства"
            maxLines = 1
        }
        AlertDialog.Builder(this)
            .setTitle("PIN устройства")
            .setMessage("Введите 4-значный код с OLED дисплея")
            .setView(input)
            .setPositiveButton("OK") { _, _ ->
                val pin = input.text.toString().toIntOrNull() ?: 0
                service?.submitPin(pin)
            }
            .setNegativeButton("Отмена") { _, _ -> service?.disconnect() }
            .setCancelable(false)
            .show()
    }

    @SuppressLint("MissingPermission")
    private fun showDevicePicker(results: List<ScanDevice>) {
        val names = results.map { "${it.device.name ?: "?"} (${it.rssi} dBm)" }.toTypedArray()
        AlertDialog.Builder(this)
            .setTitle("Выберите устройство")
            .setItems(names) { _, which -> service?.connect(results[which].device) }
            .setNegativeButton("Отмена") { _, _ -> service?.disconnect() }
            .setCancelable(false)
            .show()
    }

    private fun requestPermissions() {
        val perms = mutableListOf(
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION,
        )
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            perms.add(Manifest.permission.BLUETOOTH_SCAN)
            perms.add(Manifest.permission.BLUETOOTH_CONNECT)
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            perms.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        val needed = perms.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (needed.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, needed.toTypedArray(), 1)
        }
    }
}
