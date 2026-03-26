import Foundation
import CoreBluetooth
import os.log

/// Central controller — wires BLE, Audio, VOX, and AppState together.
/// Equivalent of Android MeshTRXService.
@MainActor
class MeshTRXController: ObservableObject {

    let bleManager = BLEManager()
    let audioEngine = AudioEngine()
    let voxEngine = VoxEngine()
    let appState: AppState

    private var msgSeq = 0
    private var fileSessionCounter = 0
    private var authorizedDevices: Set<String> = []

    // Incoming file buffer
    private var incomingFileBuffer: Data?
    private var incomingFileSize = 0
    private var incomingFileOffset = 0
    private var incomingFileType = 0
    private var incomingFileName = ""
    private var incomingFileSender = ""

    // Peer cleanup timer
    private var peerCleanupTimer: Timer?

    private let log = Logger(subsystem: "com.meshtrx.app", category: "Controller")

    init(appState: AppState) {
        self.appState = appState
        loadAuthorizedDevices()
        setupCallbacks()
        audioEngine.setup()
        startPeerCleanup()
    }

    // MARK: - Setup callbacks

    private func setupCallbacks() {
        bleManager.onConnected = { [weak self] in
            Task { @MainActor in self?.handleConnected() }
        }
        bleManager.onDisconnected = { [weak self] in
            Task { @MainActor in self?.handleDisconnected() }
        }
        bleManager.onScanResult = { [weak self] peripheral, rssi in
            Task { @MainActor in self?.handleScanResult(peripheral, rssi: rssi) }
        }
        bleManager.onNeedPin = { [weak self] in
            Task { @MainActor in self?.handleNeedPin() }
        }
        bleManager.onDataReceived = { [weak self] data in
            Task { @MainActor in self?.handleBleData(data) }
        }

        audioEngine.onAudioEncoded = { [weak self] encoded in
            guard let self = self else { return }
            Task { @MainActor in
                if self.appState.isPttActive && self.bleManager.isConnected {
                    self.bleManager.sendAudioData(encoded)
                }
            }
        }
        audioEngine.onRmsLevel = { [weak self] rms in
            guard let self = self else { return }
            Task { @MainActor in
                self.appState.rmsLevel = rms
                if self.appState.txMode == .vox {
                    self.voxEngine.process(rms: rms)
                }
            }
        }

        voxEngine.onTxActivated = { [weak self] in
            Task { @MainActor in
                guard let self = self else { return }
                self.appState.isPttActive = true
                self.audioEngine.sendAudio = true
                self.bleManager.sendPttStart()
            }
        }
        voxEngine.onTxDeactivated = { [weak self] in
            Task { @MainActor in
                guard let self = self else { return }
                self.appState.isPttActive = false
                self.audioEngine.sendAudio = false
                DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
                    self.bleManager.sendPttEnd()
                }
            }
        }
        voxEngine.onStateChanged = { [weak self] state in
            Task { @MainActor in self?.appState.voxState = state }
        }
    }

    // MARK: - Connection events

    private func handleConnected() {
        appState.bleState = .connected
        appState.statusMessage = "Подключено"
        audioEngine.startPlayback()
        bleManager.requestSettings()
        sendGpsToDevice()
    }

    private func handleDisconnected() {
        appState.bleState = .disconnected
        appState.statusMessage = "Отключено"
        audioEngine.stopPlayback()
        if appState.txMode == .vox {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
        }
    }

    private func handleScanResult(_ peripheral: CBPeripheral, rssi: Int) {
        if !appState.scanResults.contains(where: { $0.peripheral.identifier == peripheral.identifier }) {
            appState.scanResults.append(ScanDevice(peripheral: peripheral, rssi: rssi))
        }
    }

    private func handleNeedPin() {
        // For now, auto-connect (no PIN required by default)
        // TODO: check authorizedDevices and show PIN dialog if needed
        handleConnected()
    }

    // MARK: - Public commands

    func startScan() {
        appState.scanResults = []
        appState.bleState = .scanning
        bleManager.startScan()
    }

    func connect(_ peripheral: CBPeripheral) {
        appState.bleState = .connecting
        appState.deviceName = peripheral.name ?? "MeshTRX"
        bleManager.connect(peripheral)
    }

    func disconnect() {
        if appState.txMode == .vox {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
        }
        bleManager.disconnect()
        appState.bleState = .disconnected
    }

    func forgetDevice() {
        disconnect()
        bleManager.forgetDevice()
        appState.deviceName = ""
    }

    func submitPin(_ pin: Int) {
        bleManager.sendPinCheck(pin)
    }

    // MARK: - PTT

    func pttDown() {
        guard appState.txMode == .ptt else { return }
        appState.isPttActive = true
        bleManager.sendPttStart()
        audioEngine.startRecording()
    }

    func pttUp() {
        guard appState.txMode == .ptt else { return }
        appState.isPttActive = false
        audioEngine.stopRecording()
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.1) {
            self.bleManager.sendPttEnd()
        }
    }

    // MARK: - TX Mode

    func setTxMode(_ mode: TxMode) {
        let prev = appState.txMode
        appState.txMode = mode

        if prev == .vox && mode == .ptt {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
            appState.isPttActive = false
            appState.voxState = .idle
        }
        if mode == .vox && appState.bleState == .connected {
            voxEngine.reset()
            audioEngine.startVoxMonitoring()
        }
    }

    func setChannel(_ ch: Int) {
        bleManager.setChannel(ch)
        appState.currentChannel = ch
    }

    // MARK: - Messages

    func sendTextMessage(_ text: String, destId: String? = nil, destName: String? = nil) {
        let seq = msgSeq
        msgSeq += 1
        bleManager.sendMessage(seq: seq, text: text)

        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let fmt = DateFormatter()
        fmt.dateFormat = "HH:mm"
        let msg = ChatMessage(
            id: now, text: text, isOutgoing: true, senderId: "me",
            senderName: appState.callSign, destId: destId, destName: destName,
            rssi: nil, status: .sending, time: fmt.string(from: Date()), timeMs: now
        )
        appState.messages.append(msg)
    }

    // MARK: - Calls

    func callAll() {
        bleManager.send(BLEPacket.callAll())
        addRecentCall(deviceId: "BROADCAST", callSign: "Общий канал", isOutgoing: true, callType: "ALL")
    }

    func callPrivate(macSuffix: Data, callSign: String) {
        bleManager.send(BLEPacket.callPrivate(macSuffix: macSuffix))
        let devId = macSuffix.map { String(format: "%02X", $0) }.joined()
        addRecentCall(deviceId: devId, callSign: callSign, isOutgoing: true, callType: "PRIVATE")
    }

    func callEmergency() {
        bleManager.send(BLEPacket.callEmergency())
        addRecentCall(deviceId: "SOS", callSign: "SOS", isOutgoing: true, callType: "SOS")
    }

    func acceptCall(_ call: IncomingCall) {
        let pkt = Data([BLECmd.callAccept, UInt8(call.callSeq & 0xFF)])
        bleManager.send(pkt)
        appState.incomingCall = nil
        appState.callActive = true
        audioEngine.startPlayback()
    }

    func rejectCall(_ call: IncomingCall) {
        let pkt = Data([BLECmd.callReject, UInt8(call.callSeq & 0xFF)])
        bleManager.send(pkt)
        appState.incomingCall = nil
    }

    // MARK: - Send file

    func sendFile(fileName: String, fileType: Int, data: Data, destMac: Data? = nil, destName: String? = nil) {
        let sessionId = (fileSessionCounter + 1) & 0xFF
        fileSessionCounter += 1
        let chunkSize = 120
        let totalChunks = (data.count + chunkSize - 1) / chunkSize
        let dest = destMac ?? Data([0, 0])

        // FILE_START header
        var header = Data(count: 36)
        header[0] = BLECmd.fileStart
        header[1] = 0xC0 // PKT_TYPE_FILE_START
        header[2] = UInt8(appState.currentChannel & 0xFF)
        header[3] = UInt8(sessionId)
        header[4] = 2 // TTL
        header[5] = 0; header[6] = 0 // sender (ESP32 fills)
        header[7] = dest.count > 0 ? dest[0] : 0
        header[8] = dest.count > 1 ? dest[1] : 0
        header[9] = UInt8(fileType & 0xFF)
        header[10] = UInt8(totalChunks & 0xFF)
        header[11] = UInt8((totalChunks >> 8) & 0xFF)
        header[12] = UInt8(data.count & 0xFF)
        header[13] = UInt8((data.count >> 8) & 0xFF)
        header[14] = UInt8((data.count >> 16) & 0xFF)
        header[15] = UInt8((data.count >> 24) & 0xFF)
        // name (max 19 bytes)
        let nameBytes = Array(fileName.data(using: .utf8)?.prefix(19) ?? Data())
        for (i, b) in nameBytes.enumerated() { header[16 + i] = b }

        bleManager.send(header)

        // Add to transfer list
        let transfer = FileTransfer(
            sessionId: sessionId, fileName: fileName, fileType: fileType,
            totalSize: data.count, chunksTotal: totalChunks, chunksDone: 0,
            isOutgoing: true, status: .transferring,
            timeMs: Int64(Date().timeIntervalSince1970 * 1000),
            data: data, destMac: destMac, destName: destName
        )
        appState.fileTransfers.insert(transfer, at: 0)

        // Send chunks with timer (100ms interval)
        var chunkIndex = 0
        Timer.scheduledTimer(withTimeInterval: 0.1, repeats: true) { [weak self] timer in
            guard let self = self else { timer.invalidate(); return }
            guard chunkIndex < totalChunks else {
                timer.invalidate()
                if let idx = self.appState.fileTransfers.firstIndex(where: { $0.sessionId == sessionId }) {
                    self.appState.fileTransfers[idx].status = .done
                }
                return
            }
            let offset = chunkIndex * chunkSize
            let len = min(chunkSize, data.count - offset)
            var chunkData = Data([BLECmd.fileChunk,
                              UInt8(chunkIndex & 0xFF), UInt8((chunkIndex >> 8) & 0xFF),
                              dest[0], dest.count > 1 ? dest[1] : 0])
            chunkData.append(data[offset..<offset + len])
            self.bleManager.send(chunkData)

            chunkIndex += 1
            if let idx = self.appState.fileTransfers.firstIndex(where: { $0.sessionId == sessionId }) {
                self.appState.fileTransfers[idx].chunksDone = chunkIndex
            }
        }
    }

    private func addRecentCall(deviceId: String, callSign: String, isOutgoing: Bool, callType: String) {
        let call = RecentCall(deviceId: deviceId, callSign: callSign,
                              isOutgoing: isOutgoing, callType: callType)
        appState.recentCalls.insert(call, at: 0)
        if appState.recentCalls.count > 20 {
            appState.recentCalls = Array(appState.recentCalls.prefix(20))
        }
    }

    // MARK: - Settings

    func setCallSign(_ cs: String) {
        appState.callSign = cs
        UserDefaults.standard.set(cs, forKey: "my_callsign")
        bleManager.sendSettings(json: "{\"callsign\":\"\(cs)\"}")
    }

    func applySettings(json: String) {
        bleManager.sendSettings(json: json)
    }

    // MARK: - GPS

    private func sendGpsToDevice() {
        guard let lat = appState.myLat, let lon = appState.myLon else {
            bleManager.sendLocationUpdate(latE7: 0, lonE7: 0, altM: 0)
            return
        }
        bleManager.sendLocationUpdate(
            latE7: Int32(lat * 1e7),
            lonE7: Int32(lon * 1e7),
            altM: 0
        )
    }

    // MARK: - BLE Data handling

    private func handleBleData(_ data: Data) {
        guard !data.isEmpty else { return }
        let cmd = data[0]

        switch cmd {
        case BLECmd.audioRx:
            handleAudioRx(data)
        case BLECmd.statusUpdate:
            handleStatusUpdate(data)
        case BLECmd.recvMessage:
            handleRecvMessage(data)
        case BLECmd.messageAck:
            log.info("Message ACK seq=\(data.count > 1 ? Int(data[1]) : -1)")
        case BLECmd.peerSeen:
            handlePeerSeen(data)
        case BLECmd.getLocation:
            sendGpsToDevice()
        case BLECmd.settingsResp:
            let json = String(data: data.dropFirst(), encoding: .utf8) ?? ""
            log.info("Settings: \(json)")
        case BLECmd.pinResult:
            handlePinResult(data)
        case BLECmd.fileProgress:
            handleFileProgress(data)
        case BLECmd.fileRecv:
            handleFileRecv(data)
        case BLECmd.fileData:
            handleFileData(data)
        case BLECmd.incomingCall:
            handleIncomingCall(data)
        case BLECmd.callStatus:
            if data.count >= 2 && data[1] == 0 {
                appState.callActive = false
            }
        default:
            break
        }
    }

    // MARK: - Audio RX

    private func handleAudioRx(_ data: Data) {
        if data.count >= 68 {
            let flags = Int(data[1])
            let audio = Data(data[4..<68])
            let isLast = (flags & 0x02) != 0

            let listen = appState.listenMode == .all
            let active = appState.callActive
            if listen || active {
                audioEngine.playEncodedPacket(audio, isLastPacket: isLast)
            }
        } else if data.count >= 66 {
            // Old firmware compat
            let flags = Int(data[1])
            let audio = Data(data[2..<66])
            let isLast = (flags & 0x02) != 0
            let listen = appState.listenMode == .all
            let active = appState.callActive
            if listen || active {
                audioEngine.playEncodedPacket(audio, isLastPacket: isLast)
            }
        }
    }

    // MARK: - Status

    private func handleStatusUpdate(_ data: Data) {
        guard data.count >= 5 else { return }
        appState.currentChannel = Int(data[1])
        let r = Int(data[2]) | (Int(data[3]) << 8)
        appState.rssi = Int(Int16(bitPattern: UInt16(r)))
        appState.snr = Int(Int8(bitPattern: data[4]))
    }

    // MARK: - Messages

    private func handleRecvMessage(_ data: Data) {
        guard data.count >= 4 else { return }
        let rssiVal = Int(Int8(bitPattern: data[1]))
        let textEnd = data[2...].firstIndex(of: 0) ?? data.endIndex
        let text = String(data: data[2..<textEnd], encoding: .utf8) ?? ""

        var senderId = "??"
        if textEnd + 2 < data.count {
            senderId = String(format: "%02X%02X", data[textEnd + 1], data[textEnd + 2])
        }

        let senderName = appState.peers.first { $0.deviceId.hasSuffix(senderId) }?.callSign ?? "TX-\(senderId)"
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let fmt = DateFormatter()
        fmt.dateFormat = "HH:mm"

        let msg = ChatMessage(
            id: now, text: text, isOutgoing: false, senderId: senderId,
            senderName: senderName, rssi: rssiVal, status: .delivered,
            time: fmt.string(from: Date()), timeMs: now
        )
        appState.messages.append(msg)
        appState.unreadMessages += 1
    }

    // MARK: - Peer discovery

    private func handlePeerSeen(_ data: Data) {
        guard data.count >= 28 else { return }
        let id = String(format: "%02X%02X%02X%02X", data[1], data[2], data[3], data[4])
        let csData = data[5..<14]
        let cs = String(data: csData, encoding: .utf8)?.trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""

        let latE7 = data.getInt32LE(at: 14)
        let lonE7 = data.getInt32LE(at: 18)
        let pRssi = Int(Int16(bitPattern: data.getUInt16LE(at: 22)))
        let pSnr = Int(Int8(bitPattern: data[24]))
        let txPwr = Int(data[25])
        let batt = Int(data[26])

        let lat: Double? = (latE7 != 0 || lonE7 != 0) ? Double(latE7) / 1e7 : nil
        let lon: Double? = (latE7 != 0 || lonE7 != 0) ? Double(lonE7) / 1e7 : nil

        let peer = Peer(
            deviceId: id, callSign: cs, rssi: pRssi, snr: pSnr, txPower: txPwr,
            batteryPct: batt == 0xFF ? nil : batt,
            lastSeenMs: Int64(Date().timeIntervalSince1970 * 1000),
            lat: lat, lon: lon
        )

        appState.peers.removeAll { $0.deviceId == id }
        appState.peers.append(peer)
    }

    // MARK: - Peer cleanup

    private func startPeerCleanup() {
        peerCleanupTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in
            guard let self = self else { return }
            Task { @MainActor in
                self.cleanupPeers()
            }
        }
    }

    private func cleanupPeers() {
        let timeoutMs = Int64(appState.peerTimeoutMin) * 60 * 1000
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        appState.peers.removeAll { (now - $0.lastSeenMs) > timeoutMs }
    }

    // MARK: - Calls

    private func handleIncomingCall(_ data: Data) {
        guard data.count >= 10 else { return }
        let callTypeCode = Int(data[1])
        let senderId = String(format: "%02X%02X", data[2], data[3])
        let csData = data[4..<min(13, data.count)]
        let callSign = String(data: csData, encoding: .utf8)?.trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
        let callSeq = Int(data[min(13, data.count - 1)])

        let call = IncomingCall(
            callType: CallType.fromCode(callTypeCode),
            senderId: senderId, callSign: callSign,
            callSeq: callSeq
        )
        appState.incomingCall = call
        addRecentCall(deviceId: senderId, callSign: callSign, isOutgoing: false,
                      callType: call.callType == .emergency ? "SOS" : call.callType == .`private` ? "PRIVATE" : "ALL")
    }

    // MARK: - PIN

    private func handlePinResult(_ data: Data) {
        guard data.count >= 2 else { return }
        if data[1] == 1 {
            log.info("PIN accepted")
            handleConnected()
        } else {
            log.info("PIN rejected")
            appState.statusMessage = "Неверный PIN"
            bleManager.disconnect()
        }
    }

    // MARK: - File transfer

    private func handleFileProgress(_ data: Data) {
        guard data.count >= 6 else { return }
        let sid = Int(data[1])
        let done = Int(data.getUInt16LE(at: 2))
        if let idx = appState.fileTransfers.firstIndex(where: { $0.sessionId == sid }) {
            appState.fileTransfers[idx].chunksDone = done
            appState.fileTransfers[idx].status = .transferring
        }
    }

    private func handleFileRecv(_ data: Data) {
        guard data.count >= 7 else { return }
        incomingFileType = Int(data[1])
        incomingFileSize = Int(data.getInt32LE(at: 2))
        incomingFileBuffer = Data(count: incomingFileSize)
        incomingFileOffset = 0

        if data.count >= 29 {
            incomingFileSender = String(format: "%02X%02X", data[7], data[8])
            let nameData = data[9..<min(29, data.count)]
            incomingFileName = String(data: nameData, encoding: .utf8)?.trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
        } else if data.count >= 27 {
            let nameData = data[7..<27]
            incomingFileName = String(data: nameData, encoding: .utf8)?.trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
        }
    }

    private func handleFileData(_ data: Data) {
        guard data.count > 1, incomingFileBuffer != nil else { return }
        let chunk = data[1...]
        let remaining = incomingFileSize - incomingFileOffset
        let toCopy = min(chunk.count, remaining)
        incomingFileBuffer!.replaceSubrange(incomingFileOffset..<incomingFileOffset + toCopy,
                                             with: chunk.prefix(toCopy))
        incomingFileOffset += toCopy

        if incomingFileOffset >= incomingFileSize {
            // File complete
            let transfer = FileTransfer(
                sessionId: fileSessionCounter,
                fileName: incomingFileName.isEmpty ? "file" : incomingFileName,
                fileType: incomingFileType,
                totalSize: incomingFileSize,
                chunksTotal: 1, chunksDone: 1,
                isOutgoing: false, status: .done,
                timeMs: Int64(Date().timeIntervalSince1970 * 1000),
                data: incomingFileBuffer
            )
            fileSessionCounter += 1
            appState.fileTransfers.insert(transfer, at: 0)
            incomingFileBuffer = nil
            log.info("File received: \(transfer.fileName)")
        }
    }

    // MARK: - Authorized devices

    private func loadAuthorizedDevices() {
        authorizedDevices = Set(UserDefaults.standard.stringArray(forKey: "authorized_devices") ?? [])
    }

    private func authorizeDevice(_ id: String) {
        authorizedDevices.insert(id)
        UserDefaults.standard.set(Array(authorizedDevices), forKey: "authorized_devices")
    }

    // MARK: - Cleanup

    func destroy() {
        peerCleanupTimer?.invalidate()
        audioEngine.destroy()
        bleManager.disconnect()
    }
}
