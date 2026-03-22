package com.meshtrx.app.ui

import android.os.Bundle
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
        }

        ServiceState.messages.observe(viewLifecycleOwner) {
            updateFilterOptions(spinnerFilter)
            applyFilter()
        }

        return v
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

            val rssiStr = msg.rssi?.let { " ${it}dBm" } ?: ""
            val destStr = if (msg.isOutgoing && msg.destName != null) " → ${msg.destName}" else ""
            holder.tvMeta.text = "${msg.time}$destStr$rssiStr"
        }

        override fun getItemCount() = messages.size

        private fun Int.dp(): Int = (this * resources.displayMetrics.density).toInt()
    }
}
