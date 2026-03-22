package com.meshtrx.app.ui

import android.graphics.Color

object Colors {
    val bgPrimary     = Color.parseColor("#141414")
    val bgSurface     = Color.parseColor("#1a1a1a")
    val bgElevated    = Color.parseColor("#222222")
    val bgBorder      = Color.parseColor("#2a2a2a")
    val borderSubtle  = Color.parseColor("#333333")
    val borderMuted   = Color.parseColor("#252525")

    val greenAccent   = Color.parseColor("#4ade80")
    val greenBg       = Color.parseColor("#1a3a1a")
    val greenBorder   = Color.parseColor("#2a5a2a")
    val greenDim      = Color.parseColor("#2d5a2d")

    val blueAccent    = Color.parseColor("#5ba3e8")
    val blueBg        = Color.parseColor("#1e3a5f")
    val blueBorder    = Color.parseColor("#2a5a8f")

    val redAccent     = Color.parseColor("#f87171")
    val redTx         = Color.parseColor("#ff6b6b")
    val redBg         = Color.parseColor("#3a0e0e")
    val redBorder     = Color.parseColor("#6b1a1a")
    val redTxBg       = Color.parseColor("#4a0a0a")
    val redTxBorder   = Color.parseColor("#aa2a2a")

    val amberAccent   = Color.parseColor("#f59e0b")
    val amberBg       = Color.parseColor("#2a1f0a")

    val textPrimary   = Color.parseColor("#e8e8e8")
    val textSecondary = Color.parseColor("#cccccc")
    val textMuted     = Color.parseColor("#888888")
    val textDim       = Color.parseColor("#555555")
    val textLabel     = Color.parseColor("#444444")

    val navBg         = Color.parseColor("#111111")
    val navBorder     = Color.parseColor("#222222")

    fun rssiColor(rssi: Int): Int = when {
        rssi >= -60 -> greenAccent
        rssi >= -90 -> amberAccent
        else -> redAccent
    }

    fun rssiBg(rssi: Int): Int = when {
        rssi >= -60 -> greenBg
        rssi >= -90 -> amberBg
        else -> redBg
    }
}
