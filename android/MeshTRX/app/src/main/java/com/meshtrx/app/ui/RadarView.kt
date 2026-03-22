package com.meshtrx.app.ui

import android.content.Context
import android.graphics.*
import com.meshtrx.app.R
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.util.AttributeSet
import android.view.View
import com.meshtrx.app.ServiceState
import com.meshtrx.app.model.Peer
import com.meshtrx.app.model.distanceKm
import kotlin.math.*

class RadarView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null
) : View(context, attrs), SensorEventListener {

    private val sensorManager = context.getSystemService(Context.SENSOR_SERVICE) as SensorManager
    private var azimuth = 0f

    // Масштаб
    private val ranges = doubleArrayOf(0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0)
    private var rangeIndex = 5 // 5.0 км по умолчанию
    private var autoScale = true

    // Режим контраста: false = обычный (тёмный), true = высокий (для улицы)
    private var highContrast = false

    private val dp = context.resources.displayMetrics.density

    // ---- Цвета: обычный режим ----
    private object NormalColors {
        val bg = Color.BLACK
        val grid = Color.parseColor("#1a3a1a")
        val gridText = Color.parseColor("#2a5a2a")
        val center = Color.parseColor("#4ade80")
        val peer = Color.parseColor("#4ade80")
        val peerYellow = Color.parseColor("#f59e0b")
        val peerRed = Color.parseColor("#f87171")
        val cross = Color.parseColor("#0d1f0d")
        val north = Color.parseColor("#f87171")
        val text = Color.parseColor("#4ade80")
        val peerDistText = Color.parseColor("#88cc88")
        val rangeText = Color.parseColor("#888888")
        val btnBg = Color.parseColor("#222222")
        val btnStroke = Color.parseColor("#1a2a1a")
        val btnText = Color.parseColor("#aaaaaa")
        val cardinal = Color.parseColor("#2a5a2a")
    }

    // ---- Цвета: высококонтрастный режим (улица) ----
    private object HiColors {
        val bg = Color.BLACK
        val grid = Color.parseColor("#2a6a2a")
        val gridText = Color.parseColor("#55cc55")
        val center = Color.parseColor("#4ade80")
        val peer = Color.parseColor("#00ff80")
        val peerYellow = Color.parseColor("#ffcc00")
        val peerRed = Color.parseColor("#ff5555")
        val cross = Color.parseColor("#1a3a1a")
        val north = Color.parseColor("#ff6666")
        val text = Color.WHITE
        val peerDistText = Color.parseColor("#ccffcc")
        val rangeText = Color.parseColor("#cccccc")
        val btnBg = Color.parseColor("#333333")
        val btnStroke = Color.parseColor("#336633")
        val btnText = Color.WHITE
        val cardinal = Color.parseColor("#55cc55")
    }

    // Текущие цвета (переключаются)
    private var C = NormalColors

    // Paints
    private val paintGrid = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintGridText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintCross = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintCenter = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintPeer = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintPeerText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintPeerDist = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintNorth = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintCardinal = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintRange = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintButton = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintButtonText = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintButtonStroke = Paint(Paint.ANTI_ALIAS_FLAG)
    private val paintPeerRing = Paint(Paint.ANTI_ALIAS_FLAG)

    // Пульсация центра
    private var pulsePhase = 0f

    // Кнопки (rect для hit testing)
    private val btnPlusRect = RectF()
    private val btnMinusRect = RectF()
    private val btnContrastRect = RectF()

    data class PeerPoint(
        val callSign: String,
        val distKm: Double,
        val bearingDeg: Double,
        val rssi: Int,
        val ageSec: Long
    )

    init {
        applyColors()

        setOnTouchListener { _, event ->
            if (event.action == android.view.MotionEvent.ACTION_DOWN) {
                val x = event.x
                val y = event.y
                if (btnPlusRect.contains(x, y)) {
                    zoomIn(); return@setOnTouchListener true
                }
                if (btnMinusRect.contains(x, y)) {
                    zoomOut(); return@setOnTouchListener true
                }
                if (btnContrastRect.contains(x, y)) {
                    toggleContrast(); return@setOnTouchListener true
                }
            }
            false
        }
    }

    private fun applyColors() {
        val c: Any = if (highContrast) HiColors else NormalColors
        val bg: Int; val grid: Int; val gridText: Int; val center: Int
        val peer: Int; val cross: Int; val north: Int; val text: Int
        val peerDistText: Int; val rangeText: Int; val btnBg: Int
        val btnStroke: Int; val btnTextC: Int; val cardinal: Int
        val gridStroke: Float; val crossStroke: Float; val peerTextSize: Float
        val peerDistSize: Float; val northSize: Float; val cardinalSize: Float
        val shadowR: Float

        if (highContrast) {
            bg = HiColors.bg; grid = HiColors.grid; gridText = HiColors.gridText
            center = HiColors.center; peer = HiColors.peer; cross = HiColors.cross
            north = HiColors.north; text = HiColors.text; peerDistText = HiColors.peerDistText
            rangeText = HiColors.rangeText; btnBg = HiColors.btnBg; btnStroke = HiColors.btnStroke
            btnTextC = HiColors.btnText; cardinal = HiColors.cardinal
            gridStroke = 2f * dp; crossStroke = 1f * dp
            peerTextSize = 16f * dp; peerDistSize = 13f * dp
            northSize = 20f * dp; cardinalSize = 16f * dp; shadowR = 4f * dp
        } else {
            bg = NormalColors.bg; grid = NormalColors.grid; gridText = NormalColors.gridText
            center = NormalColors.center; peer = NormalColors.peer; cross = NormalColors.cross
            north = NormalColors.north; text = NormalColors.text; peerDistText = NormalColors.peerDistText
            rangeText = NormalColors.rangeText; btnBg = NormalColors.btnBg; btnStroke = NormalColors.btnStroke
            btnTextC = NormalColors.btnText; cardinal = NormalColors.cardinal
            gridStroke = 1f * dp; crossStroke = 1f * dp
            peerTextSize = 14f * dp; peerDistSize = 12f * dp
            northSize = 16f * dp; cardinalSize = 14f * dp; shadowR = 2f * dp
        }

        paintGrid.apply { color = grid; style = Paint.Style.STROKE; strokeWidth = gridStroke }
        paintGridText.apply { color = gridText; textSize = 13f * dp; typeface = Typeface.DEFAULT_BOLD }
        paintCross.apply { color = cross; style = Paint.Style.STROKE; strokeWidth = crossStroke }
        paintCenter.apply { color = center; style = Paint.Style.FILL }
        paintPeer.apply { style = Paint.Style.FILL }
        paintPeerText.apply {
            color = text; textSize = peerTextSize; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD; setShadowLayer(shadowR, 0f, 0f, Color.BLACK)
        }
        paintPeerDist.apply {
            color = peerDistText; textSize = peerDistSize; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD; setShadowLayer(shadowR, 0f, 0f, Color.BLACK)
        }
        paintNorth.apply {
            color = north; textSize = northSize; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD; setShadowLayer(shadowR, 0f, 0f, Color.BLACK)
        }
        paintCardinal.apply {
            color = cardinal; textSize = cardinalSize; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD; setShadowLayer(shadowR, 0f, 0f, Color.BLACK)
        }
        paintRange.apply {
            color = rangeText; textSize = 13f * dp; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD; setShadowLayer(2f * dp, 0f, 0f, Color.BLACK)
        }
        paintButton.apply { color = btnBg; style = Paint.Style.FILL }
        paintButtonText.apply {
            color = btnTextC; textSize = 20f * dp; textAlign = Paint.Align.CENTER
            typeface = Typeface.DEFAULT_BOLD
        }
        paintButtonStroke.apply { color = btnStroke; style = Paint.Style.STROKE; strokeWidth = 2f * dp }
        paintPeerRing.apply { style = Paint.Style.STROKE; strokeWidth = 2f * dp }
    }

    fun toggleContrast() {
        highContrast = !highContrast
        applyColors()
        invalidate()
    }

    fun startCompass() {
        val sensor = sensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR)
        if (sensor != null) {
            sensorManager.registerListener(this, sensor, SensorManager.SENSOR_DELAY_UI)
        }
    }

    fun stopCompass() {
        sensorManager.unregisterListener(this)
    }

    override fun onSensorChanged(event: SensorEvent) {
        if (event.sensor.type == Sensor.TYPE_ROTATION_VECTOR) {
            val rotationMatrix = FloatArray(9)
            SensorManager.getRotationMatrixFromVector(rotationMatrix, event.values)
            val orientation = FloatArray(3)
            SensorManager.getOrientation(rotationMatrix, orientation)
            azimuth = Math.toDegrees(orientation[0].toDouble()).toFloat()
            invalidate()
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    fun zoomIn() {
        autoScale = false
        if (rangeIndex > 0) rangeIndex--
        invalidate()
    }

    fun zoomOut() {
        autoScale = false
        if (rangeIndex < ranges.size - 1) rangeIndex++
        invalidate()
    }

    fun resetAutoScale() {
        autoScale = true
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        canvas.drawColor(if (highContrast) HiColors.bg else NormalColors.bg)

        val cx = width / 2f
        val cy = height / 2f
        val radius = min(cx, cy) * 0.85f

        val myLat = ServiceState.myLat.value
        val myLon = ServiceState.myLon.value

        // Вычислить peers
        val peerPoints = mutableListOf<PeerPoint>()
        val now = System.currentTimeMillis()
        ServiceState.peers.value?.forEach { peer ->
            if (myLat != null && myLon != null && peer.lat != null && peer.lon != null) {
                val dist = distanceKm(myLat, myLon, peer.lat, peer.lon)
                val bearing = bearingDeg(myLat, myLon, peer.lat, peer.lon)
                val age = (now - peer.lastSeenMs) / 1000
                peerPoints.add(PeerPoint(peer.callSign, dist, bearing, peer.rssi, age))
            }
        }

        // Автомасштаб по дальней станции с ненулевым RSSI
        if (autoScale) {
            val maxDist = peerPoints
                .filter { it.rssi != 0 }
                .maxOfOrNull { it.distKm }
            rangeIndex = if (maxDist != null) {
                pickRangeIndex(maxDist)
            } else {
                5 // 5 км по умолчанию
            }
        }

        val rangeKm = ranges[rangeIndex]

        // Перекрестие
        canvas.drawLine(cx, cy - radius, cx, cy + radius, paintCross)
        canvas.drawLine(cx - radius, cy, cx + radius, cy, paintCross)

        // Вращение по компасу (север сверху)
        canvas.save()
        canvas.rotate(-azimuth, cx, cy)

        // Кольца дистанции (4 кольца)
        for (i in 1..4) {
            val ringKm = rangeKm * i / 4.0
            val r = (ringKm / rangeKm * radius).toFloat()
            canvas.drawCircle(cx, cy, r, paintGrid)
            if (i < 4) {
                val label = formatDist(ringKm)
                paintGridText.textAlign = Paint.Align.LEFT
                canvas.drawText(label, cx + 5 * dp, cy - r + 15 * dp, paintGridText)
            }
        }

        // Расстояние у внешнего кольца
        val outerLabel = formatDist(rangeKm)
        paintGridText.textAlign = Paint.Align.LEFT
        canvas.drawText(outerLabel, cx + 5 * dp, cy - radius + 15 * dp, paintGridText)

        // Метки сторон света
        val cardinalOffset = 8f * dp
        canvas.drawText(resources.getString(R.string.north), cx, cy - radius - cardinalOffset, paintNorth)
        paintCardinal.textAlign = Paint.Align.CENTER
        canvas.drawText(resources.getString(R.string.south), cx, cy + radius + cardinalOffset + paintCardinal.textSize, paintCardinal)
        paintCardinal.textAlign = Paint.Align.RIGHT
        canvas.drawText(resources.getString(R.string.west), cx - radius - cardinalOffset, cy + paintCardinal.textSize / 3, paintCardinal)
        paintCardinal.textAlign = Paint.Align.LEFT
        canvas.drawText(resources.getString(R.string.east), cx + radius + cardinalOffset, cy + paintCardinal.textSize / 3, paintCardinal)

        // Peers
        for (pp in peerPoints) {
            val r = (pp.distKm / rangeKm * radius).toFloat().coerceAtMost(radius)
            val angleRad = Math.toRadians(pp.bearingDeg - 90)
            val px = cx + r * cos(angleRad).toFloat()
            val py = cy + r * sin(angleRad).toFloat()

            // Цвет по давности
            val baseColor = when {
                pp.ageSec < 300 -> if (highContrast) HiColors.peer else NormalColors.peer
                pp.ageSec < 600 -> if (highContrast) HiColors.peerYellow else NormalColors.peerYellow
                else -> if (highContrast) HiColors.peerRed else NormalColors.peerRed
            }

            // Яркость по RSSI: сильный сигнал = ярче, слабый = тусклее
            val rssiAlpha = ((pp.rssi + 120).coerceIn(10, 100) * 255 / 100).coerceIn(80, 255)
            paintPeer.color = baseColor
            paintPeer.alpha = rssiAlpha

            // Размер точки
            val dotRadius = 4f * dp
            canvas.drawCircle(px, py, dotRadius, paintPeer)

            // Позывной на точке
            paintPeerText.alpha = rssiAlpha
            canvas.drawText(pp.callSign, px, py - dotRadius - 5 * dp, paintPeerText)

            // Расстояние + RSSI под точкой
            val distLabel = formatDist(pp.distKm)
            val infoLabel = "$distLabel ${pp.rssi}dBm"
            paintPeerDist.alpha = rssiAlpha
            canvas.drawText(infoLabel, px, py + dotRadius + 14 * dp, paintPeerDist)
        }

        canvas.restore()

        // Центр — своя позиция с пульсацией
        pulsePhase = (pulsePhase + 0.05f) % (2f * PI.toFloat())
        val pulseR = 4f * dp + 3f * dp * sin(pulsePhase)
        val centerColor = if (highContrast) HiColors.center else NormalColors.center
        paintCenter.color = centerColor
        paintCenter.alpha = 255
        canvas.drawCircle(cx, cy, pulseR, paintCenter)
        paintCenter.alpha = 50
        canvas.drawCircle(cx, cy, pulseR * 2.5f, paintCenter)
        paintCenter.alpha = 255

        // --- Кнопки (не вращаются) ---
        val btnSize = 40f * dp
        val btnMargin = 12f * dp
        val btnRight = width - btnMargin

        // Кнопки +/− внизу справа
        val btnBottomMinus = height - btnMargin
        btnMinusRect.set(btnRight - btnSize, btnBottomMinus - btnSize, btnRight, btnBottomMinus)
        canvas.drawRoundRect(btnMinusRect, 8f * dp, 8f * dp, paintButton)
        canvas.drawRoundRect(btnMinusRect, 8f * dp, 8f * dp, paintButtonStroke)
        canvas.drawText("−", btnMinusRect.centerX(), btnMinusRect.centerY() + 7f * dp, paintButtonText)

        val btnBottomPlus = btnMinusRect.top - 8f * dp
        btnPlusRect.set(btnRight - btnSize, btnBottomPlus - btnSize, btnRight, btnBottomPlus)
        canvas.drawRoundRect(btnPlusRect, 8f * dp, 8f * dp, paintButton)
        canvas.drawRoundRect(btnPlusRect, 8f * dp, 8f * dp, paintButtonStroke)
        canvas.drawText("+", btnPlusRect.centerX(), btnPlusRect.centerY() + 7f * dp, paintButtonText)

        // Кнопка контраста (левый нижний угол)
        val cBtnSize = 40f * dp
        val cBtnLeft = btnMargin
        val cBtnTop = height - btnMargin - cBtnSize
        btnContrastRect.set(cBtnLeft, cBtnTop, cBtnLeft + cBtnSize, cBtnTop + cBtnSize)
        canvas.drawRoundRect(btnContrastRect, 8f * dp, 8f * dp, paintButton)
        canvas.drawRoundRect(btnContrastRect, 8f * dp, 8f * dp, paintButtonStroke)
        // Иконка: солнце (☀) для высокого контраста, луна (☾) для обычного
        val contrastIcon = if (highContrast) "☀" else "☾"
        paintButtonText.textSize = 22f * dp
        canvas.drawText(contrastIcon, btnContrastRect.centerX(), btnContrastRect.centerY() + 8f * dp, paintButtonText)
        paintButtonText.textSize = 20f * dp

        // Обновлять для анимации
        postInvalidateDelayed(50)
    }

    private fun formatDist(km: Double): String {
        return if (km < 1) "${(km * 1000).toInt()}м" else "%.1fкм".format(km)
    }

    private fun bearingDeg(lat1: Double, lon1: Double, lat2: Double, lon2: Double): Double {
        val dLon = Math.toRadians(lon2 - lon1)
        val la1 = Math.toRadians(lat1)
        val la2 = Math.toRadians(lat2)
        val y = sin(dLon) * cos(la2)
        val x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dLon)
        return (Math.toDegrees(atan2(y, x)) + 360) % 360
    }

    private fun pickRangeIndex(maxDistKm: Double): Int {
        for (i in ranges.indices) {
            if (maxDistKm <= ranges[i] * 0.85) return i
        }
        return ranges.size - 1
    }
}
