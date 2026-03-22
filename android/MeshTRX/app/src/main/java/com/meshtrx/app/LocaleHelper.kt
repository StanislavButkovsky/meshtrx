package com.meshtrx.app

import android.content.Context
import android.content.res.Configuration
import java.util.Locale

object LocaleHelper {

    private const val PREF_NAME = "meshtrx_locale"
    private const val KEY_LANG = "language"

    fun getSavedLanguage(context: Context): String {
        return context.getSharedPreferences(PREF_NAME, 0).getString(KEY_LANG, "ru") ?: "ru"
    }

    fun setLanguage(context: Context, lang: String) {
        context.getSharedPreferences(PREF_NAME, 0).edit().putString(KEY_LANG, lang).apply()
    }

    fun applyLocale(context: Context): Context {
        val lang = getSavedLanguage(context)
        val locale = Locale(lang)
        Locale.setDefault(locale)
        val config = Configuration(context.resources.configuration)
        config.setLocale(locale)
        return context.createConfigurationContext(config)
    }
}
