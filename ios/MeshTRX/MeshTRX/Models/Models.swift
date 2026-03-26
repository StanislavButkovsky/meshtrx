import Foundation
import CoreBluetooth

// MARK: - Enums

enum BleState: String {
    case disconnected, scanning, connecting, connected
}

enum TxMode: String {
    case ptt, vox
}

enum ListenMode: String {
    case all, privateOnly
}

enum MessageStatus: String {
    case sending, sent, delivered, failed
}

enum FileStatus: String {
    case pending, transferring, done, failed
}

enum CallType: Int {
    case all = 0, `private` = 1, group = 2, emergency = 3

    static func fromCode(_ code: Int) -> CallType {
        CallType(rawValue: code) ?? .all
    }
}

enum VoxState: String {
    case idle, attack, active, hangtime
}

// MARK: - Data Models

struct ChatMessage: Identifiable {
    let id: Int64
    let text: String
    let isOutgoing: Bool
    let senderId: String
    var senderName: String = ""
    var destId: String? = nil
    var destName: String? = nil
    let rssi: Int?
    var status: MessageStatus
    let time: String
    let timeMs: Int64

    init(id: Int64, text: String, isOutgoing: Bool, senderId: String,
         senderName: String = "", destId: String? = nil, destName: String? = nil,
         rssi: Int? = nil, status: MessageStatus, time: String,
         timeMs: Int64 = Int64(Date().timeIntervalSince1970 * 1000)) {
        self.id = id; self.text = text; self.isOutgoing = isOutgoing
        self.senderId = senderId; self.senderName = senderName
        self.destId = destId; self.destName = destName
        self.rssi = rssi; self.status = status
        self.time = time; self.timeMs = timeMs
    }
}

struct Peer: Identifiable {
    var id: String { deviceId }
    let deviceId: String
    let callSign: String
    let rssi: Int
    let snr: Int
    let txPower: Int
    let batteryPct: Int?
    let lastSeenMs: Int64
    var lat: Double? = nil
    var lon: Double? = nil
}

struct RecentCall: Identifiable {
    let id = UUID()
    let deviceId: String
    let callSign: String
    let isOutgoing: Bool
    let callType: String
    var groupName: String? = nil
    var rssi: Int? = nil
    let timeMs: Int64

    init(deviceId: String, callSign: String, isOutgoing: Bool, callType: String,
         groupName: String? = nil, rssi: Int? = nil,
         timeMs: Int64 = Int64(Date().timeIntervalSince1970 * 1000)) {
        self.deviceId = deviceId; self.callSign = callSign
        self.isOutgoing = isOutgoing; self.callType = callType
        self.groupName = groupName; self.rssi = rssi; self.timeMs = timeMs
    }
}

struct IncomingCall: Identifiable {
    var id: Int64 { timeMs }
    let callType: CallType
    let senderId: String
    let callSign: String
    var lat: Double? = nil
    var lon: Double? = nil
    let callSeq: Int
    var rssi: Int = 0
    let timeMs: Int64

    init(callType: CallType, senderId: String, callSign: String,
         lat: Double? = nil, lon: Double? = nil, callSeq: Int, rssi: Int = 0,
         timeMs: Int64 = Int64(Date().timeIntervalSince1970 * 1000)) {
        self.callType = callType; self.senderId = senderId
        self.callSign = callSign; self.lat = lat; self.lon = lon
        self.callSeq = callSeq; self.rssi = rssi; self.timeMs = timeMs
    }
}

struct FileTransfer: Identifiable {
    var id: Int { sessionId }
    let sessionId: Int
    let fileName: String
    let fileType: Int       // 0x01=photo, 0x02=text, 0x03=binary
    let totalSize: Int
    let chunksTotal: Int
    var chunksDone: Int
    let isOutgoing: Bool
    var status: FileStatus
    let timeMs: Int64
    var data: Data? = nil
    var localPath: String? = nil
    var destMac: Data? = nil
    var destName: String? = nil

    func loadData() -> Data? {
        if let data = data { return data }
        guard let path = localPath else { return nil }
        return try? Data(contentsOf: URL(fileURLWithPath: path))
    }
}

struct ScanDevice: Identifiable {
    var id: UUID { peripheral.identifier }
    let peripheral: CBPeripheral
    let rssi: Int
}

// MARK: - Haversine

func distanceKm(_ lat1: Double, _ lon1: Double, _ lat2: Double, _ lon2: Double) -> Double {
    let R = 6371.0
    let dLat = (lat2 - lat1) * .pi / 180
    let dLon = (lon2 - lon1) * .pi / 180
    let a = sin(dLat / 2) * sin(dLat / 2) +
            cos(lat1 * .pi / 180) * cos(lat2 * .pi / 180) *
            sin(dLon / 2) * sin(dLon / 2)
    return R * 2 * atan2(sqrt(a), sqrt(1 - a))
}
