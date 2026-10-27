package com.meshtrx.app.ui

import android.content.Context
import android.graphics.Color

/**
 * Палитра приложения. Две темы — «Основная» (сине-жёлтая) и «Хаки» — с одними и
 * теми же ролями цветов; разметка берёт их через ?attr/mtx_*, код — отсюда.
 * Тему выбирают в настройках, применяется она при создании окна (apply()).
 */
object Colors {
    const val PREFS = "meshtrx_ui"
    const val KEY_THEME = "ui_theme"
    const val THEME_MAIN = "main"
    const val THEME_KHAKI = "khaki"

    var bg = 0; private set
    var bgDeep = 0; private set
    var surface = 0; private set
    var elevated = 0; private set
    var border = 0; private set
    var borderStrong = 0; private set
    var accent = 0; private set
    var onAccent = 0; private set
    var secondary = 0; private set
    var secondaryBg = 0; private set
    var secondaryBorder = 0; private set
    var text = 0; private set
    var text2 = 0; private set
    var textMuted = 0; private set
    var textDim = 0; private set
    var textFaint = 0; private set

    private fun c(hex: String) = Color.parseColor(hex)

    fun themeOf(ctx: Context): String =
        ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getString(KEY_THEME, THEME_MAIN) ?: THEME_MAIN

    fun isKhaki(ctx: Context) = themeOf(ctx) == THEME_KHAKI

    /** Стиль окна для выбранной темы — вызывать до setContentView. */
    fun themeRes(ctx: Context): Int =
        if (isKhaki(ctx)) com.meshtrx.app.R.style.Theme_MeshTRX_Khaki else com.meshtrx.app.R.style.Theme_MeshTRX

    fun apply(ctx: Context) = if (isKhaki(ctx)) khaki() else main()

    private fun main() {
        bg = c("#0E1A33")
        bgDeep = c("#0B152B")
        surface = c("#14233F")
        elevated = c("#1B2C4E")
        border = c("#22355A")
        borderStrong = c("#2A3F66")
        accent = c("#FABE14")
        onAccent = c("#101E3C")
        secondary = c("#5B9BFF")
        secondaryBg = c("#1D3566")
        secondaryBorder = c("#2F55A0")
        text = c("#EEF2FA")
        text2 = c("#C7D0E2")
        textMuted = c("#8E9AB3")
        textDim = c("#5C6A88")
        textFaint = c("#7F8FB0")
    }

    private fun khaki() {
        bg = c("#1F2318")
        bgDeep = c("#191C13")
        surface = c("#2A2F20")
        elevated = c("#343A28")
        border = c("#3E4530")
        borderStrong = c("#4A523A")
        accent = c("#CE9C3A")
        onAccent = c("#1F2318")
        secondary = c("#A3B56A")
        secondaryBg = c("#3A4425")
        secondaryBorder = c("#5C6B38")
        text = c("#EAE2C8")
        text2 = c("#D2CAB0")
        textMuted = c("#A39C82")
        textDim = c("#6F6A57")
        textFaint = c("#8A846C")
    }

    init { main() }

    // Прежние имена — чтобы не переписывать все экраны разом
    val bgPrimary get() = bg
    val bgSurface get() = surface
    val bgElevated get() = elevated
    val bgBorder get() = border
    val borderSubtle get() = borderStrong
    val borderMuted get() = border
    val greenAccent get() = accent          // главный акцент
    val greenBg get() = secondaryBg
    val greenBorder get() = secondaryBorder
    val greenDim get() = textFaint
    val blueAccent get() = secondary
    val blueBg get() = secondaryBg
    val blueBorder get() = secondaryBorder
    val textPrimary get() = text
    val textSecondary get() = text2
    val textLabel get() = textFaint
    val navBg get() = bgDeep
    val navBorder get() = elevated

    val okGreen       = Color.parseColor("#5CCB8A")   // хороший сигнал
    val redAccent     = Color.parseColor("#f87171")
    val redTx         = Color.parseColor("#ff6b6b")
    val redBg         = Color.parseColor("#3a0e0e")
    val redBorder     = Color.parseColor("#6b1a1a")
    val redTxBg       = Color.parseColor("#4a0a0a")
    val redTxBorder   = Color.parseColor("#aa2a2a")
    val amberAccent   = Color.parseColor("#f59e0b")
    val amberBg       = Color.parseColor("#2a1f0a")

    fun rssiColor(rssi: Int): Int = when {
        rssi >= -60 -> okGreen
        rssi >= -90 -> amberAccent
        else -> redAccent
    }

    fun rssiBg(rssi: Int): Int = when {
        rssi >= -60 -> secondaryBg
        rssi >= -90 -> amberBg
        else -> redBg
    }
}
