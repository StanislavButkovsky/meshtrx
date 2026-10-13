import Foundation
import Combine

@MainActor
class AppState: ObservableObject {
    // BLE
    @Published var bleState: BleState = .disconnected
    @Published var deviceName: String = ""
    @Published var callSign: String = ""
    @Published var currentChannel: Int = 0
    @Published var rssi: Int = 0
    @Published var snr: Int = 0
    @Published var batteryVoltage: Float = 0

    // PTT / Audio
    @Published var isPttActive: Bool = false
    // Последнее, что сказала рация про отправку файла (для строки состояния)
    @Published var fileStatusMessage: String? = nil
    // Сколько секунд передачи осталось (nil — не передаём). Предел 10 с, как в
    // Android и прошивке: в полудуплексе говорящий занимает канал целиком.
    @Published var pttSecondsLeft: Int? = nil
    // Идёт приём: кто говорит. Гаснет с последним пакетом фразы или через 5 с тишины.
    @Published var rxCallSign: String? = nil
    // Адресат голоса (последние 4 знака id) — как «@» в Android. nil — общий канал.
    @Published var pttTargetId: String? = nil
    @Published var pttTargetName: String? = nil
    // Судьба адресной передачи: «отправка...», «доставлено», «не доставлено»
    @Published var pttDeliveryText: String? = nil
    @Published var txMode: TxMode = .ptt
    @Published var listenMode: ListenMode = .all
    @Published var voxState: VoxState = .idle
    @Published var rmsLevel: Int = 0

    // Messages
    @Published var messages: [ChatMessage] = []
    @Published var unreadMessages: Int = 0
    @Published var messageFilter: String? = nil

    // Peers
    @Published var peers: [Peer] = []
    @Published var scanResults: [ScanDevice] = []

    // Files
    @Published var fileTransfers: [FileTransfer] = []

    // Calls
    @Published var incomingCall: IncomingCall? = nil
    @Published var callActive: Bool = false
    @Published var recentCalls: [RecentCall] = []

    // Location
    @Published var myLat: Double? = nil
    @Published var myLon: Double? = nil

    // Settings
    @Published var peerTimeoutMin: Int = 60
    @Published var fileHistoryDays: Int = 30
    @Published var messageHistoryDays: Int = 30
    @Published var rxVolume: Int = 200
    @Published var rogerBeep: Bool = true
    // Звук и VOX — телефонные настройки: применяются сразу и хранятся, как в
    // Android. Раньше жили в экране настроек и уходили только по «Применить»,
    // которая без связи с рацией выключена.
    @Published var squelchThreshold: Int = 0
    @Published var voxThreshold: Int = 800
    @Published var voxHangtimeMs: Int = 800
    @Published var languageIndex: Int = 0  // 0=Русский, 1=English

    // Encryption
    @Published var keyFingerprint: String = ""
    @Published var hearPlaintext: Bool = false
    @Published var alienPackets: Int = 0

    // Firmware
    @Published var firmwareVersion: String = ""

    // Notifications
    // По умолчанию «обо всех», как в Android (было «только личные»)
    @Published var notifyMode: Int = 2  // 0=off, 1=private only, 2=all

    // UI
    @Published var statusMessage: String = "Не подключено"
    @Published var showPinDialog: Bool = false
    @Published var showDevicePicker: Bool = false
    @Published var showClearChatConfirm: Bool = false
    @Published var scanResultText: String = ""
    @Published var scanBestChannel: Int? = nil   // самый тихий канал по последнему поиску
    @Published var updateStatus: String = ""
    @Published var updateAvailable: Bool = false
}


// MARK: - Кому можно написать или отправить лично

extension AppState {
    /// Рации, слышные в эфире, и те, от кого уже приходили сообщения. Адрес —
    /// последние четыре знака идентификатора, как в Android.
    var directRecipients: [(id: String, name: String)] {
        var out: [(id: String, name: String)] = []
        var seen = Set<String>()
        for p in peers.sorted(by: { $0.rssi > $1.rssi }) {
            let id = String(p.deviceId.suffix(4))
            guard id.count == 4, !seen.contains(id) else { continue }
            seen.insert(id)
            out.append((id, p.callSign.isEmpty ? "TX-\(id)" : p.callSign))
        }
        for m in messages where !m.isOutgoing && m.senderId.count == 4 && m.senderId != "??" {
            guard !seen.contains(m.senderId) else { continue }
            seen.insert(m.senderId)
            out.append((m.senderId, m.senderName.isEmpty ? "TX-\(m.senderId)" : m.senderName))
        }
        return out
    }
}
