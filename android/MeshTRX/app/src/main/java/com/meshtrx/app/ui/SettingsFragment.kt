package com.meshtrx.app.ui

import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.launch
import com.meshtrx.app.LatestVersions
import com.meshtrx.app.UpdateChecker
import com.meshtrx.app.BuildConfig
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

        // Поиск свободного канала
        val btnScan = v.findViewById<Button>(R.id.btnScanChannels)
        val tvScan = v.findViewById<TextView>(R.id.tvScanResult)
        btnScan.setOnClickListener {
            if (ServiceState.connectionState.value != BleState.CONNECTED) {
                Toast.makeText(requireContext(), getString(R.string.disconnected),
                    Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            tvScan.visibility = View.VISIBLE
            tvScan.text = "Слушаю эфир…"
            service?.bleManager?.scanChannels()
        }
        // Перевести всю группу на выбранный канал
        v.findViewById<Button>(R.id.btnChannelAll).setOnClickListener {
            if (ServiceState.connectionState.value != BleState.CONNECTED) {
                Toast.makeText(requireContext(), getString(R.string.disconnected),
                    Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val ch = spinnerChannel.selectedItemPosition
            androidx.appcompat.app.AlertDialog.Builder(requireContext())
                .setMessage(getString(R.string.channel_all_confirm, ch))
                .setPositiveButton("OK") { _, _ ->
                    service?.bleManager?.setChannelForAll(ch)
                    Toast.makeText(requireContext(), getString(R.string.channel_all_sent),
                        Toast.LENGTH_LONG).show()
                }
                .setNegativeButton(getString(R.string.cancel), null)
                .show()
        }

        ServiceState.channelNoise.observe(viewLifecycleOwner) { levels ->
            if (levels.isEmpty()) return@observe
            val best = ServiceState.channelBest.value ?: -1
            // Показываем три самых тихих канала и сам уровень: человеку важно
            // видеть, есть ли вообще разница, а не только «вот этот лучше».
            val quiet = levels.withIndex().sortedBy { it.value }.take(3)
                .joinToString(", ") { "CH ${it.index} (${it.value} dBm)" }
            val noisiest = levels.withIndex().maxByOrNull { it.value }
            tvScan.visibility = View.VISIBLE
            tvScan.text = buildString {
                append("Тише всего: $quiet")
                if (noisiest != null) {
                    append("\nШумнее всего: CH ${noisiest.index} (${noisiest.value} dBm)")
                }
                append("\nЧтобы перевести всю группу разом — «Сменить канал у всех»")
            }
            if (best in levels.indices) spinnerChannel.setSelection(best)
        }

        // === Шифрование эфира ===
        val tvKeyState = v.findViewById<TextView>(R.id.tvKeyState)
        val etPassphrase = v.findViewById<EditText>(R.id.etPassphrase)

        fun showKeyState() {
            val fp = ServiceState.keyFingerprint.value.orEmpty()
            val alien = ServiceState.alienPackets.value ?: 0
            val base = if (fp.isEmpty()) getString(R.string.key_none)
                       else getString(R.string.key_set, fp)
            // Отдельная строка про чужой ключ: тишина в эфире и «говорят, но не
            // нашим ключом» выглядят одинаково, а причина у них разная.
            tvKeyState.text = if (fp.isNotEmpty() && alien > 0)
                base + "\n" + getString(R.string.key_alien, alien) else base
            tvKeyState.setTextColor(when {
                fp.isEmpty() -> 0xFFff9d5c.toInt()
                alien > 0 -> 0xFFff9d5c.toInt()
                else -> 0xFF4ade80.toInt()
            })
        }
        showKeyState()
        // Спрашиваем сами: состояние ключа могло не дойти, если рация сказала
        // о нём раньше, чем телефон успел подписаться на уведомления.
        if (ServiceState.connectionState.value == BleState.CONNECTED) {
            service?.bleManager?.requestKeyState()
            service?.bleManager?.requestFirmwareVersion()
        }
        ServiceState.keyFingerprint.observe(viewLifecycleOwner) { showKeyState() }
        ServiceState.alienPackets.observe(viewLifecycleOwner) { showKeyState() }

        // Выбор человека, а не наше решение за него: кому-то нужен и общий
        // открытый чат, и свои под ключом. Но открытые сообщения помечаются,
        // чтобы защищённое и незащищённое не выглядели одинаково.
        val switchHearPlain = v.findViewById<SwitchMaterial>(R.id.switchHearPlain)
        var hearPlainInit = false
        ServiceState.hearPlaintext.observe(viewLifecycleOwner) { on ->
            hearPlainInit = true
            if (switchHearPlain.isChecked != on) switchHearPlain.isChecked = on
        }
        switchHearPlain.setOnCheckedChangeListener { _, on ->
            if (!hearPlainInit) return@setOnCheckedChangeListener
            if (ServiceState.connectionState.value != BleState.CONNECTED) {
                Toast.makeText(requireContext(), getString(R.string.disconnected),
                    Toast.LENGTH_SHORT).show()
                return@setOnCheckedChangeListener
            }
            service?.bleManager?.setHearPlaintext(on)
            Toast.makeText(requireContext(),
                getString(if (on) R.string.hear_plain_on else R.string.hear_plain_off),
                Toast.LENGTH_LONG).show()
        }

        v.findViewById<Button>(R.id.btnKeyApply).setOnClickListener {
            if (ServiceState.connectionState.value != BleState.CONNECTED) {
                Toast.makeText(requireContext(), getString(R.string.disconnected),
                    Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            val phrase = etPassphrase.text.toString().trim()
            // Считаем символы, а не байты: в кириллице длина в байтах вдвое
            // больше, и короткое слово прошло бы проверку.
            if (phrase.codePointCount(0, phrase.length) < 8) {
                Toast.makeText(requireContext(), getString(R.string.key_short),
                    Toast.LENGTH_LONG).show()
                return@setOnClickListener
            }
            service?.bleManager?.setPassphrase(phrase)
            etPassphrase.setText("")
            Toast.makeText(requireContext(), getString(R.string.key_applying),
                Toast.LENGTH_LONG).show()
        }

        v.findViewById<Button>(R.id.btnKeyClear).setOnClickListener {
            if (ServiceState.connectionState.value != BleState.CONNECTED) {
                Toast.makeText(requireContext(), getString(R.string.disconnected),
                    Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }
            androidx.appcompat.app.AlertDialog.Builder(requireContext())
                .setMessage(getString(R.string.key_none))
                .setPositiveButton(getString(R.string.key_clear)) { _, _ ->
                    service?.bleManager?.clearKey()
                }
                .setNegativeButton(getString(R.string.cancel), null)
                .show()
        }

        // Очистка списка абонентов
        v.findViewById<Button>(R.id.btnClearPeers).setOnClickListener {
            androidx.appcompat.app.AlertDialog.Builder(requireContext())
                .setMessage(getString(R.string.clear_peers_confirm))
                .setPositiveButton("OK") { _, _ ->
                    ServiceState.peers.value = emptyList()
                    ServiceState.recentCalls.value = emptyList()
                    service?.savePeers(emptyList())
                    Toast.makeText(requireContext(), "OK", Toast.LENGTH_SHORT).show()
                }
                .setNegativeButton(getString(R.string.cancel), null)
                .show()
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
        seekRxVolume.progress = service?.rxVolume() ?: (ServiceState.rxVolume.value ?: 200)
        tvRxVolume.text = getString(R.string.rx_volume, seekRxVolume.progress)
        seekRxVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) {
                    val vol = value.coerceAtLeast(50) // минимум 50%
                    service?.setRxVolume(vol)
                    tvRxVolume.text = getString(R.string.rx_volume, vol)
                }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // PTT RMS (шумоподавление)
        val tvPttRms = v.findViewById<TextView>(R.id.tvPttRms)
        val seekPttRms = v.findViewById<SeekBar>(R.id.seekPttRms)
        val currentRms = service?.pttRms() ?: 0
        seekPttRms.progress = currentRms
        tvPttRms.text = getString(R.string.ptt_rms, currentRms)
        seekPttRms.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar?, value: Int, fromUser: Boolean) {
                if (fromUser) {
                    service?.setPttRms(value)
                    tvPttRms.text = getString(R.string.ptt_rms, value)
                }
            }
            override fun onStartTrackingTouch(sb: SeekBar?) {}
            override fun onStopTrackingTouch(sb: SeekBar?) {}
        })

        // Звук окончания передачи
        val switchRogerBeep = v.findViewById<SwitchMaterial>(R.id.switchRogerBeep)
        switchRogerBeep.isChecked = service?.rogerBeepEnabled() ?: true
        switchRogerBeep.setOnCheckedChangeListener { _, checked ->
            service?.setRogerBeep(checked)
        }

        // VOX
        val tvVoxThreshold = v.findViewById<TextView>(R.id.tvVoxThreshold)
        val seekVoxThreshold = v.findViewById<SeekBar>(R.id.seekVoxThreshold)
        val tvVoxHangtime = v.findViewById<TextView>(R.id.tvVoxHangtime)
        val seekVoxHangtime = v.findViewById<SeekBar>(R.id.seekVoxHangtime)

        // Ползунки VOX раньше не заполнялись вовсе и всегда показывали значения
        // из разметки: подобранный порог оставался в силе, а на экране стояло
        // «800», и человек правил настройку вслепую.
        val currentVoxThreshold = service?.voxThreshold() ?: VoxEngine.DEFAULT_THRESHOLD
        val currentVoxHangtime = (service?.voxHangtime() ?: VoxEngine.DEFAULT_HANGTIME_MS).toInt()
        seekVoxThreshold.progress = currentVoxThreshold
        tvVoxThreshold.text = getString(R.string.vox_threshold, currentVoxThreshold)
        seekVoxHangtime.progress = currentVoxHangtime
        tvVoxHangtime.text = getString(R.string.vox_hangtime, currentVoxHangtime)

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

            // Сохранить позывной локально (на девайс отправится в общем JSON ниже)
            if (callSign.isNotEmpty()) {
                service?.saveCallSignLocal(callSign)
            }

            // Отправить все настройки одним пакетом (включая callsign)
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

        // === Обновления ===
        val tvUpdate = v.findViewById<TextView>(R.id.tvUpdate)
        val tvUpdateApp = v.findViewById<TextView>(R.id.tvUpdateApp)
        val tvUpdateFw = v.findViewById<TextView>(R.id.tvUpdateFw)
        val btnUpdateGet = v.findViewById<Button>(R.id.btnUpdateGet)
        var latestUrl: String? = null
        var lastLatest: LatestVersions? = null
        val colorOk = 0xFF888888.toInt()
        val colorNew = 0xFF4ade80.toInt()

        // Две отдельные строки: приложение и прошивка обновляются по-разному —
        // приложение кнопкой, прошивка кабелем, — и человеку нужно видеть
        // состояние каждой, а не одну строку про «версию».
        fun showVersions(latest: LatestVersions?) {
            val myFw = ServiceState.firmwareVersion.value
            if (latest == null) {
                tvUpdateApp.text = getString(R.string.ver_app_ok, BuildConfig.VERSION_NAME)
                tvUpdateApp.setTextColor(colorOk)
                tvUpdateFw.text = when {
                    !myFw.isNullOrBlank() -> getString(R.string.ver_fw_ok, myFw)
                    ServiceState.connectionState.value == BleState.CONNECTED ->
                        getString(R.string.ver_fw_unknown)
                    else -> getString(R.string.ver_fw_offline)
                }
                tvUpdateFw.setTextColor(colorOk)
                return
            }

            val appOld = latest.appCode > BuildConfig.VERSION_CODE
            tvUpdateApp.text = if (appOld)
                getString(R.string.ver_app_old, BuildConfig.VERSION_NAME, latest.appVersion)
            else getString(R.string.ver_app_ok, BuildConfig.VERSION_NAME)
            tvUpdateApp.setTextColor(if (appOld) colorNew else colorOk)

            val fwOld = UpdateChecker.firmwareOlder(myFw, latest.firmwareVersion)
            tvUpdateFw.text = when {
                myFw.isNullOrBlank() && ServiceState.connectionState.value != BleState.CONNECTED ->
                    getString(R.string.ver_fw_offline)
                myFw.isNullOrBlank() -> getString(R.string.ver_fw_unknown)
                fwOld -> getString(R.string.ver_fw_old, myFw, latest.firmwareVersion)
                else -> getString(R.string.ver_fw_ok, myFw)
            }
            tvUpdateFw.setTextColor(if (fwOld) colorNew else colorOk)

            latestUrl = if (appOld) latest.appUrl else null
            btnUpdateGet.visibility = if (appOld) View.VISIBLE else View.GONE
        }

        fun checkUpdates(byHand: Boolean) {
            if (byHand) tvUpdateApp.text = getString(R.string.update_checking)
            viewLifecycleOwner.lifecycleScope.launch {
                val latest = UpdateChecker.fetch()
                if (!isAdded) return@launch
                if (latest == null) {
                    // Молчим при автопроверке: рация нужна там, где сети нет, и
                    // ругаться на её отсутствие в каждом запуске незачем.
                    showVersions(null)
                    if (byHand) tvUpdate.apply {
                        visibility = View.VISIBLE
                        text = getString(R.string.update_offline)
                    }
                    return@launch
                }
                showVersions(latest)
                lastLatest = latest
                requireContext()
                    .getSharedPreferences("updates", android.content.Context.MODE_PRIVATE)
                    .edit().putLong("lastCheck", System.currentTimeMillis()).apply()
            }
        }

        showVersions(null)
        ServiceState.firmwareVersion.observe(viewLifecycleOwner) { showVersions(lastLatest) }
        ServiceState.connectionState.observe(viewLifecycleOwner) { showVersions(lastLatest) }

        v.findViewById<Button>(R.id.btnUpdateCheck).setOnClickListener { checkUpdates(true) }
        btnUpdateGet.setOnClickListener {
            latestUrl?.let {
                startActivity(android.content.Intent(android.content.Intent.ACTION_VIEW,
                    android.net.Uri.parse(it)))
            }
        }

        // Сама, но не чаще раза в сутки: выпуски выходят несколько раз в неделю,
        // и человек узнавал о них, только если заходил на сайт.
        val prefs = requireContext()
            .getSharedPreferences("updates", android.content.Context.MODE_PRIVATE)
        val since = System.currentTimeMillis() - prefs.getLong("lastCheck", 0)
        if (since > UpdateChecker.CHECK_INTERVAL_MS) checkUpdates(false)

        fun showDeviceLine() {
            // Версию приложения берём из сборки, а не из строки ресурсов:
            // зашитый номер отстал от релизов, и люди по нему решали, что
            // обновление не встало. Версию прошивки называет сама рация; до
            // 4.4.22 она этого не умела, и тогда строки про неё просто нет.
            val base = getString(R.string.device_label,
                ServiceState.deviceName.value.orEmpty(), BuildConfig.VERSION_NAME)
            val fw = ServiceState.firmwareVersion.value
            tvInfo.text = if (fw.isNullOrBlank()) base
                          else base + getString(R.string.device_firmware, fw)
        }
        ServiceState.deviceName.observe(viewLifecycleOwner) { showDeviceLine() }
        ServiceState.firmwareVersion.observe(viewLifecycleOwner) { showDeviceLine() }

        return v
    }
}
