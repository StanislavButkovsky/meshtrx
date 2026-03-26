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

    // UI
    @Published var statusMessage: String = "Не подключено"
    @Published var showPinDialog: Bool = false
    @Published var showDevicePicker: Bool = false
}
