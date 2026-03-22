package com.meshtrx.app.ui

import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import com.google.android.material.switchmaterial.SwitchMaterial
import com.meshtrx.app.*
import com.meshtrx.app.model.*
import com.meshtrx.app.LocaleHelper

class SettingsFragment : Fragment() {

    private val service: MeshTRXService? get() = (activity as? MainActivity)?.service

    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.fragment_settings, c, false)

        val tvConnStatus = v.findViewById<TextView>(R.id.tvConnStatus)
        val btnConnect = v.findViewById<Button>(R.id.btnConnect)
        val btnForgetDevice = v.findViewById<Button>(R.id.btnForgetDevice)
        val etCallSign = v.findViewById<EditText>(R.id.etCallSign)
        val tvTxPower = v.findViewById<TextView>(R.id.tvTxPower)
        val seekTxPower = v.findViewById<SeekBar>(R.id.seekTxPower)
        val switchDutyCycle = v.findViewById<SwitchMaterial>(R.id.switchDutyCycle)
        val spinnerBeacon = v.findViewById<Spinner>(R.id.spinnerBeaconInterval)
        val btnApply = v.findViewById<Button>(R.id.btnApplySettings)
        val tvInfo = v.findViewById<TextView>(R.id.tvDeviceInfo)

        // Канал
        val spinnerChannel = v.findViewById<Spinner>(R.id.spinnerChannel)
        val channels = (0..22).map { "CH $it — %.2f MHz".format(863.150 + it * 0.300) }
        spinnerChannel.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, channels)
        spinnerChannel.setSelection(ServiceState.currentChannel.value ?: 0)
        var channelInitDone = false
        spinnerChannel.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: AdapterView<*>?, v2: View?, pos: Int, id: Long) {
                if (!channelInitDone) { channelInitDone = true; return }
                if (ServiceState.connectionState.value == BleState.CONNECTED) service?.setChannel(pos)
            }
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }
        ServiceState.currentChannel.observe(viewLifecycleOwner) { ch ->
            spinnerChannel.setSelection(ch)
        }

        // Подключение
        btnConnect.setOnClickListener {
            when (ServiceState.connectionState.value) {
                BleState.DISCONNECTED -> service?.startScan()
                BleState.CONNECTED -> service?.disconnect()
                BleState.SCANNING, BleState.CONNECTING -> {
                    service?.bleManager?.stopScan()
                    service?.disconnect()
                }
                else -> {}
            }
        }

        btnForgetDevice.setOnClickListener {
            (activity as? MainActivity)?.showConnectionMenu()
        }

        // Peer timeout
        val spinnerPeerTimeout = v.findViewById<Spinner>(R.id.spinnerPeerTimeout)
        val timeoutOptions = listOf(getString(R.string.timeout_15), getString(R.string.timeout_30), getString(R.string.timeout_60), getString(R.string.timeout_2h), getString(R.string.timeout_6h), getString(R.string.timeout_24h))
        val timeoutValues = listOf(15, 30, 60, 120, 360, 1440)
        spinnerPeerTimeout.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, timeoutOptions)
        spinnerPeerTimeout.setSelection(2) // 60 мин
        spinnerPeerTimeout.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: AdapterView<*>?, v2: View?, pos: Int, id: Long) {
                ServiceState.peerTimeoutMin.value = timeoutValues[pos]
            }
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }

        // Громкость приёма
        val tvRxVolume = v.findViewById<TextView>(R.id.tvRxVolume)
        val seekRxVolume = v.findViewById<SeekBar>(R.id.seekRxVolume)
        seekRxVolume.progress = ServiceState.rxVolume.value ?: 200
        tvRxVolume.text = getString(R.string.rx_volume, seekRxVolume.progress)
        seekRxVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) {
                    val vol = value.coerceAtLeast(50) // минимум 50%
                    ServiceState.rxVolume.value = vol
                    service?.audioEngine?.volumeBoost = vol / 100f
                    tvRxVolume.text = getString(R.string.rx_volume, vol)
                }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // PTT RMS (шумоподавление)
        val tvPttRms = v.findViewById<TextView>(R.id.tvPttRms)
        val seekPttRms = v.findViewById<SeekBar>(R.id.seekPttRms)
        val currentRms = service?.audioEngine?.squelchThreshold ?: 0
        seekPttRms.progress = currentRms
        tvPttRms.text = getString(R.string.ptt_rms, currentRms)
        seekPttRms.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) {
                    service?.audioEngine?.squelchThreshold = value
                    tvPttRms.text = getString(R.string.ptt_rms, value)
                }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // VOX
        val tvVoxThreshold = v.findViewById<TextView>(R.id.tvVoxThreshold)
        val seekVoxThreshold = v.findViewById<SeekBar>(R.id.seekVoxThreshold)
        val tvVoxHangtime = v.findViewById<TextView>(R.id.tvVoxHangtime)
        val seekVoxHangtime = v.findViewById<SeekBar>(R.id.seekVoxHangtime)

        seekVoxThreshold.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) { service?.setVoxThreshold(value); tvVoxThreshold.text = getString(R.string.vox_threshold, value) }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })
        seekVoxHangtime.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) { val ms = value.coerceAtLeast(200).toLong(); service?.setVoxHangtime(ms); tvVoxHangtime.text = getString(R.string.vox_hangtime, ms.toInt()) }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // Beacon intervals
        val beaconOptions = listOf(getString(R.string.beacon_never), getString(R.string.beacon_1min), getString(R.string.beacon_3min), getString(R.string.beacon_5min), getString(R.string.beacon_15min), getString(R.string.beacon_30min), getString(R.string.beacon_1hour))
        spinnerBeacon.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, beaconOptions)
        spinnerBeacon.setSelection(3)

        seekTxPower.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                tvTxPower.text = getString(R.string.tx_power, value)
                if (value > 14) tvTxPower.append(" ⚠️EU")
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // File history
        val spinnerFileHistory = v.findViewById<Spinner>(R.id.spinnerFileHistory)
        val historyOptions = listOf(getString(R.string.history_7d), getString(R.string.history_14d), getString(R.string.history_30d), getString(R.string.history_90d), getString(R.string.history_unlimited))
        val historyValues = listOf(7, 14, 30, 90, 3650)
        spinnerFileHistory.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, historyOptions)
        spinnerFileHistory.setSelection(2) // 30 дней
        spinnerFileHistory.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: AdapterView<*>?, v2: View?, pos: Int, id: Long) {
                ServiceState.fileHistoryDays.value = historyValues[pos]
            }
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }

        // Язык
        val spinnerLanguage = v.findViewById<Spinner>(R.id.spinnerLanguage)
        val langNames = listOf("Русский", "English")
        val langCodes = listOf("ru", "en")
        spinnerLanguage.adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_dropdown_item, langNames)
        val currentLang = LocaleHelper.getSavedLanguage(requireContext())
        spinnerLanguage.setSelection(langCodes.indexOf(currentLang).coerceAtLeast(0))
        spinnerLanguage.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            private var init = true
            override fun onItemSelected(p: AdapterView<*>?, v2: View?, pos: Int, id: Long) {
                if (init) { init = false; return }
                val newLang = langCodes[pos]
                if (newLang != LocaleHelper.getSavedLanguage(requireContext())) {
                    LocaleHelper.setLanguage(requireContext(), newLang)
                    activity?.recreate()
                }
            }
            override fun onNothingSelected(p: AdapterView<*>?) {}
        }

        btnApply.setOnClickListener {
            val power = seekTxPower.progress
            val dc = switchDutyCycle.isChecked
            val beaconIdx = spinnerBeacon.selectedItemPosition
            val beaconSec = listOf(0, 60, 180, 300, 900, 1800, 3600)[beaconIdx]
            val callSign = etCallSign.text.toString().trim()

            // Сохранить позывной локально и на девайс
            if (callSign.isNotEmpty()) {
                service?.setCallSign(callSign)
            }

            val json = buildString {
                append("{\"tx_power\":$power,\"duty_cycle\":$dc")
                append(",\"beacon_interval\":$beaconSec")
                if (callSign.isNotEmpty()) append(",\"callsign\":\"$callSign\"")
                append("}")
            }
            service?.bleManager?.sendSettings(json)
            Toast.makeText(requireContext(), getString(R.string.settings_applied), Toast.LENGTH_SHORT).show()
        }

        // Ретранслятор
        val etRepeaterSsid = v.findViewById<EditText>(R.id.etRepeaterSsid)
        val etRepeaterPass = v.findViewById<EditText>(R.id.etRepeaterPass)
        val etRepeaterIp = v.findViewById<EditText>(R.id.etRepeaterIp)
        val btnRepeaterOn = v.findViewById<Button>(R.id.btnRepeaterOn)
        val btnRepeaterOff = v.findViewById<Button>(R.id.btnRepeaterOff)

        btnRepeaterOn.setOnClickListener {
            androidx.appcompat.app.AlertDialog.Builder(requireContext())
                .setTitle(getString(R.string.repeater_title))
                .setMessage(getString(R.string.repeater_warning))
                .setPositiveButton(getString(R.string.activate_repeater)) { _, _ ->
                    val ssid = etRepeaterSsid.text.toString().trim()
                    val pass = etRepeaterPass.text.toString().trim()
                    val ip = etRepeaterIp.text.toString().trim()
                    service?.bleManager?.sendRepeaterConfig(true, ssid, pass, ip)
                }
                .setNegativeButton(getString(R.string.cancel), null)
                .show()
        }
        btnRepeaterOff.setOnClickListener {
            service?.bleManager?.sendRepeaterConfig(false)
        }

        // Загрузить сохранённый позывной в поле
        ServiceState.callSign.observe(viewLifecycleOwner) { cs ->
            if (etCallSign.text.isEmpty() && cs.isNotEmpty()) {
                etCallSign.setText(cs)
            }
        }

        // Observers
        ServiceState.connectionState.observe(viewLifecycleOwner) { state ->
            when (state) {
                BleState.DISCONNECTED -> {
                    tvConnStatus.text = getString(R.string.disconnected)
                    btnConnect.text = getString(R.string.connect)
                }
                BleState.SCANNING -> {
                    tvConnStatus.text = getString(R.string.scanning)
                    btnConnect.text = getString(R.string.cancel)
                }
                BleState.CONNECTING -> {
                    tvConnStatus.text = getString(R.string.connecting)
                    btnConnect.text = getString(R.string.cancel)
                }
                BleState.CONNECTED -> {
                    tvConnStatus.text = getString(R.string.connected)
                    btnConnect.text = getString(R.string.disconnect)
                }
            }
            val connected = state == BleState.CONNECTED
            btnApply.isEnabled = connected
            btnRepeaterOn.isEnabled = connected
            btnRepeaterOff.isEnabled = connected
        }

        ServiceState.deviceName.observe(viewLifecycleOwner) { name ->
            tvInfo.text = getString(R.string.device_label, name)
        }

        return v
    }
}
