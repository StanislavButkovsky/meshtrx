package com.meshtrx.app.ui

import android.annotation.SuppressLint
import android.content.Context
import android.media.AudioManager
import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.switchmaterial.SwitchMaterial
import com.meshtrx.app.*
import com.meshtrx.app.model.*

class VoiceFragment : Fragment() {

    private val service: MeshTRXService? get() = (activity as? MainActivity)?.service
    private var targetId: String? = null   // null = broadcast
    private var targetName: String? = null
    private val voiceRecorder = VoiceRecorder(maxDurationSec = 10)

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.fragment_voice, c, false)

        val switchVox = v.findViewById<SwitchMaterial>(R.id.switchVox)
        val tvVoxState = v.findViewById<TextView>(R.id.tvVoxState)
        val tvStatusLine = v.findViewById<TextView>(R.id.tvStatusLine)
        val pttButton = v.findViewById<PttButtonView>(R.id.pttButton)
        val btnListenAll = v.findViewById<Button>(R.id.btnListenAll)
        val btnListenMy = v.findViewById<Button>(R.id.btnListenMy)
        val btnSelectTarget = v.findViewById<ImageButton>(R.id.btnSelectTarget)
        val tvTargetName = v.findViewById<TextView>(R.id.tvTargetName)
        val tvTargetSignal = v.findViewById<TextView>(R.id.tvTargetSignal)
        val tvDeliveryStatus = v.findViewById<TextView>(R.id.tvDeliveryStatus)
        val btnCall = v.findViewById<ImageButton>(R.id.btnCall)
        val rvRecent = v.findViewById<androidx.recyclerview.widget.RecyclerView>(R.id.rvRecentCalls)
        rvRecent.layoutManager = LinearLayoutManager(requireContext())

        // Громкая связь — по умолчанию включена
        val btnSpeaker = v.findViewById<ImageButton>(R.id.btnSpeaker)
        val audioManager = requireContext().getSystemService(Context.AUDIO_SERVICE) as AudioManager
        var speakerOn = true
        audioManager.isSpeakerphoneOn = true
        fun updateSpeakerIcon() {
            btnSpeaker.setImageResource(if (speakerOn) R.drawable.ic_speaker else R.drawable.ic_speaker_off)
            btnSpeaker.setBackgroundResource(if (speakerOn) R.drawable.btn_circle_green else R.drawable.btn_circle_gray)
        }
        updateSpeakerIcon()
        btnSpeaker.setOnClickListener {
            speakerOn = !speakerOn
            audioManager.isSpeakerphoneOn = speakerOn
            updateSpeakerIcon()
        }

        // Listen mode toggle
        fun updateListenButtons(mode: ListenMode) {
            val isAll = mode == ListenMode.ALL
            btnListenAll.setTextColor(if (isAll) 0xFF4ade80.toInt() else 0xFF888888.toInt())
            btnListenAll.backgroundTintList = android.content.res.ColorStateList.valueOf(
                if (isAll) 0xFF1e3a1e.toInt() else 0xFF222222.toInt())
            btnListenMy.setTextColor(if (!isAll) 0xFF4ade80.toInt() else 0xFF888888.toInt())
            btnListenMy.backgroundTintList = android.content.res.ColorStateList.valueOf(
                if (!isAll) 0xFF1e3a1e.toInt() else 0xFF222222.toInt())
        }
        btnListenAll.setOnClickListener {
            service?.setListenMode(ListenMode.ALL)
            updateListenButtons(ListenMode.ALL)
        }
        btnListenMy.setOnClickListener {
            service?.setListenMode(ListenMode.PRIVATE_ONLY)
            updateListenButtons(ListenMode.PRIVATE_ONLY)
        }
        updateListenButtons(ServiceState.listenMode.value ?: ListenMode.ALL)

        // === Выбор адресата ===
        fun updateTargetUI() {
            // VOX только для broadcast — при адресате деактивировать
            if (targetId != null) {
                if (ServiceState.txMode.value == TxMode.VOX) {
                    service?.setTxMode(TxMode.PTT)
                    switchVox.isChecked = false
                }
                switchVox.isEnabled = false
            } else {
                val connected = ServiceState.connectionState.value == BleState.CONNECTED
                switchVox.isEnabled = connected
            }

            if (targetId == null) {
                tvTargetName.text = getString(R.string.general_channel)
                tvTargetName.setTextColor(0xFF5ba3e8.toInt())
                tvTargetSignal.visibility = View.GONE
            } else {
                tvTargetName.text = targetName ?: "TX-$targetId"
                tvTargetName.setTextColor(Colors.greenAccent)
                // Найти peer и показать сигнал
                val peer = ServiceState.peers.value?.find { it.deviceId.endsWith(targetId!!) }
                if (peer != null) {
                    val ago = (System.currentTimeMillis() - peer.lastSeenMs) / 1000
                    val agoStr = when {
                        ago < 60 -> "${ago}s"
                        ago < 3600 -> "${ago / 60}m"
                        else -> "${ago / 3600}h"
                    }
                    tvTargetSignal.text = "${peer.rssi}dBm / ${peer.snr}dB · $agoStr"
                    tvTargetSignal.setTextColor(Colors.rssiColor(peer.rssi))
                    tvTargetSignal.visibility = View.VISIBLE
                } else {
                    tvTargetSignal.visibility = View.GONE
                }
            }
        }

        btnSelectTarget.setOnClickListener {
            val sheet = FileDestPickerSheet()
            sheet.customTitle = getString(R.string.select_subscriber)
            sheet.onSelected = { mac, name ->
                if (mac == null) {
                    targetId = null
                    targetName = null
                } else {
                    targetId = String.format("%02X%02X", mac[0].toInt() and 0xFF, mac[1].toInt() and 0xFF)
                    targetName = name
                }
                updateTargetUI()
            }
            sheet.show(parentFragmentManager, "target_picker")
        }

        // Вызов — адресат уже выбран
        btnCall.setOnClickListener {
            if (targetId == null) {
                // Общий вызов
                service?.bleManager?.send(byteArrayOf(BleManager.CMD_CALL_ALL.toByte()))
                service?.addRecentCall(RecentCall("BROADCAST", getString(R.string.general_channel), true, "ALL"))
                Toast.makeText(requireContext(), getString(R.string.call_general), Toast.LENGTH_SHORT).show()
            } else {
                // Приватный вызов выбранному адресату
                val idBytes = targetId!!.padStart(8, '0').chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                val pkt = ByteArray(1 + 4)
                pkt[0] = BleManager.CMD_CALL_PRIVATE.toByte()
                System.arraycopy(idBytes, 0, pkt, 1, 4.coerceAtMost(idBytes.size))
                service?.bleManager?.send(pkt)
                service?.addRecentCall(RecentCall(targetId!!, targetName ?: "TX-$targetId", true, "PRIVATE",
                    rssi = ServiceState.rssi.value))
                Toast.makeText(requireContext(), "Call → ${targetName}", Toast.LENGTH_SHORT).show()
            }
        }

        // Авто-отправка при достижении лимита записи
        voiceRecorder.onMaxReached = {
            android.os.Handler(android.os.Looper.getMainLooper()).post {
                sendBufferedVoice(tvStatusLine, tvDeliveryStatus, pttButton)
            }
        }

        // PTT — touch: broadcast = realtime, addressed = buffered
        pttButton.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    if (targetId == null) {
                        // Broadcast: обычный realtime PTT
                        service?.pttDown()
                    } else {
                        // Addressed: буферизованная запись
                        voiceRecorder.startRecording()
                        pttButton.state = PttButtonView.State.TX
                        tvStatusLine.text = "● запись..."
                        tvStatusLine.setTextColor(Colors.redTx)
                        tvDeliveryStatus.visibility = View.GONE
                    }
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    if (targetId == null) {
                        service?.pttUp()
                    } else {
                        // Остановить запись, отправить
                        sendBufferedVoice(tvStatusLine, tvDeliveryStatus, pttButton)
                    }
                    true
                }
                else -> true
            }
        }

        // VOX switch
        switchVox.setOnCheckedChangeListener { _, isVox ->
            service?.setTxMode(if (isVox) TxMode.VOX else TxMode.PTT)
        }

        // === Observers ===
        ServiceState.connectionState.observe(viewLifecycleOwner) { state ->
            val connected = state == BleState.CONNECTED
            val playing = ServiceState.isPlayingVoice.value == true
            pttButton.isEnabled = connected && !playing
            switchVox.isEnabled = connected && targetId == null
            btnSelectTarget.isEnabled = connected
            btnCall.isEnabled = connected
        }

        ServiceState.isPlayingVoice.observe(viewLifecycleOwner) { playing ->
            val connected = ServiceState.connectionState.value == BleState.CONNECTED
            pttButton.isEnabled = connected && !playing
            if (playing) {
                tvStatusLine.text = "● воспроизведение..."
                tvStatusLine.setTextColor(Colors.blueAccent)
            }
        }

        ServiceState.isPttActive.observe(viewLifecycleOwner) { active ->
            if (active) {
                val isVox = ServiceState.txMode.value == TxMode.VOX
                pttButton.state = if (isVox) PttButtonView.State.VOX_TX else PttButtonView.State.TX
                tvStatusLine.text = "● передача..."
                tvStatusLine.setTextColor(Colors.redTx)
            } else {
                val isVox = ServiceState.txMode.value == TxMode.VOX
                pttButton.state = if (isVox) PttButtonView.State.VOX_IDLE else PttButtonView.State.IDLE
                tvStatusLine.text = "● ожидание"
                tvStatusLine.setTextColor(Colors.greenDim)
            }
        }

        ServiceState.txMode.observe(viewLifecycleOwner) { mode ->
            val isVox = mode == TxMode.VOX
            pttButton.state = if (isVox) PttButtonView.State.VOX_IDLE else PttButtonView.State.IDLE
            tvVoxState.visibility = if (isVox) View.VISIBLE else View.GONE
        }

        ServiceState.voxState.observe(viewLifecycleOwner) { state ->
            tvVoxState.text = when (state) {
                VoxEngine.State.IDLE -> ""
                VoxEngine.State.ATTACK -> "..."
                VoxEngine.State.ACTIVE -> ">>> TX <<<"
                VoxEngine.State.HANGTIME -> "TX (пауза)"
                else -> ""
            }
        }

        ServiceState.rmsLevel.observe(viewLifecycleOwner) { rms ->
            pttButton.rmsLevel = rms / 5000f
        }

        ServiceState.listenMode.observe(viewLifecycleOwner) { mode ->
            updateListenButtons(mode)
        }

        // === Инфо входящего broadcast вызова (пузырёк слева) ===
        val layoutRxInfo = v.findViewById<View>(R.id.layoutRxInfo)
        val tvRxCallSign = v.findViewById<TextView>(R.id.tvRxCallSign)
        val tvRxSignal = v.findViewById<TextView>(R.id.tvRxSignal)
        val rxHideHandler = android.os.Handler(android.os.Looper.getMainLooper())
        val rxHideRunnable = Runnable { layoutRxInfo.visibility = View.GONE }

        ServiceState.isReceiving.observe(viewLifecycleOwner) { receiving ->
            val senderId = ServiceState.lastRxDeviceId.value ?: ""
            val isAddressed = targetId != null && senderId == targetId

            if (receiving) {
                if (isAddressed) {
                    // Адресный — не показывать пузырёк, PTT в RX
                    pttButton.state = PttButtonView.State.RX
                    tvStatusLine.text = "● приём"
                    tvStatusLine.setTextColor(Colors.blueAccent)
                } else {
                    // Broadcast — показать пузырёк, авто-скрыть через 5 сек
                    layoutRxInfo.visibility = View.VISIBLE
                    rxHideHandler.removeCallbacks(rxHideRunnable)
                    pttButton.state = PttButtonView.State.RX
                    tvStatusLine.text = "● приём"
                    tvStatusLine.setTextColor(Colors.blueAccent)
                }
            } else if (ServiceState.isPttActive.value != true) {
                pttButton.state = PttButtonView.State.IDLE
                tvStatusLine.text = "● ожидание"
                tvStatusLine.setTextColor(Colors.greenDim)
                // Скрыть пузырёк через 5 сек
                rxHideHandler.removeCallbacks(rxHideRunnable)
                rxHideHandler.postDelayed(rxHideRunnable, 5000)
            }
        }
        ServiceState.lastRxCallSign.observe(viewLifecycleOwner) { name ->
            if (name.isNotEmpty()) tvRxCallSign.text = name
        }
        ServiceState.lastRxDeviceId.observe(viewLifecycleOwner) { _ -> }

        // Пункт 2: авто-выбор адресата только при входящем PRIVATE вызове
        ServiceState.incomingCall.observe(viewLifecycleOwner) { call ->
            if (call != null && call.callType == CallType.PRIVATE) {
                val senderId = call.senderId.takeLast(4)
                if (senderId.isNotEmpty() && senderId != "0000") {
                    targetId = senderId
                    targetName = call.callSign
                    updateTargetUI()
                }
            }
        }
        ServiceState.lastRxRssi.observe(viewLifecycleOwner) { rssi ->
            val snr = ServiceState.lastRxSnr.value ?: 0
            tvRxSignal.text = "${rssi}dBm / ${snr}dB"
            tvRxSignal.setTextColor(Colors.rssiColor(rssi))
        }

        ServiceState.recentCalls.observe(viewLifecycleOwner) { calls ->
            // Обновить RSSI из текущих peers
            val peers = ServiceState.peers.value ?: emptyList()
            val updated = calls.take(20).map { call ->
                val peer = peers.find { it.deviceId.endsWith(call.deviceId.takeLast(4)) }
                if (peer != null) call.copy(rssi = peer.rssi) else call
            }
            rvRecent.adapter = RecentCallAdapter(updated) { call -> redial(call) }
        }

        // Обновлять target info при изменении peers
        ServiceState.peers.observe(viewLifecycleOwner) {
            if (targetId != null) updateTargetUI()
        }

        return v
    }

    private fun sendBufferedVoice(tvStatusLine: TextView, tvDeliveryStatus: TextView, pttButton: PttButtonView) {
        val data = voiceRecorder.stopAndEncode()

        if (data == null || targetId == null) {
            pttButton.state = PttButtonView.State.IDLE
            tvStatusLine.text = "● ожидание"
            tvStatusLine.setTextColor(Colors.greenDim)
            return
        }

        // Кнопка серая + "ОТПРАВКА"
        pttButton.isEnabled = false
        pttButton.state = PttButtonView.State.SENDING
        tvStatusLine.text = "● отправка..."
        tvStatusLine.setTextColor(Colors.amberAccent)
        tvDeliveryStatus.visibility = View.GONE

        val destMac = byteArrayOf(
            targetId!!.substring(0, 2).toInt(16).toByte(),
            targetId!!.substring(2, 4).toInt(16).toByte()
        )
        val fileName = "ptt_${System.currentTimeMillis()}.c2"
        service?.sendFile(fileName, 0x05, data, destMac, targetName) // FILE_TYPE_PTT_VOICE

        val handler = android.os.Handler(android.os.Looper.getMainLooper())
        Thread {
            var delivered = false
            for (i in 0..30) { // 15 сек
                Thread.sleep(500)
                val transfers = ServiceState.fileTransfers.value
                val last = transfers?.firstOrNull { it.isOutgoing && it.fileName == fileName }
                if (last?.status == com.meshtrx.app.model.FileStatus.DONE) {
                    delivered = true
                    break
                }
            }
            handler.post {
                pttButton.isEnabled = true
                pttButton.state = PttButtonView.State.IDLE
                if (delivered) {
                    tvStatusLine.text = "● доставлено"
                    tvStatusLine.setTextColor(Colors.greenAccent)
                    handler.postDelayed({
                        tvStatusLine.text = "● ожидание"
                        tvStatusLine.setTextColor(Colors.greenDim)
                    }, 3000)
                } else {
                    tvStatusLine.text = "● не доставлено"
                    tvStatusLine.setTextColor(Colors.redTx)
                    handler.postDelayed({
                        tvStatusLine.text = "● ожидание"
                        tvStatusLine.setTextColor(Colors.greenDim)
                    }, 5000)
                }
            }
        }.start()
    }

    private fun redial(call: RecentCall) {
        when (call.callType) {
            "ALL" -> {
                service?.bleManager?.send(byteArrayOf(BleManager.CMD_CALL_ALL.toByte()))
                service?.addRecentCall(call.copy(timeMs = System.currentTimeMillis()))
            }
            "PRIVATE" -> {
                val idBytes = call.deviceId.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
                val pkt = ByteArray(1 + 4)
                pkt[0] = BleManager.CMD_CALL_PRIVATE.toByte()
                System.arraycopy(idBytes, 0, pkt, 1, idBytes.size.coerceAtMost(4))
                service?.bleManager?.send(pkt)
                service?.addRecentCall(call.copy(timeMs = System.currentTimeMillis()))
            }
        }
        Toast.makeText(requireContext(), getString(R.string.calling, call.callSign), Toast.LENGTH_SHORT).show()
    }

    inner class RecentCallAdapter(
        private val calls: List<RecentCall>,
        private val onRedial: (RecentCall) -> Unit
    ) : androidx.recyclerview.widget.RecyclerView.Adapter<RecentCallAdapter.VH>() {

        inner class VH(view: View) : androidx.recyclerview.widget.RecyclerView.ViewHolder(view) {
            val tvIcon: TextView = view.findViewById(R.id.tvCallIcon)
            val tvName: TextView = view.findViewById(R.id.tvRecentName)
            val tvInfo: TextView = view.findViewById(R.id.tvRecentInfo)
            val btnCall: ImageButton = view.findViewById(R.id.btnRecentCall)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            return VH(LayoutInflater.from(parent.context).inflate(R.layout.item_recent_call, parent, false))
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val call = calls[position]

            holder.tvIcon.text = if (call.isOutgoing) "→" else "←"
            val typeColor = when (call.callType) {
                "PRIVATE" -> Colors.greenAccent; "ALL" -> Colors.blueAccent
                "GROUP" -> Colors.amberAccent; "SOS" -> Colors.redAccent
                else -> Colors.textDim
            }
            holder.tvIcon.setTextColor(typeColor)

            val name = when {
                call.callType == "ALL" -> getString(R.string.general_channel)
                call.callType == "SOS" -> "SOS"
                call.groupName != null -> call.groupName
                else -> call.callSign
            }
            holder.tvName.text = name
            holder.tvName.setTextColor(Colors.textSecondary)

            val time = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault())
                .format(java.util.Date(call.timeMs))
            val typeStr = when (call.callType) {
                "ALL" -> getString(R.string.call_type_all); "PRIVATE" -> getString(R.string.call_type_private); "GROUP" -> getString(R.string.call_type_group); "SOS" -> "SOS"
                else -> call.callType
            }
            val rssiStr = call.rssi?.let { " · ${it}dBm" } ?: ""
            holder.tvInfo.text = "$time · $typeStr$rssiStr"
            holder.tvInfo.setTextColor(Colors.textDim)

            holder.btnCall.setOnClickListener { onRedial(call) }
            holder.itemView.setOnClickListener { onRedial(call) }
        }

        override fun getItemCount() = calls.size
    }
}
