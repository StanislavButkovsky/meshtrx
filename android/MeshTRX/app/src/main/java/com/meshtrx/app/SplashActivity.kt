package com.meshtrx.app

import android.content.Intent
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.SpannableString
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import android.view.animation.AlphaAnimation
import android.view.animation.DecelerateInterpolator
import android.widget.ImageView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.meshtrx.app.ui.Colors

class SplashActivity : AppCompatActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        Colors.apply(this)
        val khaki = Colors.isKhaki(this)
        if (khaki) setTheme(R.style.Theme_MeshTRX_Splash_Khaki)
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_splash)

        val logo = findViewById<ImageView>(R.id.splashLogo)
        if (khaki) {
            findViewById<android.view.View>(R.id.splashRoot).setBackgroundColor(0xFF24281C.toInt())
            logo.setImageResource(R.drawable.splash_mark_khaki)
        }
        // «Mesh» — цветом текста, «TRX» — фирменным жёлтым (в «Хаки» — охрой)
        val title = SpannableString("MeshTRX")
        title.setSpan(ForegroundColorSpan(Colors.accent), 4, 7, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
        findViewById<TextView>(R.id.splashTitle).text = title
        val center = findViewById<android.view.View>(R.id.splashCenter)
        val version = findViewById<TextView>(R.id.splashVersion)
        // Номер берём из самого приложения: в разметке стояло «v4.3.1», и
        // заставка показывала его при любой версии.
        version.text = "v" + packageManager.getPackageInfo(packageName, 0).versionName

        // Fade in через animate()
        center.alpha = 0f
        center.scaleX = 0.92f; center.scaleY = 0.92f
        center.animate().alpha(1f).scaleX(1f).scaleY(1f)
            .setInterpolator(DecelerateInterpolator()).setDuration(700).start()

        version.alpha = 0f
        version.animate().alpha(1f).setStartDelay(400).setDuration(600).start()

        // Переход на главный экран
        Handler(Looper.getMainLooper()).postDelayed({
            startActivity(Intent(this, MainActivity::class.java))
            finish()
            overridePendingTransition(android.R.anim.fade_in, android.R.anim.fade_out)
        }, 2000)
    }
}
