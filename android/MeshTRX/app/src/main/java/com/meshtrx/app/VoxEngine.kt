package com.meshtrx.app

import android.util.Log

/**
 * VOX (Voice Operated Exchange) — автоматическое управление TX по голосу.
 * State machine: IDLE → ATTACK → ACTIVE → HANGTIME → IDLE
 */
class VoxEngine {
    companion object {
        private const val TAG = "VoxEngine"
        const val DEFAULT_THRESHOLD = 800   // 0-32767
        const val DEFAULT_HANGTIME_MS = 800L
        const val DEFAULT_ATTACK_MS = 50L
    }

    enum class State { IDLE, ATTACK, ACTIVE, HANGTIME }

    var threshold: Int = DEFAULT_THRESHOLD
    var hangtimeMs: Long = DEFAULT_HANGTIME_MS
    var attackMs: Long = DEFAULT_ATTACK_MS

    var onTxActivated: (() -> Unit)? = null
    var onTxDeactivated: (() -> Unit)? = null
    var onStateChanged: ((State) -> Unit)? = null

    private var state = State.IDLE
    private var timerStart = 0L

    val currentState: State get() = state
    val isActive: Boolean get() = state == State.ACTIVE || state == State.HANGTIME

    fun process(rms: Int) {
        val now = System.currentTimeMillis()
        val prevActive = isActive

        when (state) {
            State.IDLE -> {
                if (rms > threshold) {
                    timerStart = now
                    state = State.ATTACK
                    onStateChanged?.invoke(state)
                }
            }
            State.ATTACK -> {
                if (rms < threshold) {
                    // Щелчок / помеха — вернуться
                    state = State.IDLE
                    onStateChanged?.invoke(state)
                } else if (now - timerStart >= attackMs) {
                    state = State.ACTIVE
                    onStateChanged?.invoke(state)
                    Log.d(TAG, "TX activated (RMS=$rms, threshold=$threshold)")
                }
            }
            State.ACTIVE -> {
                if (rms < threshold) {
                    timerStart = now
                    state = State.HANGTIME
                    onStateChanged?.invoke(state)
                }
            }
            State.HANGTIME -> {
                if (rms > threshold) {
                    // Продолжение речи
                    state = State.ACTIVE
                    onStateChanged?.invoke(state)
                } else if (now - timerStart >= hangtimeMs) {
                    state = State.IDLE
                    onStateChanged?.invoke(state)
                    Log.d(TAG, "TX deactivated (hangtime expired)")
                }
            }
        }

        // Callbacks на переходах
        if (!prevActive && isActive) {
            onTxActivated?.invoke()
        } else if (prevActive && !isActive) {
            onTxDeactivated?.invoke()
        }
    }

    fun reset() {
        state = State.IDLE
        timerStart = 0
    }
}
