package com.meshtrx.app.ui

import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import com.google.android.material.tabs.TabLayout
import com.meshtrx.app.*
import com.meshtrx.app.model.*
import com.meshtrx.app.model.distanceKm

class CallPickerSheet : BottomSheetDialogFragment() {

    private val service: MeshTRXService? get() = (activity as? MainActivity)?.service

    private lateinit var rvList: RecyclerView
    private lateinit var layoutSort: View
    private lateinit var btnNewGroup: Button

    private var showingPeers = true
    enum class SortMode { RSSI, NAME, DISTANCE }
    private var sortMode = SortMode.RSSI

    // Группы (сохраняются локально)
    data class Group(val name: String, val memberIds: List<String>)
    private val groups = mutableListOf<Group>() // TODO: загружать из SharedPreferences

    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val v = inflater.inflate(R.layout.sheet_call_picker, c, false)

        val tabs = v.findViewById<TabLayout>(R.id.tabsCallType)
        rvList = v.findViewById(R.id.rvList)
        layoutSort = v.findViewById(R.id.layoutSort)
        btnNewGroup = v.findViewById(R.id.btnNewGroup)
        val btnSortRssi = v.findViewById<Button>(R.id.btnSortRssi)
        val btnSortName = v.findViewById<Button>(R.id.btnSortName)
        val btnSortDist = v.findViewById<Button>(R.id.btnSortDist)

        rvList.layoutManager = LinearLayoutManager(requireContext())

        // Табы
        tabs.addTab(tabs.newTab().setText(getString(R.string.stations)))
        tabs.addTab(tabs.newTab().setText(getString(R.string.groups)))
        tabs.addOnTabSelectedListener(object : TabLayout.OnTabSelectedListener {
            override fun onTabSelected(tab: TabLayout.Tab) {
                showingPeers = tab.position == 0
                layoutSort.visibility = if (showingPeers) View.VISIBLE else View.GONE
                btnNewGroup.visibility = if (showingPeers) View.GONE else View.VISIBLE
                updateList()
            }
            override fun onTabUnselected(tab: TabLayout.Tab) {}
            override fun onTabReselected(tab: TabLayout.Tab) {}
        })

        // Сортировка
        btnSortRssi.setOnClickListener { sortMode = SortMode.RSSI; updateList() }
        btnSortName.setOnClickListener { sortMode = SortMode.NAME; updateList() }
        btnSortDist.setOnClickListener { sortMode = SortMode.DISTANCE; updateList() }

        btnNewGroup.setOnClickListener {
            // TODO: диалог создания группы
            Toast.makeText(requireContext(), getString(R.string.groups_coming_soon), Toast.LENGTH_SHORT).show()
        }

        // Обновлять при изменении peers
        ServiceState.peers.observe(viewLifecycleOwner) { updateList() }

        updateList()
        return v
    }

    private fun peerDistance(peer: Peer): Double? {
        val myLat = ServiceState.myLat.value ?: return null
        val myLon = ServiceState.myLon.value ?: return null
        val pLat = peer.lat ?: return null
        val pLon = peer.lon ?: return null
        return distanceKm(myLat, myLon, pLat, pLon)
    }

    private fun updateList() {
        if (showingPeers) {
            val peers = ServiceState.peers.value ?: emptyList()
            val sorted = when (sortMode) {
                SortMode.RSSI -> peers.sortedByDescending { it.rssi }
                SortMode.NAME -> peers.sortedBy { it.callSign }
                SortMode.DISTANCE -> peers.sortedBy { peerDistance(it) ?: Double.MAX_VALUE }
            }
            rvList.adapter = PeerAdapter(sorted) { peer -> callPeer(peer) }
        } else {
            rvList.adapter = GroupAdapter(groups) { group -> callGroup(group) }
        }
    }

    private fun callPeer(peer: Peer) {
        val idBytes = peer.deviceId.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val pkt = ByteArray(1 + 4)
        pkt[0] = BleManager.CMD_CALL_PRIVATE.toByte()
        System.arraycopy(idBytes, 0, pkt, 1, idBytes.size.coerceAtMost(4))
        service?.bleManager?.send(pkt)
        service?.addRecentCall(RecentCall(peer.deviceId, peer.callSign, true, "PRIVATE", rssi = peer.rssi))
        Toast.makeText(requireContext(), getString(R.string.calling, peer.callSign), Toast.LENGTH_SHORT).show()
        dismiss()
    }

    private fun callGroup(group: Group) {
        // GROUP CALL: ad-hoc, members
        val count = group.memberIds.size.coerceAtMost(8)
        val pkt = ByteArray(3 + count * 4)
        pkt[0] = BleManager.CMD_CALL_GROUP.toByte()
        pkt[1] = 0xFF.toByte() // ad-hoc
        pkt[2] = count.toByte()
        for (i in 0 until count) {
            val idBytes = group.memberIds[i].chunked(2).map { it.toInt(16).toByte() }.toByteArray()
            System.arraycopy(idBytes, 0, pkt, 3 + i * 4, idBytes.size.coerceAtMost(4))
        }
        service?.bleManager?.send(pkt)
        service?.addRecentCall(RecentCall("GROUP_${group.name}", group.name, true, "GROUP", groupName = group.name))
        Toast.makeText(requireContext(), getString(R.string.group_call, group.name), Toast.LENGTH_SHORT).show()
        dismiss()
    }

    // === Адаптеры ===

    inner class PeerAdapter(
        private val peers: List<Peer>,
        private val onCall: (Peer) -> Unit
    ) : RecyclerView.Adapter<PeerAdapter.VH>() {

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val viewStatus: View = view.findViewById(R.id.viewStatus)
            val tvCallSign: TextView = view.findViewById(R.id.tvCallSign)
            val tvDetails: TextView = view.findViewById(R.id.tvDetails)
            val tvLastSeen: TextView = view.findViewById(R.id.tvLastSeen)
            val progressSignal: ProgressBar = view.findViewById(R.id.progressSignal)
            val btnCall: ImageButton = view.findViewById(R.id.btnCall)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.item_peer, parent, false)
            return VH(view)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val peer = peers[position]
            holder.tvCallSign.text = peer.callSign
            val dist = peerDistance(peer)
            val distStr = dist?.let { "  %.1f km".format(it) } ?: ""
            holder.tvDetails.text = "${peer.rssi}dBm  ${peer.txPower}dBm" +
                    (peer.batteryPct?.let { "  $it%" } ?: "") + distStr

            // Время с последнего пинга
            val ago = (System.currentTimeMillis() - peer.lastSeenMs) / 1000
            holder.tvLastSeen.text = when {
                ago < 60 -> "${ago}с назад"
                ago < 3600 -> "${ago / 60}мин назад"
                else -> "${ago / 3600}ч назад"
            }

            // Статус цвет
            val statusColor = when {
                ago < 300 -> android.graphics.Color.parseColor("#4CAF50")   // зелёный
                ago < 600 -> android.graphics.Color.parseColor("#FFC107")   // жёлтый
                else -> android.graphics.Color.parseColor("#F44336")         // красный
            }
            holder.viewStatus.setBackgroundColor(statusColor)

            // RSSI бар: -120..-30
            holder.progressSignal.progress = ((peer.rssi + 120) * 100 / 90).coerceIn(0, 100)

            holder.btnCall.setOnClickListener { onCall(peer) }
            holder.itemView.setOnClickListener { onCall(peer) }
        }

        override fun getItemCount() = peers.size
    }

    inner class GroupAdapter(
        private val groups: List<Group>,
        private val onCall: (Group) -> Unit
    ) : RecyclerView.Adapter<GroupAdapter.VH>() {

        inner class VH(view: View) : RecyclerView.ViewHolder(view) {
            val tvName: TextView = view.findViewById(R.id.tvGroupName)
            val tvMembers: TextView = view.findViewById(R.id.tvGroupMembers)
            val btnCall: ImageButton = view.findViewById(R.id.btnCallGroup)
        }

        override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
            val view = LayoutInflater.from(parent.context).inflate(R.layout.item_group, parent, false)
            return VH(view)
        }

        override fun onBindViewHolder(holder: VH, position: Int) {
            val group = groups[position]
            holder.tvName.text = group.name
            holder.tvMembers.text = "${group.memberIds.size} участников"
            holder.btnCall.setOnClickListener { onCall(group) }
            holder.itemView.setOnClickListener { onCall(group) }
        }

        override fun getItemCount() = groups.size
    }
}
