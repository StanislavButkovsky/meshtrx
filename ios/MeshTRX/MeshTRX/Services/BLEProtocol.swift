import Foundation
import CoreBluetooth

// MARK: - Nordic UART Service UUIDs

enum BLEUUID {
    static let service    = CBUUID(string: "6E400001-B5A3-F393-E0A9-E50E24DCCA9E")
    static let rxChar     = CBUUID(string: "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") // write
    static let txChar     = CBUUID(string: "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") // notify
    static let cccd       = CBUUID(string: "00002902-0000-1000-8000-00805f9b34fb")
}

// MARK: - Command codes

enum BLECmd {
    static let audioTx:      UInt8 = 0x01
    static let audioRx:      UInt8 = 0x02
    static let pttStart:     UInt8 = 0x03
    static let pttEnd:       UInt8 = 0x04
    static let setChannel:   UInt8 = 0x05
    static let statusUpdate: UInt8 = 0x06
    static let sendMessage:  UInt8 = 0x07
    static let recvMessage:  UInt8 = 0x08
    static let messageAck:   UInt8 = 0x09
    static let setSettings:  UInt8 = 0x0A
    static let getSettings:  UInt8 = 0x0B
    static let settingsResp: UInt8 = 0x0C
    static let fileStart:    UInt8 = 0x0D
    static let fileChunk:    UInt8 = 0x0E
    static let fileRecv:     UInt8 = 0x0F
    static let fileProgress: UInt8 = 0x10
    static let setTxMode:    UInt8 = 0x11
    static let voxStatus:    UInt8 = 0x12
    static let voxLevel:     UInt8 = 0x13
    static let getLocation:  UInt8 = 0x14
    static let locationUpd:  UInt8 = 0x15
    static let beaconSent:   UInt8 = 0x16
    static let peerSeen:     UInt8 = 0x17
    static let callAll:      UInt8 = 0x18
    static let callPrivate:  UInt8 = 0x19
    static let callGroup:    UInt8 = 0x1A
    static let callEmergency: UInt8 = 0x1B
    static let callAccept:   UInt8 = 0x1C
    static let callReject:   UInt8 = 0x1D
    static let callCancel:   UInt8 = 0x1E
    static let incomingCall: UInt8 = 0x1F
    static let callStatus:   UInt8 = 0x20
    static let pinCheck:     UInt8 = 0x25
    static let pinResult:    UInt8 = 0x26
    static let fileData:     UInt8 = 0x27
    static let setRepeater:  UInt8 = 0x28
}

// MARK: - Packet builders

enum BLEPacket {

    static func audioTx(_ codec2Data: Data) -> Data {
        var pkt = Data([BLECmd.audioTx])
        pkt.append(codec2Data)
        return pkt
    }

    static func pttStart() -> Data { Data([BLECmd.pttStart]) }
    static func pttEnd()   -> Data { Data([BLECmd.pttEnd]) }

    static func setChannel(_ ch: Int) -> Data {
        Data([BLECmd.setChannel, UInt8(ch & 0xFF)])
    }

    static func sendMessage(seq: Int, text: String) -> Data {
        var pkt = Data([BLECmd.sendMessage, UInt8(seq & 0xFF)])
        pkt.append(text.data(using: .utf8) ?? Data())
        return pkt
    }

    static func setSettings(json: String) -> Data {
        var pkt = Data([BLECmd.setSettings])
        pkt.append(json.data(using: .utf8) ?? Data())
        return pkt
    }

    static func getSettings() -> Data { Data([BLECmd.getSettings]) }

    static func pinCheck(_ pin: Int) -> Data {
        var pkt = Data(count: 5)
        pkt[0] = BLECmd.pinCheck
        pkt[1] = UInt8(pin & 0xFF)
        pkt[2] = UInt8((pin >> 8) & 0xFF)
        pkt[3] = UInt8((pin >> 16) & 0xFF)
        pkt[4] = UInt8((pin >> 24) & 0xFF)
        return pkt
    }

    static func locationUpdate(latE7: Int32, lonE7: Int32, altM: Int16) -> Data {
        var pkt = Data(count: 11)
        pkt[0] = BLECmd.locationUpd
        putInt32(&pkt, offset: 1, value: latE7)
        putInt32(&pkt, offset: 5, value: lonE7)
        putInt16(&pkt, offset: 9, value: altM)
        return pkt
    }

    static func setTxMode(_ mode: TxMode) -> Data {
        Data([BLECmd.setTxMode, mode == .vox ? 1 : 0])
    }

    static func callAll() -> Data { Data([BLECmd.callAll]) }
    static func callPrivate(macSuffix: Data) -> Data {
        var pkt = Data([BLECmd.callPrivate])
        pkt.append(macSuffix)
        return pkt
    }
    static func callGroup(groupId: UInt8) -> Data {
        Data([BLECmd.callGroup, groupId])
    }
    static func callEmergency() -> Data { Data([BLECmd.callEmergency]) }
    static func callAccept()    -> Data { Data([BLECmd.callAccept]) }
    static func callReject()    -> Data { Data([BLECmd.callReject]) }
    static func callCancel()    -> Data { Data([BLECmd.callCancel]) }

    static func fileStart(sessionId: UInt8, fileType: UInt8, totalSize: UInt16,
                          fileName: String, destMac: Data? = nil) -> Data {
        var pkt = Data([BLECmd.fileStart, sessionId, fileType])
        pkt.append(UInt8(totalSize & 0xFF))
        pkt.append(UInt8((totalSize >> 8) & 0xFF))
        if let mac = destMac { pkt.append(mac) } else { pkt.append(Data([0, 0])) }
        pkt.append(fileName.data(using: .utf8) ?? Data())
        pkt.append(0) // null terminator
        return pkt
    }

    static func fileChunk(sessionId: UInt8, chunkIndex: UInt16, data: Data) -> Data {
        var pkt = Data([BLECmd.fileChunk, sessionId])
        pkt.append(UInt8(chunkIndex & 0xFF))
        pkt.append(UInt8((chunkIndex >> 8) & 0xFF))
        pkt.append(data)
        return pkt
    }

    static func setRepeater(enable: Bool, ssid: String = "", password: String = "", ip: String = "") -> Data {
        var pkt = Data([BLECmd.setRepeater, enable ? 1 : 0])
        pkt.append(ssid.data(using: .utf8) ?? Data()); pkt.append(0)
        pkt.append(password.data(using: .utf8) ?? Data()); pkt.append(0)
        pkt.append(ip.data(using: .utf8) ?? Data()); pkt.append(0)
        return pkt
    }

    // MARK: - Helpers

    private static func putInt32(_ data: inout Data, offset: Int, value: Int32) {
        data[offset]     = UInt8(value & 0xFF)
        data[offset + 1] = UInt8((value >> 8) & 0xFF)
        data[offset + 2] = UInt8((value >> 16) & 0xFF)
        data[offset + 3] = UInt8((value >> 24) & 0xFF)
    }

    private static func putInt16(_ data: inout Data, offset: Int, value: Int16) {
        data[offset]     = UInt8(value & 0xFF)
        data[offset + 1] = UInt8((value >> 8) & 0xFF)
    }
}

// MARK: - Packet parsing helpers

extension Data {
    func getInt32LE(at offset: Int) -> Int32 {
        guard offset + 4 <= count else { return 0 }
        return Int32(self[offset]) |
               Int32(self[offset+1]) << 8 |
               Int32(self[offset+2]) << 16 |
               Int32(self[offset+3]) << 24
    }

    func getUInt16LE(at offset: Int) -> UInt16 {
        guard offset + 2 <= count else { return 0 }
        return UInt16(self[offset]) | UInt16(self[offset+1]) << 8
    }

    func getInt16LE(at offset: Int) -> Int16 {
        Int16(bitPattern: getUInt16LE(at: offset))
    }
}
