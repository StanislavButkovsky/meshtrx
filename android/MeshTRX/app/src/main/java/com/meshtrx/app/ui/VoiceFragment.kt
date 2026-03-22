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

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.fragment_voice, c, false)

        val switchVox = v.findViewById<SwitchMaterial>(R.id.switchVox)
        val tvVoxState = v.findViewById<TextView>(R.id.tvVoxState)
        val tvStatusLine = v.findViewById<TextView>(R.id.tvStatusLine)
        val pttButton = v.findViewById<PttButtonView>(R.id.pttButton)
        val btnListenAll = v.findViewById<Button>(R.id.btnListenAll)
        val btnListenMy = v.findViewById<Button>(R.id.btnListenMy)
        val btnCallAll = v.findViewById<Button>(R.id.btnCallAll)
        val btnCallPrivate = v.findViewById<Button>(R.id.btnCallPrivate)
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

        // PTT — touch на кастомной кнопке
        pttButton.setOnTouchListener { view, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    if (ServiceState.txMode.value == TxMode.PTT) service?.pttDown()
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    if (ServiceState.txMode.value == TxMode.PTT) service?.pttUp()
                    true
                }
                else -> true
            }
        }

        // VOX switch
        switchVox.setOnCheckedChangeListener { _, isVox ->
            service?.setTxMode(if (isVox) TxMode.VOX else TxMode.PTT)
        }

        // Вызовы
        btnCallAll.setOnClickListener {
            service?.bleManager?.send(byteArrayOf(BleManager.CMD_CALL_ALL.toByte()))
            service?.addRecentCall(RecentCall("BROADCAST", getString(R.string.general_channel), true, "ALL"))
            Toast.makeText(requireContext(), getString(R.string.call_general), Toast.LENGTH_SHORT).show()
        }
        btnCallPrivate.setOnClickListener {
            CallPickerSheet().show(parentFragmentManager, "callPicker")
        }

        // === Observers ===
        ServiceState.connectionState.observe(viewLifecycleOwner) { state ->
            val connected = state == BleState.CONNECTED
            pttButton.isEnabled = connected
            switchVox.isEnabled = connected
            btnCallAll.isEnabled = connected
            btnCallPrivate.isEnabled = connected
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

        ServiceState.recentCalls.observe(viewLifecycleOwner) { calls ->
            rvRecent.adapter = RecentCallAdapter(calls.take(20)) { call -> redial(call) }
        }

        return v
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
