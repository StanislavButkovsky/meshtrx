package com.meshtrx.app

import androidx.lifecycle.MutableLiveData
import com.meshtrx.app.model.*

/**
 * Singleton — единый источник состояния.
 * Service пишет, Fragments читают через observe.
 */
object ServiceState {
    val connectionState = MutableLiveData(BleState.DISCONNECTED)
    val currentChannel = MutableLiveData(0)
    val rssi = MutableLiveData(0)
    val snr = MutableLiveData(0)
    val isPttActive = MutableLiveData(false)
    // Сколько секунд осталось говорить; null — передача не идёт
    val pttSecondsLeft = MutableLiveData<Int?>(null)
    val txMode = MutableLiveData(TxMode.PTT)
    val listenMode = MutableLiveData(ListenMode.ALL)
    val voxState = MutableLiveData(VoxEngine.State.IDLE)
    val messages = MutableLiveData<List<ChatMessage>>(emptyList())
    val peers = MutableLiveData<List<Peer>>(emptyList())
    val scanResults = MutableLiveData<List<ScanDevice>>(emptyList())
    val deviceName = MutableLiveData("")
    val callSign = MutableLiveData("")  // позывной (из настроек или из beacon)
    val rmsLevel = MutableLiveData(0)
    val statusMessage = MutableLiveData("Не подключено")
    val batteryVoltage = MutableLiveData(0f)
    val myLat = MutableLiveData<Double?>(null)
    val myLon = MutableLiveData<Double?>(null)

    val peerTimeoutMin = MutableLiveData(60)
    val fileHistoryDays = MutableLiveData(30) // дней хранения истории файлов
    val messageHistoryDays = MutableLiveData(30) // дней хранения истории сообщений
    val rxVolume = MutableLiveData(200) // громкость приёма 0-300 (100=норма, 200=x2, 300=x3)
    val messageFilter = MutableLiveData<String?>(null) // null=все, senderId=фильтр
    // Смотрит ли человек в приложение прямо сейчас. По нему служба решает,
    // показывать ли уведомление о входящем: сообщать о том, что и так на
    // экране, — шум. Выставляется в MainActivity по onResume/onPause.
    val appVisible = MutableLiveData(false)
    val recentCalls = MutableLiveData<List<RecentCall>>(emptyList())

    val fileTransfers = MutableLiveData<List<FileTransfer>>(emptyList())
    val incomingCall = MutableLiveData<IncomingCall?>(null)
    val callActive = MutableLiveData(false)

    // Последний принятый абонент (PTT экран)
    val lastRxCallSign = MutableLiveData("")
    val lastRxDeviceId = MutableLiveData("")
    val lastRxRssi = MutableLiveData(0)
    val lastRxSnr = MutableLiveData(0)
    val isReceiving = MutableLiveData(false)
    val isPlayingVoice = MutableLiveData(false) // воспроизведение адресного голосового
    val isReceivingFile = MutableLiveData(false) // приём файла (адресный PTT и др.)

    // Уровень шума по каналам после сканирования: пары «канал → дБм».
    // Пусто, пока человек не попросил послушать эфир.
    val channelNoise = MutableLiveData<List<Int>>(emptyList())
    val channelBest = MutableLiveData(-1)
    // Отпечаток ключа — четыре знака, по которым сверяют, что у всех он один.
    // Пустая строка означает, что эфир открыт.
    val keyFingerprint = MutableLiveData("")
    // Версия прошивки рации. Пустая строка означает «рация не сказала»: так
    // отвечают устройства до 4.4.22, где версии в прошивке не было вовсе.
    val firmwareVersion = MutableLiveData("")
    // Сколько пакетов рядом рация не смогла разобрать своим ключом. Ноль —
    // либо все свои, либо рядом тихо; растёт — значит ключи разошлись.
    val alienPackets = MutableLiveData(0)
    // Слушает ли рация открытый эфир при заданном ключе
    val hearPlaintext = MutableLiveData(false)
    // Сетка частот рации: прошивка сообщает её после версии (с 4.4.34).
    // Пока не сообщила — считаем 868, как у всех прошивок до этого.
    val band = MutableLiveData(Band.EU868)
    // Настройки, которые сейчас стоят в рации (ответ на запрос настроек).
    // null — рация ещё не ответила.
    val deviceTxPower = MutableLiveData<Int?>(null)
    val deviceDutyCycle = MutableLiveData<Boolean?>(null)
    val deviceBeaconSec = MutableLiveData<Int?>(null)

    // Events (one-shot) — UI подписывается
    val showPinDialog = MutableLiveData(false)
    val showDevicePicker = MutableLiveData(false)
}

/** Сетка каналов рации: частота канала n = base + n*step. */
data class Band(val baseKhz: Int, val stepKhz: Int, val count: Int,
                val maxDbm: Int, val minDbm: Int) {
    fun freqMhz(ch: Int): Double = (baseKhz + ch.toLong() * stepKhz) / 1000.0
    val is433: Boolean get() = baseKhz < 500_000
    // У 868 сетка кратна 10 кГц — хватает двух знаков; у 433 (433,225) нужно три.
    fun mhz(ch: Int): String = (if (is433) "%.3f" else "%.2f").format(freqMhz(ch))
    fun label(ch: Int): String = "CH $ch — ${mhz(ch)} MHz"

    companion object {
        // Предел 28 — как у V4 с усилителем: прошивки до 4.4.34 сетку не
        // сообщают, и ползунок не должен отрезать им мощность. Лишнее рация
        // V3 обрежет сама до своих 22 дБм.
        val EU868 = Band(863_150, 300, 23, 28, -9)
    }
}
