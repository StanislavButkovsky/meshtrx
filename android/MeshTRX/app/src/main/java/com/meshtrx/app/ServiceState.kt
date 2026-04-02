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

    // Events (one-shot) — UI подписывается
    val showPinDialog = MutableLiveData(false)
    val showDevicePicker = MutableLiveData(false)
}
