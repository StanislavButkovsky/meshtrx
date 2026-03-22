package com.meshtrx.app.ui

import android.animation.ValueAnimator
import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import android.view.animation.LinearInterpolator
import com.meshtrx.app.R

/**
 * Кастомная PTT кнопка с анимацией расходящихся кругов при TX/RX.
 * Состояния: IDLE (зелёная), TX (красная + круги), RX (синяя + пульс), VOX (зелёная с надписью VOX)
 */
class PttButtonView @JvmOverloads constructor(
    ctx: Context, attrs: AttributeSet? = null
) : View(ctx, attrs) {

    enum class State { IDLE, TX, RX, VOX_IDLE, VOX_TX }

    var state: State = State.IDLE
        set(value) { field = value; invalidate() }

    var rmsLevel: Float = 0f  // 0.0 - 1.0, для анимации кругов
        set(value) { field = value.coerceIn(0f, 1f); invalidate() }

    var label: String = "ГОВОРИТЬ"
    var subLabel: String = "удержать"

    private val paintRing = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 3f }
    private val paintInner = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val paintInnerBorder = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 2f }
    private val paintText = Paint(Paint.ANTI_ALIAS_FLAG).apply { textAlign = Paint.Align.CENTER }
    private val paintWave = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; strokeWidth = 2f }

    // Волны (расходящиеся круги)
    private data class Wave(var radius: Float, var alpha: Int, var startTime: Long)
    private val waves = mutableListOf<Wave>()
    private var lastWaveTime = 0L

    // Пульс-аниматор для непрерывной анимации
    private val animator = ValueAnimator.ofFloat(0f, 1f).apply {
        duration = 16 // ~60fps
        repeatCount = ValueAnimator.INFINITE
        interpolator = LinearInterpolator()
        addUpdateListener { updateWaves(); invalidate() }
    }

    init {
        animator.start()
    }

    private fun updateWaves() {
        val now = System.currentTimeMillis()

        // Добавлять новые волны при TX/VOX_TX на основе RMS
        if ((state == State.TX || state == State.VOX_TX) && rmsLevel > 0.05f) {
            val interval = (300 - (rmsLevel * 200)).toLong().coerceAtLeast(80)
            if (now - lastWaveTime > interval) {
                waves.add(Wave(0f, (rmsLevel * 200).toInt().coerceIn(40, 200), now))
                lastWaveTime = now
            }
        }

        // Обновить волны
        val maxRadius = width / 2f * 1.5f
        val iterator = waves.iterator()
        while (iterator.hasNext()) {
            val w = iterator.next()
            val age = now - w.startTime
            val progress = age / 1500f // 1.5 сек жизнь волны
            w.radius = progress * maxRadius
            w.alpha = ((1f - progress) * w.alpha).toInt().coerceAtLeast(0)
            if (progress > 1f) iterator.remove()
        }
    }

    override fun onDraw(canvas: Canvas) {
        val cx = width / 2f
        val cy = height / 2f
        val outerR = minOf(cx, cy) - 4f
        val innerR = outerR - 8f

        // Цвета по состоянию
        val (bgColor, borderColor, ringColor, textColor, mainLabel, subLbl, waveColor) = when (state) {
            State.IDLE -> Scheme(Colors.greenBg, Colors.greenBorder, Colors.bgBorder, Colors.greenAccent, resources.getString(R.string.ptt_talk), resources.getString(R.string.ptt_hold), 0)
            State.TX -> Scheme(Colors.redTxBg, Colors.redTxBorder, Colors.redBorder, Colors.redTx, resources.getString(R.string.ptt_transmit), resources.getString(R.string.ptt_release), Colors.redTx)
            State.RX -> Scheme(Color.parseColor("#0a2a4a"), Colors.blueBorder, Color.parseColor("#1a3a6b"), Colors.blueAccent, resources.getString(R.string.ptt_receive), "", Colors.blueAccent)
            State.VOX_IDLE -> Scheme(Colors.greenBg, Colors.greenBorder, Colors.bgBorder, Colors.greenAccent, "VOX", resources.getString(R.string.ptt_vox_auto), 0)
            State.VOX_TX -> Scheme(Colors.redTxBg, Colors.redTxBorder, Colors.redBorder, Colors.redTx, "VOX TX", "", Colors.redTx)
        }

        // Волны (за кнопкой)
        if (waveColor != 0) {
            for (w in waves) {
                paintWave.color = waveColor
                paintWave.alpha = w.alpha
                canvas.drawCircle(cx, cy, outerR + w.radius, paintWave)
            }
        }

        // Внешнее кольцо
        paintRing.color = ringColor
        canvas.drawCircle(cx, cy, outerR, paintRing)

        // Внутренний круг
        paintInner.color = bgColor
        canvas.drawCircle(cx, cy, innerR, paintInner)
        paintInnerBorder.color = borderColor
        canvas.drawCircle(cx, cy, innerR, paintInnerBorder)

        // Текст
        paintText.color = textColor
        paintText.textSize = 17f * resources.displayMetrics.density
        paintText.typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        canvas.drawText(mainLabel, cx, cy + 6f, paintText)

        if (subLbl.isNotEmpty()) {
            paintText.textSize = 11f * resources.displayMetrics.density
            paintText.typeface = Typeface.DEFAULT
            val subColor = when (state) {
                State.IDLE, State.VOX_IDLE -> Colors.greenDim
                State.TX, State.VOX_TX -> Colors.redBorder
                State.RX -> Color.parseColor("#1a3a6b")
            }
            paintText.color = subColor
            canvas.drawText(subLbl, cx, cy + 28f * resources.displayMetrics.density / 2f, paintText)
        }
    }

    override fun onDetachedFromWindow() {
        animator.cancel()
        super.onDetachedFromWindow()
    }

    private data class Scheme(
        val bg: Int, val border: Int, val ring: Int,
        val text: Int, val label: String, val sub: String,
        val wave: Int
    )
}
