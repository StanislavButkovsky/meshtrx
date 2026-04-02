package com.meshtrx.app.ui

import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.meshtrx.app.R
import com.meshtrx.app.ServiceState
import com.meshtrx.app.model.Peer

class FileDestPickerSheet : BottomSheetDialogFragment() {

    var onSelected: ((destMac: ByteArray?, destName: String) -> Unit)? = null
    var showBroadcast: Boolean = true // false для файлов (только адресная)
    var customTitle: String? = null

    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val ctx = requireContext()
        val layout = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, 24, 0, 24)
            setBackgroundColor(0xFF1a1a1a.toInt())
        }

        val title = TextView(ctx).apply {
            text = customTitle ?: getString(R.string.send_to)
            textSize = 18f
            setTextColor(0xFFe8e8e8.toInt())
            setPadding(32, 16, 32, 16)
        }
        layout.addView(title)

        val rv = RecyclerView(ctx).apply {
            layoutManager = LinearLayoutManager(ctx)
        }
        layout.addView(rv, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT
        ))

        val peers = ServiceState.peers.value?.sortedByDescending { it.rssi } ?: emptyList()
        rv.adapter = DestAdapter(peers)

        return layout
    }

    private fun peerToMac(peer: Peer): ByteArray {
        val id = peer.deviceId.takeLast(4)
        val b0 = id.substring(0, 2).toIntOrNull(16)?.toByte() ?: 0
        val b1 = id.substring(2, 4).toIntOrNull(16)?.toByte() ?: 0
        return byteArrayOf(b0, b1)
    }

    inner class DestAdapter(private val peers: List<Peer>) :
        RecyclerView.Adapter<DestAdapter.VH>() {

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val tvName: TextView = view.findViewById(android.R.id.text1)
            val tvInfo: TextView = view.findViewById(android.R.id.text2)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context)
                .inflate(android.R.layout.simple_list_item_2, parent, false)
            view.findViewById<TextView>(android.R.id.text1).setTextColor(0xFFe8e8e8.toInt())
            view.findViewById<TextView>(android.R.id.text2).setTextColor(0xFF888888.toInt())
            view.setBackgroundColor(0xFF1a1a1a.toInt())
            return VH(view)
        }

        private val offset = if (showBroadcast) 1 else 0

        override fun onBindViewHolder(holder: VH, position: Int) {
            if (showBroadcast && position == 0) {
                holder.tvName.text = getString(R.string.all)
                holder.tvInfo.text = getString(R.string.broadcast_channel)
                holder.itemView.setOnClickListener {
                    onSelected?.invoke(null, getString(R.string.all))
                    dismiss()
                }
            } else {
                val peer = peers[position - offset]
                holder.tvName.text = peer.callSign
                val ago = (System.currentTimeMillis() - peer.lastSeenMs) / 1000
                val agoStr = when {
                    ago < 60 -> "${ago}с"
                    ago < 3600 -> "${ago / 60}мин"
                    else -> "${ago / 3600}ч"
                }
                holder.tvInfo.text = "${peer.rssi}dBm · $agoStr назад"
                holder.itemView.setOnClickListener {
                    onSelected?.invoke(peerToMac(peer), peer.callSign)
                    dismiss()
                }
            }
        }

        override fun getItemCount() = peers.size + offset
    }
}
