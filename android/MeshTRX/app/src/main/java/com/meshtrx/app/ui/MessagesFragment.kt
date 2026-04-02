package com.meshtrx.app.ui

import android.annotation.SuppressLint
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.meshtrx.app.*
import com.meshtrx.app.model.*

class MessagesFragment : Fragment() {

    private val service: MeshTRXService? get() = (activity as? MainActivity)?.service

    private var destId: String? = null
    private var destName: String? = null
    private lateinit var tvDest: TextView
    private lateinit var rvMessages: RecyclerView
    private var currentFilter: String? = null // null = все
    private val voiceRecorder = VoiceRecorder()
    private val handler = Handler(Looper.getMainLooper())

    @SuppressLint("ClickableViewAccessibility")
    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.fragment_messages, c, false)

        val spinnerFilter = v.findViewById<Spinner>(R.id.spinnerFilter)
        rvMessages = v.findViewById(R.id.rvMessages)
        tvDest = v.findViewById(R.id.tvDest)
        val btnDest = v.findViewById<ImageButton>(R.id.btnDest)
        val etMessage = v.findViewById<EditText>(R.id.etMessage)
        val btnSend = v.findViewById<ImageButton>(R.id.btnSend)

        rvMessages.layoutManager = LinearLayoutManager(requireContext()).apply {
            stackFromEnd = true
        }

        // Отправка
        btnSend.setOnClickListener {
            val text = etMessage.text.toString().trim()
            if (text.isNotEmpty() && text.length <= 84) {
                service?.sendTextMessage(text, destId, destName)
                etMessage.text.clear()
            }
        }

        // Голосовое сообщение
        val btnVoice = v.findViewById<ImageButton>(R.id.btnVoice)
        val layoutRecording = v.findViewById<LinearLayout>(R.id.layoutRecording)
        val tvRecTime = v.findViewById<TextView>(R.id.tvRecTime)
        val pbRecLevel = v.findViewById<ProgressBar>(R.id.pbRecLevel)
        val tvRecCancel = v.findViewById<TextView>(R.id.tvRecCancel)

        fun updateVoiceButtonVisibility() {
            // Показывать кнопку 🎤 только при адресной отправке и подключении
            val connected = ServiceState.connectionState.value == BleState.CONNECTED
            btnVoice.visibility = if (destId != null && connected) View.VISIBLE else View.GONE
        }

        voiceRecorder.onTimeUpdate = { sec ->
            handler.post { tvRecTime.text = String.format("0:%02d", sec) }
        }
        voiceRecorder.onRmsLevel = { rms ->
            handler.post { pbRecLevel.progress = rms.coerceAtMost(5000) }
        }
        voiceRecorder.onMaxReached = {
            handler.post { finishVoiceRecording(layoutRecording, etMessage, btnSend, btnVoice) }
        }

        btnVoice.setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> {
                    // Начать запись
                    voiceRecorder.startRecording()
                    layoutRecording.visibility = View.VISIBLE
                    etMessage.visibility = View.GONE
                    btnSend.visibility = View.GONE
                    tvRecTime.text = "0:00"
                    pbRecLevel.progress = 0
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    finishVoiceRecording(layoutRecording, etMessage, btnSend, btnVoice)
                    true
                }
                else -> false
            }
        }

        tvRecCancel.setOnClickListener {
            voiceRecorder.cancel()
            layoutRecording.visibility = View.GONE
            etMessage.visibility = View.VISIBLE
            btnSend.visibility = View.VISIBLE
        }

        // Выбор адресата
        btnDest.setOnClickListener {
            val sheet = FileDestPickerSheet()
            sheet.onSelected = { mac, name ->
                if (mac == null) {
                    destId = null
                    destName = null
                    tvDest.text = getString(R.string.to_all)
                } else {
                    destId = String.format("%02X%02X", mac[0].toInt() and 0xFF, mac[1].toInt() and 0xFF)
                    destName = name
                    tvDest.text = getString(R.string.to_label, name)
                }
                updateVoiceButtonVisibility()
            }
            sheet.show(parentFragmentManager, "msg_dest")
        }

        // Фильтр
        setupFilter(spinnerFilter)

        // Observers
        ServiceState.connectionState.observe(viewLifecycleOwner) { state ->
            val connected = state == BleState.CONNECTED
            btnSend.isEnabled = connected
            etMessage.isEnabled = connected
            btnDest.isEnabled = connected
            updateVoiceButtonVisibility()
        }

        ServiceState.messages.observe(viewLifecycleOwner) {
            updateFilterOptions(spinnerFilter)
            applyFilter()
        }

        return v
    }

    private fun finishVoiceRecording(layoutRecording: View, etMessage: View, btnSend: View, btnVoice: View) {
        val duration = voiceRecorder.getDurationSec()
        val data = voiceRecorder.stopAndEncode()
        layoutRecording.visibility = View.GONE
        etMessage.visibility = View.VISIBLE
        btnSend.visibility = View.VISIBLE

        if (data != null && destId != null) {
            // Отправить через файловый протокол
            val destMac = byteArrayOf(
                destId!!.substring(0, 2).toInt(16).toByte(),
                destId!!.substring(2, 4).toInt(16).toByte()
            )
            val fileName = "voice_${java.text.SimpleDateFormat("HHmm", java.util.Locale.getDefault())
                .format(java.util.Date())}.c2"
            // Сохранить .c2 на диск для воспроизведения
            val now = System.currentTimeMillis()
            val localPath = service?.saveVoiceData(fileName, now, data)

            service?.sendFile(fileName, 0x04, data, destMac, destName) // FILE_TYPE_VOICE = 0x04

            // Добавить в чат как голосовое сообщение
            val msg = ChatMessage(
                id = now, text = "\uD83C\uDFA4 ${duration}s",
                isOutgoing = true, senderId = "me",
                senderName = ServiceState.callSign.value ?: "",
                destId = destId, destName = destName,
                rssi = null, status = MessageStatus.SENDING,
                time = java.text.SimpleDateFormat("HH:mm", java.util.Locale.getDefault())
                    .format(java.util.Date()),
                timeMs = now,
                voicePath = localPath
            )
            val list = ServiceState.messages.value?.toMutableList() ?: mutableListOf()
            list.add(msg)
            ServiceState.messages.postValue(list)
            service?.saveMessages()

            Toast.makeText(requireContext(), "Voice ${duration}s → ${destName}", Toast.LENGTH_SHORT).show()
        }
    }

    private fun setupFilter(spinner: Spinner) {
        spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                val item = parent?.getItemAtPosition(pos) as? String ?: return
                currentFilter = if (item == getString(R.string.all)) null else item
                applyFilter()
            }
            override fun onNothingSelected(parent: AdapterView<*>?) {}
        }
        updateFilterOptions(spinner)
    }

    private fun updateFilterOptions(spinner: Spinner) {
        val msgs = ServiceState.messages.value ?: emptyList()
        val contacts = mutableSetOf<String>()
        msgs.forEach { m ->
            if (!m.isOutgoing && m.senderId != "??") {
                contacts.add(m.senderName.ifEmpty { "TX-${m.senderId}" })
            }
            if (m.isOutgoing && m.destName != null) {
                contacts.add(m.destName)
            }
        }
        val items = mutableListOf(getString(R.string.all))
        items.addAll(contacts.sorted())

        val adapter = ArrayAdapter(requireContext(), android.R.layout.simple_spinner_item, items)
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner.adapter = adapter

        // Восстановить выбор
        val idx = items.indexOf(currentFilter ?: getString(R.string.all))
        if (idx >= 0) spinner.setSelection(idx)
    }

    private fun applyFilter() {
        val msgs = ServiceState.messages.value ?: emptyList()
        val filtered = if (currentFilter == null) {
            msgs
        } else {
            msgs.filter { m ->
                if (m.isOutgoing) {
                    m.destName == currentFilter
                } else {
                    val name = m.senderName.ifEmpty { "TX-${m.senderId}" }
                    name == currentFilter
                }
            }
        }
        rvMessages.adapter = MessageAdapter(filtered)
        if (filtered.isNotEmpty()) {
            rvMessages.scrollToPosition(filtered.size - 1)
        }
    }

    // === Adapter ===

    inner class MessageAdapter(
        private val messages: List<ChatMessage>
    ) : RecyclerView.Adapter<MessageAdapter.VH>() {

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val bubbleLayout: View = view.findViewById(R.id.bubbleLayout)
            val tvSender: TextView = view.findViewById(R.id.tvSender)
            val tvText: TextView = view.findViewById(R.id.tvText)
            val tvMeta: TextView = view.findViewById(R.id.tvMeta)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            return VH(LayoutInflater.from(parent.context).inflate(R.layout.item_message, parent, false))
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val msg = messages[position]
            val params = holder.bubbleLayout.layoutParams as FrameLayout.LayoutParams

            if (msg.isOutgoing) {
                // Исходящие — справа, синий пузырь
                params.gravity = Gravity.END
                params.marginStart = 48.dp()
                params.marginEnd = 0
                holder.bubbleLayout.setBackgroundResource(R.drawable.bubble_outgoing)
                holder.tvSender.visibility = View.GONE
            } else {
                // Входящие — слева, серый пузырь
                params.gravity = Gravity.START
                params.marginStart = 0
                params.marginEnd = 48.dp()
                holder.bubbleLayout.setBackgroundResource(R.drawable.bubble_incoming)
                holder.tvSender.visibility = View.VISIBLE
                holder.tvSender.text = msg.senderName.ifEmpty { "TX-${msg.senderId}" }
            }
            holder.bubbleLayout.layoutParams = params

            holder.tvText.text = msg.text

            // Голосовое сообщение — кнопка воспроизведения
            if (msg.voicePath != null) {
                holder.tvText.setCompoundDrawablesWithIntrinsicBounds(
                    android.R.drawable.ic_media_play, 0, 0, 0)
                holder.tvText.compoundDrawablePadding = 8
                holder.bubbleLayout.setOnClickListener {
                    service?.playVoiceFile(msg.voicePath)
                }
            } else {
                holder.tvText.setCompoundDrawablesWithIntrinsicBounds(0, 0, 0, 0)
                holder.bubbleLayout.setOnClickListener(null)
            }

            // Мета-строка: время + адресат + RSSI + статус доставки
            val rssiStr = msg.rssi?.let { " ${it}dBm" } ?: ""
            val destStr = if (msg.isOutgoing && msg.destName != null) " → ${msg.destName}" else ""
            val statusStr = if (msg.isOutgoing && msg.destId != null) {
                when (msg.status) {
                    MessageStatus.SENDING -> " \u23F3"    // ⏳
                    MessageStatus.DELIVERED -> " \u2713"  // ✓
                    MessageStatus.FAILED -> " \u2717"     // ✗
                    else -> ""
                }
            } else ""
            holder.tvMeta.text = "${msg.time}$destStr$rssiStr$statusStr"

            // Цвет статуса
            if (msg.isOutgoing && msg.status == MessageStatus.FAILED) {
                holder.tvMeta.setTextColor(Colors.redTx)
            } else if (msg.isOutgoing && msg.status == MessageStatus.DELIVERED) {
                holder.tvMeta.setTextColor(Colors.greenAccent)
            } else {
                holder.tvMeta.setTextColor(0xFF666666.toInt())
            }

            // Retry при нажатии на FAILED сообщение
            if (msg.isOutgoing && msg.status == MessageStatus.FAILED && msg.voicePath == null) {
                // Текстовое — повторная отправка
                holder.bubbleLayout.setOnClickListener {
                    service?.sendTextMessage(msg.text, msg.destId, msg.destName)
                    Toast.makeText(requireContext(), "Retry...", Toast.LENGTH_SHORT).show()
                }
            } else if (msg.isOutgoing && msg.status == MessageStatus.FAILED && msg.voicePath != null) {
                // Голосовое — повторная отправка файла
                holder.bubbleLayout.setOnClickListener {
                    val file = java.io.File(msg.voicePath)
                    if (file.exists() && msg.destId != null) {
                        val data = file.readBytes()
                        val destMac = byteArrayOf(
                            msg.destId.substring(0, 2).toInt(16).toByte(),
                            msg.destId.substring(2, 4).toInt(16).toByte()
                        )
                        service?.sendFile("voice_retry.c2", 0x04, data, destMac, msg.destName)
                        Toast.makeText(requireContext(), "Retry voice...", Toast.LENGTH_SHORT).show()
                    }
                }
            }
        }

        override fun getItemCount() = messages.size

        private fun Int.dp(): Int = (this * resources.displayMetrics.density).toInt()
    }
}
