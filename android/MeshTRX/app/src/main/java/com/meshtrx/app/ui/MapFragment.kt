package com.meshtrx.app.ui

import android.graphics.Color
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import com.meshtrx.app.R
import com.meshtrx.app.ServiceState
import com.meshtrx.app.model.distanceKm
import org.osmdroid.config.Configuration
import org.osmdroid.tileprovider.tilesource.TileSourceFactory
import org.osmdroid.util.BoundingBox
import org.osmdroid.util.GeoPoint
import org.osmdroid.views.MapView
import org.osmdroid.views.overlay.Marker
import org.osmdroid.views.overlay.Polyline

class MapFragment : Fragment() {

    private var mapView: MapView? = null
    private var radarView: RadarView? = null
    private var isRadarMode = false
    private var centeredOnce = false

    override fun onCreateView(inflater: LayoutInflater, c: ViewGroup?, s: Bundle?): View {
        val ctx = requireContext()
        Configuration.getInstance().userAgentValue = ctx.packageName

        val root = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(0xFF141414.toInt())
        }

        // Табы: Карта / Радар
        val tabBar = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(0xFF111111.toInt())
            gravity = Gravity.CENTER
            setPadding(0, 8, 0, 8)
        }

        val btnMap = Button(ctx).apply {
            text = getString(R.string.map_tab)
            setTextColor(0xFF4ade80.toInt())
            setBackgroundColor(0xFF1a3a1a.toInt())
            setPadding(32, 8, 32, 8)
        }
        val btnRadar = Button(ctx).apply {
            text = getString(R.string.radar_tab)
            setTextColor(0xFF888888.toInt())
            setBackgroundColor(0xFF1a1a1a.toInt())
            setPadding(32, 8, 32, 8)
        }
        tabBar.addView(btnMap, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        tabBar.addView(btnRadar, LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1f))
        root.addView(tabBar)

        // Контейнер для карты и радара
        val container = FrameLayout(ctx)
        root.addView(container, LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f))

        // OSM Map
        mapView = MapView(ctx).apply {
            setTileSource(TileSourceFactory.MAPNIK)
            setMultiTouchControls(true)
            controller.setZoom(14.0)
        }
        container.addView(mapView)

        // Radar
        radarView = RadarView(ctx).apply {
            visibility = View.GONE
        }
        container.addView(radarView)

        // FAB контейнер
        val fabContainer = FrameLayout(ctx)
        container.addView(fabContainer, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT))

        val fabMyPos = createFab(ctx, getString(R.string.fab_me)).apply {
            setOnClickListener { centerOnMe() }
        }
        val fabAll = createFab(ctx, getString(R.string.fab_all)).apply {
            setOnClickListener { zoomToFitAll() }
        }
        fabContainer.addView(fabMyPos, FrameLayout.LayoutParams(
            100, 100, Gravity.BOTTOM or Gravity.END).apply {
            setMargins(0, 0, 16, 180)
        })
        fabContainer.addView(fabAll, FrameLayout.LayoutParams(
            100, 100, Gravity.BOTTOM or Gravity.END).apply {
            setMargins(0, 0, 16, 72)
        })

        // GPS статус (поверх карты и радара)
        val tvGps = TextView(ctx).apply {
            text = getString(R.string.gps_waiting)
            setTextColor(0xFFf59e0b.toInt())
            textSize = 12f
            setPadding(16, 4, 16, 4)
            setBackgroundColor(0xCC111111.toInt())
        }
        container.addView(tvGps, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT, FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.START).apply { setMargins(8, 8, 0, 0) })

        ServiceState.myLat.observe(viewLifecycleOwner) { lat ->
            val lon = ServiceState.myLon.value
            if (lat != null && lon != null) {
                tvGps.text = "GPS: %.5f, %.5f".format(lat, lon)
                tvGps.setTextColor(0xFF4ade80.toInt())
                if (!centeredOnce && !isRadarMode) {
                    centeredOnce = true
                    mapView?.controller?.setCenter(org.osmdroid.util.GeoPoint(lat, lon))
                    mapView?.controller?.setZoom(15.0)
                    updateMap()
                }
            } else {
                tvGps.text = getString(R.string.gps_waiting)
                tvGps.setTextColor(0xFFf59e0b.toInt())
            }
        }

        // Табы переключение
        btnMap.setOnClickListener {
            isRadarMode = false
            mapView?.visibility = View.VISIBLE
            radarView?.visibility = View.GONE
            radarView?.stopCompass()
            fabContainer.visibility = View.VISIBLE
            btnMap.setTextColor(0xFF4ade80.toInt()); btnMap.setBackgroundColor(0xFF1a3a1a.toInt())
            btnRadar.setTextColor(0xFF888888.toInt()); btnRadar.setBackgroundColor(0xFF1a1a1a.toInt())
            updateMap()
        }
        btnRadar.setOnClickListener {
            isRadarMode = true
            mapView?.visibility = View.GONE
            radarView?.visibility = View.VISIBLE
            radarView?.startCompass()
            fabContainer.visibility = View.GONE
            btnRadar.setTextColor(0xFF4ade80.toInt()); btnRadar.setBackgroundColor(0xFF1a3a1a.toInt())
            btnMap.setTextColor(0xFF888888.toInt()); btnMap.setBackgroundColor(0xFF1a1a1a.toInt())
        }

        // Observers
        ServiceState.peers.observe(viewLifecycleOwner) {
            if (!isRadarMode) updateMap()
        }
        ServiceState.myLat.observe(viewLifecycleOwner) {
            if (!isRadarMode) updateMap()
        }

        // Начальная позиция
        updateMap()

        return root
    }

    private fun createFab(ctx: android.content.Context, label: String): Button {
        return Button(ctx).apply {
            text = label
            textSize = 11f
            setTextColor(0xFFe8e8e8.toInt())
            val bg = GradientDrawable().apply {
                shape = GradientDrawable.OVAL
                setColor(0xFF1a3a1a.toInt())
                setStroke(2, 0xFF2a5a2a.toInt())
            }
            background = bg
            setPadding(0, 0, 0, 0)
            gravity = Gravity.CENTER
        }
    }

    private fun updateMap() {
        val map = mapView ?: return
        map.overlays.clear()

        val myLat = ServiceState.myLat.value
        val myLon = ServiceState.myLon.value

        // Маркер "Я"
        if (myLat != null && myLon != null) {
            val myMarker = Marker(map).apply {
                position = GeoPoint(myLat, myLon)
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_CENTER)
                title = getString(R.string.me_label)
                snippet = "%.5f, %.5f".format(myLat, myLon)
            }
            map.overlays.add(myMarker)
        }

        // Peers
        val peers = ServiceState.peers.value ?: emptyList()
        val now = System.currentTimeMillis()
        for (peer in peers) {
            if (peer.lat == null || peer.lon == null) continue
            val ageSec = (now - peer.lastSeenMs) / 1000

            val marker = Marker(map).apply {
                position = GeoPoint(peer.lat, peer.lon)
                setAnchor(Marker.ANCHOR_CENTER, Marker.ANCHOR_BOTTOM)
                title = peer.callSign
                val dist = if (myLat != null && myLon != null) {
                    val d = distanceKm(myLat, myLon, peer.lat, peer.lon)
                    if (d < 1) "${(d * 1000).toInt()}м" else "%.1fкм".format(d)
                } else "?"
                snippet = "${peer.rssi}dBm · $dist · ${formatAge(ageSec)}"
            }
            map.overlays.add(marker)

            // Линия до peer
            if (myLat != null && myLon != null) {
                val line = Polyline().apply {
                    addPoint(GeoPoint(myLat, myLon))
                    addPoint(GeoPoint(peer.lat, peer.lon))
                    outlinePaint.color = when {
                        ageSec < 300 -> 0x604ade80.toInt()
                        ageSec < 600 -> 0x60f59e0b.toInt()
                        else -> 0x60f87171.toInt()
                    }
                    outlinePaint.strokeWidth = 2f
                    outlinePaint.pathEffect = android.graphics.DashPathEffect(floatArrayOf(10f, 10f), 0f)
                }
                map.overlays.add(line)
            }
        }

        map.invalidate()
    }

    private fun centerOnMe() {
        val lat = ServiceState.myLat.value
        val lon = ServiceState.myLon.value
        if (lat == null || lon == null) {
            Toast.makeText(requireContext(), getString(R.string.gps_no_coords), Toast.LENGTH_SHORT).show()
            return
        }
        mapView?.controller?.animateTo(GeoPoint(lat, lon))
        mapView?.controller?.setZoom(16.0)
        updateMap()
    }

    private fun zoomToFitAll() {
        val points = mutableListOf<GeoPoint>()
        ServiceState.myLat.value?.let { lat ->
            ServiceState.myLon.value?.let { lon -> points.add(GeoPoint(lat, lon)) }
        }
        ServiceState.peers.value?.forEach { peer ->
            if (peer.lat != null && peer.lon != null) points.add(GeoPoint(peer.lat, peer.lon))
        }
        if (points.size < 2) {
            centerOnMe()
            return
        }
        val box = BoundingBox.fromGeoPoints(points)
        mapView?.zoomToBoundingBox(box.increaseByScale(1.3f), true)
    }

    private fun formatAge(sec: Long): String = when {
        sec < 60 -> "${sec}с"
        sec < 3600 -> "${sec / 60}мин"
        else -> "${sec / 3600}ч"
    }

    override fun onResume() {
        super.onResume()
        mapView?.onResume()
        if (isRadarMode) radarView?.startCompass()
    }

    override fun onPause() {
        super.onPause()
        mapView?.onPause()
        radarView?.stopCompass()
    }

    override fun onDestroyView() {
        mapView?.onDetach()
        radarView?.stopCompass()
        super.onDestroyView()
    }
}
