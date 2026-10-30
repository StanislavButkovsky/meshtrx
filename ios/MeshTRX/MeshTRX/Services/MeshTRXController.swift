import Foundation
import Combine
import CoreBluetooth
import AVFoundation
import UserNotifications
import os.log

/// Central controller — wires BLE, Audio, VOX, and AppState together.
/// Equivalent of Android MeshTRXService.
@MainActor
class MeshTRXController: ObservableObject {

    let bleManager = BLEManager()
    let audioEngine = AudioEngine()
    // GPS живёт с приложением, как в Android: раньше включался только на
    // вкладке «Карта», и рация получала координаты лишь при подключении.
    let locationManager = LocationManager()
    let notificationDelegate = NotificationDelegate()
    private var lastGpsSentAt: Date = .distantPast
    var cancellables = Set<AnyCancellable>()
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
        loadPersisted()
        loadAuthorizedDevices()
        setupNotifications()
        setupCallbacks()
        audioEngine.setup()
        startPeerCleanup()
        // Доступ к микрофону спрашиваем сразу, как Android: иначе первое
        // нажатие PTT уходит в тишину, пока система показывает свой вопрос.
        AVAudioSession.sharedInstance().requestRecordPermission { _ in }
        setupPersistence()
        checkForUpdatesIfDue()
        setupLocation()
    }

    private func setupLocation() {
        locationManager.requestPermission()
        locationManager.$latitude.combineLatest(locationManager.$longitude)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] lat, lon in
                guard let self = self, let lat = lat, let lon = lon else { return }
                self.appState.myLat = lat
                self.appState.myLon = lon
                // На рацию — при изменении, но не чаще раза в 10 с
                if self.appState.bleState == .connected,
                   Date().timeIntervalSince(self.lastGpsSentAt) >= 10 {
                    self.lastGpsSentAt = Date()
                    self.sendGpsToDevice()
                }
            }
            .store(in: &cancellables)
    }

    // MARK: - Предел передачи

    static let pttMaxSeconds = 10
    private var pttLimitTask: Task<Void, Never>?

    /// Отсчёт речи — и для кнопки, и для голосовой активации. По истечении
    /// передача гасится сама.
    private func startPttLimit() {
        pttLimitTask?.cancel()
        appState.pttSecondsLeft = Self.pttMaxSeconds
        pttLimitTask = Task { @MainActor [weak self] in
            for left in stride(from: Self.pttMaxSeconds - 1, through: 0, by: -1) {
                try? await Task.sleep(nanoseconds: 1_000_000_000)
                guard let self = self, !Task.isCancelled, self.appState.isPttActive else { return }
                self.appState.pttSecondsLeft = left
            }
            guard let self = self, !Task.isCancelled, self.appState.isPttActive else { return }
            self.appState.statusMessage = "Достигнут предел \(Self.pttMaxSeconds) с — передача завершена"
            if self.appState.txMode == .vox {
                self.voxEngine.reset()
                self.endTransmission()
            } else {
                self.pttUp(force: true)
            }
        }
    }

    private func stopPttLimit() {
        pttLimitTask?.cancel()
        pttLimitTask = nil
        appState.pttSecondsLeft = nil
    }

    private func endTransmission() {
        stopPttLimit()
        appState.isPttActive = false
        audioEngine.sendAudio = false
        // «Конец» — после хвоста речи: рация шлёт его в эфир сразу, а кадры
        // речи у неё в очереди. Пришёл раньше них — сигнал конца на той
        // стороне терялся, а конец фразы обрывался. 0,1 с не хватало.
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.35) {
            self.bleManager.sendPttEnd()
        }
    }

    // MARK: - Setup callbacks

    private func setupCallbacks() {
        bleManager.onConnected = { [weak self] in
            Task { @MainActor in self?.handleConnected() }
        }
        bleManager.onDisconnected = { [weak self] in
            Task { @MainActor in self?.handleDisconnected() }
        }
        bleManager.onConnecting = { [weak self] name in
            Task { @MainActor in
                guard let self = self else { return }
                self.appState.bleState = .connecting
                self.appState.deviceName = name
                self.appState.statusMessage = "Подключение к \(name)..."
            }
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
        bleManager.onScanStopped = { [weak self] in
            Task { @MainActor in
                guard let self = self else { return }
                if self.appState.bleState == .scanning {
                    // Как в Android: одна рация — подключаемся сразу; ни одной — так и говорим
                    if self.appState.scanResults.count == 1, let only = self.appState.scanResults.first {
                        self.connect(only.peripheral)
                    } else {
                        self.appState.bleState = .disconnected
                        self.appState.statusMessage = self.appState.scanResults.isEmpty
                            ? "Устройства не найдены" : "Выберите устройство"
                    }
                }
            }
        }

        audioEngine.onAudioEncoded = { [weak self] encoded in
            guard let self = self else { return }
            Task { @MainActor in
                // При адресате голос не идёт в эфир сразу: копится и уходит файлом.
                // Флаг «передача» здесь не проверяем: кадр закодирован, пока
                // кнопка была нажата, а сюда доходит позже — и хвост фразы
                // отбрасывался, когда кнопку уже отпустили.
                if self.bleManager.isConnected && self.appState.pttTargetId == nil {
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
                self.startPttLimit()
            }
        }
        voxEngine.onTxDeactivated = { [weak self] in
            Task { @MainActor in
                guard let self = self, self.appState.isPttActive else { return }
                self.endTransmission()
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
        appState.band = .eu868      // рация могла смениться — ждём её сетку заново
        bleManager.requestSettings()
        bleManager.scanPeers()      // список абонентов должен наполниться сразу, как в Android
        sendGpsToDevice()
        // Версию и ключ рация называет сама, но уведомление, посланное до
        // подписки телефона, теряется — переспрашиваем, по одному и с паузой.
        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 3_000_000_000)
            self?.bleManager.send(Data([BLECmd.getKeyState]))
            try? await Task.sleep(nanoseconds: 700_000_000)
            self?.bleManager.send(Data([BLECmd.getFwVersion]))
        }
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
        let addr = bleManager.connectedDeviceIdentifier ?? ""
        if authorizedDevices.contains(addr) {
            log.info("Device \(addr) already authorized, skipping PIN")
            bleManager.markConnected()
        } else {
            log.info("Need PIN for \(addr)")
            appState.bleState = .connecting
            appState.statusMessage = "Введите PIN"
            appState.showPinDialog = true
        }
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

    /// «Новое устройство», как в Android: забыть рацию и её PIN и сразу искать.
    /// Раньше адрес забывался, а PIN оставались, и поиск не начинался.
    func forgetDevice() {
        disconnect()
        bleManager.forgetDevice()
        authorizedDevices.removeAll()
        UserDefaults.standard.removeObject(forKey: "authorized_devices")
        appState.deviceName = ""
        startScan()
    }

    func submitPin(_ pin: Int) {
        bleManager.sendPinCheck(pin)
    }

    // MARK: - PTT

    func pttDown() {
        guard appState.txMode == .ptt, !appState.isPttActive else { return }
        // Нажали снова, пока дописывался хвост прошлой фразы — сначала закрыть её
        if let tail = pttTailTask {
            tail.cancel(); pttTailTask = nil
            audioEngine.stopRecording()
            endTransmission()
        }
        // Без доступа к микрофону передача ушла бы пустой — говорим об этом прямо
        if AVAudioSession.sharedInstance().recordPermission != .granted {
            AVAudioSession.sharedInstance().requestRecordPermission { _ in }
            appState.pttDeliveryText = "нет доступа к микрофону — разрешите в Настройках iPhone"
            Task { @MainActor [weak self] in
                try? await Task.sleep(nanoseconds: 4_000_000_000)
                self?.appState.pttDeliveryText = nil
            }
            return
        }
        appState.isPttActive = true
        audioEngine.rogerBeep = appState.rogerBeep
        if appState.pttTargetId != nil {
            // Адресная передача, как в Android: запись до 10 с, потом файлом 0x05
            appState.pttDeliveryText = nil
            audioEngine.startBufferedRecording()
        } else {
            bleManager.sendPttStart()
            audioEngine.startRecording()
        }
        startPttLimit()
    }

    /// Сколько микрофон пишет после отпускания кнопки. У iPhone вход звука с
    /// задержкой: последние 100–200 мс фразы ещё не дошли до приложения, когда
    /// кнопку отпустили, и выключение микрофона сразу срезало конец фразы.
    static let pttTailSeconds = 0.25
    private var pttTailTask: Task<Void, Never>?

    /// force — передачу погасил предел времени, а кнопка ещё нажата.
    func pttUp(force: Bool = false) {
        guard appState.txMode == .ptt, appState.isPttActive || force else { return }
        stopPttLimit()
        appState.isPttActive = false
        let target = appState.pttTargetId
        pttTailTask?.cancel()
        pttTailTask = Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(Self.pttTailSeconds * 1_000_000_000))
            guard let self = self, !Task.isCancelled else { return }
            self.pttTailTask = nil      // хвост дописан — следующему нажатию закрывать нечего
            self.audioEngine.stopRecording()
            if let target = target {
                self.sendAddressedVoice(to: target)
            } else {
                self.endTransmission()
            }
        }
    }

    private func sendAddressedVoice(to target: String) {
        guard let data = audioEngine.getRecordedCodec2Data(),
              let b0 = UInt8(target.prefix(2), radix: 16),
              let b1 = UInt8(target.suffix(2), radix: 16) else { return }
        let name = "ptt_\(Int(Date().timeIntervalSince1970)).c2"
        appState.pttDeliveryText = "отправка..."
        sendFile(fileName: name, fileType: 0x05, data: data,
                 destMac: Data([b0, b1]), destName: appState.pttTargetName)
        Task { @MainActor [weak self] in
            // Ждём итога от рации, как в Android 4.4.33: раньше через 15 с
            // писали «не доставлено», хотя с переспросами доставка дольше и
            // собеседник всё слышал. Через 15 с — «доставка...», до 2 минут.
            for i in 0..<240 {
                try? await Task.sleep(nanoseconds: 500_000_000)
                guard let self = self else { return }
                if let t = self.appState.fileTransfers.first(where: { $0.isOutgoing && $0.fileName == name }),
                   t.status == .done || t.status == .failed {
                    self.appState.pttDeliveryText = t.status == .done ? "доставлено" : "не доставлено"
                    break
                }
                if i == 30 { self.appState.pttDeliveryText = "доставка..." }
            }
            if let d = self?.appState.pttDeliveryText, d == "отправка..." || d == "доставка..." {
                self?.appState.pttDeliveryText = "не доставлено"
            }
            try? await Task.sleep(nanoseconds: 3_000_000_000)
            self?.appState.pttDeliveryText = nil
        }
    }

    /// Вызов из строки «@»: общий — всем, иначе личный выбранному абоненту.
    /// Прошивке нужен полный 4-байтный id — достраиваем по списку абонентов.
    func callTarget() {
        guard let short = appState.pttTargetId else { callAll(); return }
        guard let peer = appState.peers.first(where: { $0.deviceId.hasSuffix(short) }),
              peer.deviceId.count == 8 else {
            appState.statusMessage = "Нет полного адреса абонента — дождитесь его маяка"
            return
        }
        let bytes = stride(from: 0, to: 8, by: 2).compactMap { i -> UInt8? in
            let s = peer.deviceId.index(peer.deviceId.startIndex, offsetBy: i)
            return UInt8(peer.deviceId[s..<peer.deviceId.index(s, offsetBy: 2)], radix: 16)
        }
        callPrivate(macSuffix: Data(bytes), callSign: appState.pttTargetName ?? peer.callSign)
    }

    // MARK: - TX Mode

    func setTxMode(_ mode: TxMode) {
        let prev = appState.txMode
        appState.txMode = mode
        bleManager.setTxMode(mode)

        if prev == .vox && mode == .ptt {
            audioEngine.stopVoxMonitoring()
            voxEngine.reset()
            stopPttLimit()
            appState.isPttActive = false
            appState.voxState = .idle
        }
        if mode == .vox && appState.bleState == .connected {
            voxEngine.reset()
            requestMicPermissionAndStartVox()
        }
    }

    private func requestMicPermissionAndStartVox() {
        AVAudioSession.sharedInstance().requestRecordPermission { [weak self] granted in
            Task { @MainActor in
                guard let self = self, granted else {
                    self?.log.error("Microphone permission denied")
                    self?.appState.txMode = .ptt
                    return
                }
                self.audioEngine.startVoxMonitoring()
            }
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
        bleManager.sendMessage(seq: seq, destId: destId, text: text)

        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let fmt = DateFormatter()
        fmt.dateFormat = "HH:mm"
        let isAddressed = !(destId ?? "").isEmpty
        var msg = ChatMessage(
            id: now, text: text, isOutgoing: true, senderId: "me",
            senderName: appState.callSign, destId: destId, destName: destName,
            rssi: nil, status: isAddressed ? .sending : .sent,
            time: fmt.string(from: Date()), timeMs: now
        )
        msg.seq = seq
        appState.messages.append(msg)

        // Адресное повторяем, пока не придёт подтверждение — паузы растут, как
        // в Android: через ретранслятор ответу нужно больше времени.
        if isAddressed {
            retryText(msgId: now, seq: seq, destId: destId, text: text, pauses: [3, 5, 8])
        }
    }

    private func retryText(msgId: Int64, seq: Int, destId: String?, text: String, pauses: [Double]) {
        guard let pause = pauses.first else {
            // Подтверждения так и нет: «ушло, ответа нет», а не «не доставлено» —
            // потеряться мог и сам ответ.
            if let i = appState.messages.firstIndex(where: { $0.id == msgId }),
               appState.messages[i].status == .sending {
                appState.messages[i].status = .sent
            }
            return
        }
        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: UInt64(pause * 1_000_000_000))
            guard let self = self,
                  let i = self.appState.messages.firstIndex(where: { $0.id == msgId }),
                  self.appState.messages[i].status == .sending else { return }
            self.log.info("Text retry seq=\(seq)")
            self.bleManager.sendMessage(seq: seq, destId: destId, text: text)
            self.retryText(msgId: msgId, seq: seq, destId: destId, text: text,
                           pauses: Array(pauses.dropFirst()))
        }
    }

    /// Подтверждение от рации: [0x09, seq, status]. status (прошивка 4.4.32):
    /// 0 — на телефоне адресата, 1 — принято его рацией, телефона у неё нет.
    /// Старые прошивки третьего байта не шлют — это «доставлено».
    private func handleMessageAck(_ data: Data) {
        guard data.count >= 2 else { return }
        let seq = Int(data[1])
        let stored = data.count >= 3 && data[2] == 1
        log.info("Message ACK seq=\(seq) stored=\(stored)")
        guard let i = appState.messages.lastIndex(where: {
            $0.isOutgoing && ($0.seq.map { $0 & 0xFF == seq } ?? false) &&
            ($0.status == .sending || $0.status == .sent)
        }) else { return }
        appState.messages[i].status = stored ? .accepted : .delivered
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

    /// Файлы v2, как в Android: [0x30, тип, адрес(2), размер(4), имя(20)], затем
    /// данные кусками [0x31, …] по 120 байт, а итог рация сообщает сама (0x32).
    /// Раньше шёл старый протокол, и галочка ставилась, как только данные ушли
    /// на свою рацию, а не когда файл дошёл.
    func sendFile(fileName: String, fileType: Int, data: Data, destMac: Data? = nil, destName: String? = nil) {
        fileSessionCounter += 1
        let sessionId = fileSessionCounter & 0xFF
        let dest = destMac ?? Data([0, 0])

        // Имя — до 19 байт, расширение сохраняем
        var shortName = fileName
        if shortName.utf8.count > 19 {
            let ext = (fileName as NSString).pathExtension
            let base = (fileName as NSString).deletingPathExtension
            if !ext.isEmpty && ext.count < 6 {
                shortName = String(base.prefix(19 - ext.count - 1)) + "." + ext
            } else {
                shortName = String(fileName.prefix(19))
            }
        }

        var header = Data(count: 28)
        header[0] = BLECmd.fileUploadStart
        header[1] = UInt8(fileType & 0xFF)
        header[2] = dest.count > 0 ? dest[0] : 0
        header[3] = dest.count > 1 ? dest[1] : 0
        header[4] = UInt8(data.count & 0xFF)
        header[5] = UInt8((data.count >> 8) & 0xFF)
        header[6] = UInt8((data.count >> 16) & 0xFF)
        header[7] = UInt8((data.count >> 24) & 0xFF)
        for (i, b) in Array(shortName.utf8.prefix(20)).enumerated() { header[8 + i] = b }
        bleManager.send(header)

        let totalChunks = (data.count + 119) / 120
        let transfer = FileTransfer(
            sessionId: sessionId, fileName: fileName, fileType: fileType,
            totalSize: data.count, chunksTotal: totalChunks, chunksDone: 0,
            isOutgoing: true, status: .transferring,
            timeMs: Int64(Date().timeIntervalSince1970 * 1000),
            data: data, destMac: destMac, destName: destName
        )
        let uid = transfer.uid
        // Голосовые (0x04) и адресный PTT (0x05) в «Файлах» не показываем — как Android
        appState.fileTransfers.insert(transfer, at: 0)

        Task { @MainActor [weak self] in
            try? await Task.sleep(nanoseconds: 50_000_000)   // рации — выделить буфер
            var offset = 0
            while offset < data.count {
                guard let self = self else { return }
                let len = min(120, data.count - offset)
                var chunk = Data([BLECmd.fileUploadData])
                chunk.append(data[offset..<offset + len])
                self.bleManager.send(chunk)
                offset += len
                try? await Task.sleep(nanoseconds: 50_000_000)
            }
            // Ждём итога от рации; нет ответа за 120 с — не доставлено
            let deadline = Date().addingTimeInterval(120)
            while Date() < deadline {
                try? await Task.sleep(nanoseconds: 500_000_000)
                guard let self = self,
                      let t = self.appState.fileTransfers.first(where: { $0.uid == uid }) else { return }
                if t.status != .transferring { return }
            }
            guard let self = self,
                  let i = self.appState.fileTransfers.firstIndex(where: { $0.uid == uid }),
                  self.appState.fileTransfers[i].status == .transferring else { return }
            self.appState.fileTransfers[i].status = .failed
            self.markVoiceStatus(fileType: fileType, ok: false)
        }
    }

    /// «Повторить отправку» из подробностей файла — тому же адресату
    func resendFile(_ t: FileTransfer) {
        guard let data = t.loadData(), let dest = t.destMac else { return }
        appState.fileTransfers.removeAll { $0.uid == t.uid }
        sendFile(fileName: t.fileName, fileType: t.fileType, data: data, destMac: dest, destName: t.destName)
    }

    /// Итог отправки от рации: [0x32, статус, сессия]. 0 принят, 1 занята,
    /// 2 отправляется, 3 доставлен, 4 не доставлен, 5 не хватило памяти,
    /// 6 разослан всем (подтверждать некому, с прошивки 4.4.35).
    private func handleFileUploadStatus(_ data: Data) {
        guard data.count >= 3 else { return }
        let status = Int(data[1])
        log.info("UPLOAD_STATUS status=\(status) session=\(Int(data[2]))")
        guard let i = appState.fileTransfers.lastIndex(where: { $0.isOutgoing && $0.status == .transferring })
        else { return }
        let fileType = appState.fileTransfers[i].fileType
        switch status {
        case 1:
            appState.fileTransfers[i].status = .failed
            appState.statusMessage = "Устройство занято"
            markVoiceStatus(fileType: fileType, ok: false)
        case 3:
            appState.fileTransfers[i].status = .done
            appState.fileTransfers[i].chunksDone = appState.fileTransfers[i].chunksTotal
            markVoiceStatus(fileType: fileType, ok: true)
        case 4:
            appState.fileTransfers[i].status = .failed
            markVoiceStatus(fileType: fileType, ok: false)
        case 6:
            appState.fileTransfers[i].status = .done
            appState.fileTransfers[i].chunksDone = appState.fileTransfers[i].chunksTotal
            markVoiceStatus(fileType: fileType, ok: true, broadcast: true)
        case 5:
            appState.fileTransfers[i].status = .failed
            appState.statusMessage = "Файл слишком большой"
            markVoiceStatus(fileType: fileType, ok: false)
        default:
            break
        }
    }

    /// Голосовое в чате — тот же файл: его судьбу переносим на сообщение.
    private func markVoiceStatus(fileType: Int, ok: Bool, broadcast: Bool = false) {
        guard fileType == 0x04,
              let i = appState.messages.lastIndex(where: { $0.isOutgoing && $0.isVoice && $0.status == .sending })
        else { return }
        appState.messages[i].status = !ok ? .failed : (broadcast ? .sent : .delivered)
    }

    private func addRecentCall(deviceId: String, callSign: String, isOutgoing: Bool, callType: String) {
        let call = RecentCall(deviceId: deviceId, callSign: callSign,
                              isOutgoing: isOutgoing, callType: callType)
        // Без повторов, как в Android: один абонент — одна строка, свежая сверху
        appState.recentCalls.removeAll { $0.deviceId == deviceId && $0.callType == callType }
        appState.recentCalls.insert(call, at: 0)
        if appState.recentCalls.count > 20 {
            appState.recentCalls = Array(appState.recentCalls.prefix(20))
        }
    }

    /// Проиграть запись Codec2 (голосовое из чата, адресный PTT): кусками по 32 байта
    func playVoice(_ data: Data) {
        #if DEBUG
        print("MTXDBG playVoice bytes=\(data.count) nonzero=\(data.filter { $0 != 0 }.count)")
        #endif
        audioEngine.startPlayback()
        audioEngine.rogerBeep = appState.rogerBeep
        let bytes = [UInt8](data)
        var off = 0
        while off + 32 <= bytes.count {
            audioEngine.playEncodedPacket(Data(bytes[off..<off + 32]))
            off += 32
        }
        audioEngine.playEncodedPacket(Data(), isLastPacket: true)
    }

    /// Повтор по ✗: текст — заново, голосовое — заново файлом
    func retry(_ msg: ChatMessage) {
        appState.messages.removeAll { $0.id == msg.id }
        if msg.isVoice, let data = msg.voiceData,
           let d = msg.destId, let b0 = UInt8(d.prefix(2), radix: 16), let b1 = UInt8(d.suffix(2), radix: 16) {
            sendFile(fileName: "voice_retry.c2", fileType: 0x04, data: data,
                     destMac: Data([b0, b1]), destName: msg.destName)
            var again = msg
            again.status = .sending
            appState.messages.append(again)
        } else if !msg.isVoice {
            sendTextMessage(msg.text, destId: msg.destId, destName: msg.destName)
        }
    }

    // MARK: - Settings

    /// Настройки с рации. Позывной — как в Android: приоритет у сохранённого в
    /// телефоне (и он досылается на рацию, если там другой), иначе берём с рации.
    private func handleSettingsResp(_ data: Data) {
        guard data.count > 1,
              let obj = try? JSONSerialization.jsonObject(with: data.dropFirst()) as? [String: Any]
        else { return }
        // Только при смене: опрос идёт регулярно и не должен перебивать то,
        // что человек сейчас выставляет на экране настроек.
        if let p = obj["tx_power"] as? Int, appState.deviceTxPower != p { appState.deviceTxPower = p }
        if let dc = obj["duty_cycle"] as? Bool, appState.deviceDutyCycle != dc { appState.deviceDutyCycle = dc }
        if let bs = obj["beacon_interval"] as? Int, appState.deviceBeaconSec != bs { appState.deviceBeaconSec = bs }
        let deviceCs = obj["callsign"] as? String ?? ""
        let saved = UserDefaults.standard.string(forKey: "my_callsign") ?? ""
        if !saved.isEmpty {
            appState.callSign = saved
            if deviceCs != saved {
                bleManager.sendSettings(json: "{\"callsign\":\"\(saved)\"}")
            }
        } else if !deviceCs.isEmpty && !deviceCs.hasPrefix("TX-") {
            appState.callSign = deviceCs
            UserDefaults.standard.set(deviceCs, forKey: "my_callsign")
        }
    }

    func setRepeater(enable: Bool, ssid: String = "", password: String = "", ip: String = "") {
        bleManager.sendRepeaterConfig(enable: enable, ssid: ssid, password: password, ip: ip)
    }

    func setCallSign(_ cs: String) {
        appState.callSign = cs
        UserDefaults.standard.set(cs, forKey: "my_callsign")
        bleManager.sendSettings(json: "{\"callsign\":\"\(cs)\"}")
    }

    func applySettings(json: String) {
        bleManager.sendSettings(json: json)
    }

    // MARK: - Encryption

    func setEncryptionKey(_ passphrase: String) {
        guard passphrase.count >= 8 else { return }
        bleManager.sendSetKey(passphrase: passphrase)
    }

    func clearEncryptionKey() {
        bleManager.sendClearKey()
        appState.keyFingerprint = ""
        appState.alienPackets = 0
    }

    func setHearPlaintext(_ enable: Bool) {
        appState.hearPlaintext = enable
        bleManager.sendHearPlaintext(enable)
    }

    // MARK: - Channel All

    func setChannelAll(_ ch: Int) {
        bleManager.sendSetChannelAll(ch)
    }

    // MARK: - Channel All

    func scanChannels() {
        bleManager.send(Data([BLECmd.scanChannels]))
    }

    // MARK: - Peers

    /// Вместе с абонентами — и последние вызовы, как в Android
    func clearPeers() {
        appState.peers.removeAll()
        appState.recentCalls.removeAll()
    }

    // MARK: - Clear chat

    func clearChat() {
        appState.messages.removeAll()
        appState.unreadMessages = 0
    }

    // MARK: - Notifications

    private func setupNotifications() {
        UNUserNotificationCenter.current().delegate = notificationDelegate
        notificationDelegate.onOpenChat = { [weak self] in self?.appState.requestedTab = 1 }
        UNUserNotificationCenter.current().requestAuthorization(options: [.alert, .sound, .badge]) { granted, _ in
            if granted {
                self.log.info("Notifications authorized")
            }
        }
    }

    private func postCallNotification(call: IncomingCall) {
        let content = UNMutableNotificationContent()
        let typeLabel: String
        switch call.callType {
        case .all: typeLabel = "Общий вызов"
        case .private: typeLabel = "Личный вызов"
        case .group: typeLabel = "Групповой вызов"
        case .emergency: typeLabel = "SOS"
        }
        content.title = typeLabel
        content.body = call.callSign
        content.sound = .defaultCritical
        content.interruptionLevel = .timeSensitive
        content.threadIdentifier = "meshtrx-call"

        let request = UNNotificationRequest(
            identifier: "call-\(call.callSeq)",
            content: content,
            trigger: nil
        )
        UNUserNotificationCenter.current().add(request)
    }

    private func postMessageNotification(senderName: String, text: String, isPrivate: Bool) {
        let mode = appState.notifyMode
        guard mode > 0 else { return }
        if mode == 1 && !isPrivate { return }

        let content = UNMutableNotificationContent()
        // Заголовки — как в Android: сразу видно, лично это или в общий чат
        content.title = "\(senderName) — \(isPrivate ? "лично вам" : "в общий чат")"
        content.body = text
        if isPrivate {
            content.sound = .default
        }
        content.threadIdentifier = isPrivate ? "meshtrx-private" : "meshtrx-broadcast"
        content.userInfo = ["open": "chat"]

        // Одно уведомление на отправителя, отдельно личное и общее, как в Android:
        // новое сообщение обновляет его, а не плодит ленту
        let request = UNNotificationRequest(
            identifier: "msg-\(isPrivate ? "p" : "a")-\(senderName)",
            content: content,
            trigger: nil
        )
        UNUserNotificationCenter.current().add(request)
    }

    // MARK: - Updates

    /// Тот же файл, что у Android: https://meshtrx.ru/latest.json, где есть
    /// {app, firmware, ios}. Раньше iPhone спрашивал несуществующий адрес
    /// /api/latest.json с другими ключами и обновлений не видел никогда.
    func checkForUpdates(silent: Bool = false) {
        if !silent { appState.updateStatus = "Проверяю..." }
        Task {
            do {
                let url = URL(string: "https://meshtrx.ru/latest.json")!
                let (data, _) = try await URLSession.shared.data(from: url)
                guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else { return }
                let latestApp = (json["ios"] as? [String: Any])?["version"] as? String ?? ""
                let latestFw = (json["firmware"] as? [String: Any])?["version"] as? String ?? ""
                let currentApp = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
                let currentFw = appState.firmwareVersion
                UserDefaults.standard.set(Date().timeIntervalSince1970, forKey: "last_update_check")

                var lines: [String] = []
                var available = false
                if !latestApp.isEmpty && Self.isNewer(latestApp, than: currentApp) {
                    lines.append("Приложение: \(currentApp) → \(latestApp)")
                    available = true
                } else {
                    lines.append("Приложение \(currentApp) — последняя")
                }
                if !currentFw.isEmpty && !latestFw.isEmpty && Self.isNewer(latestFw, than: currentFw) {
                    lines.append("Прошивка: \(currentFw) → \(latestFw)")
                    available = true
                } else if !currentFw.isEmpty {
                    lines.append("Прошивка \(currentFw) — последняя")
                }
                appState.updateAvailable = available
                appState.updateStatus = lines.joined(separator: "\n")
            } catch {
                if !silent { appState.updateStatus = "Нет подключения к интернету" }
                appState.updateAvailable = false
            }
        }
    }

    /// Раз в сутки — сама, как в Android
    func checkForUpdatesIfDue() {
        let last = UserDefaults.standard.double(forKey: "last_update_check")
        if Date().timeIntervalSince1970 - last > 24 * 3600 { checkForUpdates(silent: true) }
    }

    /// «4.4.10» новее «4.4.9»: сравниваем по числам, а не строкой
    static func isNewer(_ a: String, than b: String) -> Bool {
        let pa = a.split(separator: ".").map { Int($0) ?? 0 }
        let pb = b.split(separator: ".").map { Int($0) ?? 0 }
        for i in 0..<max(pa.count, pb.count) {
            let x = i < pa.count ? pa[i] : 0, y = i < pb.count ? pb[i] : 0
            if x != y { return x > y }
        }
        return false
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
        #if DEBUG
        if data[0] != 0x17 { print(String(format: "MTXDBG ble cmd=0x%02X len=%d", data[0], data.count)) }
        #endif
        let cmd = data[0]

        switch cmd {
        case BLECmd.audioRx:
            handleAudioRx(data)
        case BLECmd.statusUpdate:
            handleStatusUpdate(data)
        case BLECmd.recvMessage:
            handleRecvMessage(data)
        case BLECmd.messageAck:
            handleMessageAck(data)
        case BLECmd.peerSeen:
            handlePeerSeen(data)
        case BLECmd.getLocation:
            sendGpsToDevice()
        case BLECmd.settingsResp:
            handleSettingsResp(data)
        case BLECmd.pinResult:
            handlePinResult(data)
        case BLECmd.fileUploadStatus:
            handleFileUploadStatus(data)
        case BLECmd.fileProgress:
            handleFileProgress(data)
        case BLECmd.fileRecv:
            handleFileRecv(data)
        case BLECmd.fileData:
            handleFileData(data)
        case BLECmd.incomingCall:
            handleIncomingCall(data)
        case BLECmd.callStatus:
            // 3 — тайм-аут, 4 — вызов отменён: окно вызова закрываем, как Android
            if data.count >= 2 && (data[1] == 0 || data[1] >= 3) {
                appState.callActive = false
                if data[1] >= 3 { appState.incomingCall = nil }
            }
        case BLECmd.keyState:
            handleKeyState(data)
        case BLECmd.fwVersion:
            handleFwVersion(data)
        case BLECmd.bandInfo:
            handleBandInfo(data)
        case BLECmd.cryptoAlien:
            handleCryptoAlien(data)
        case BLECmd.scanResult:
            handleScanResult(data)
        default:
            break
        }
    }

    // MARK: - Audio RX

    private var rxIdleTask: Task<Void, Never>?

    /// Голос с рации: [0x02, флаги, отправитель(2), Codec2 32 байта] — 36 байт.
    /// Раньше iPhone ждал 68 (или 66) и всё принятое выбрасывал.
    private func handleAudioRx(_ data: Data) {
        guard data.count >= 36 else { return }
        let flags = Int(data[1])
        let senderId = String(format: "%02X%02X", data[2], data[3])
        let audio = Data(data[4..<36])
        let isLast = (flags & 0x02) != 0   // PKT_FLAG_PTT_END

        let listen = appState.listenMode == .all
        let active = appState.callActive
        if listen || active {
            audioEngine.rogerBeep = appState.rogerBeep
            audioEngine.playEncodedPacket(audio, isLastPacket: isLast)
        }

        // Кто говорит — для строки статуса. Гаснет с концом фразы или через 5 с.
        if senderId != "0000" {
            let name = appState.peers.first { $0.deviceId.hasSuffix(senderId) }?.callSign
            appState.rxCallSign = isLast ? nil : (name?.isEmpty == false ? name : "TX-\(senderId)")
            rxIdleTask?.cancel()
            if !isLast {
                rxIdleTask = Task { @MainActor [weak self] in
                    try? await Task.sleep(nanoseconds: 5_000_000_000)
                    if !Task.isCancelled { self?.appState.rxCallSign = nil }
                }
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
        // Байт 5 — напряжение батареи рации в десятых вольта (как читает Android)
        if data.count >= 6, data[5] > 0 { appState.batteryVoltage = Float(data[5]) / 10 }
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

        // После отправителя прошивка кладёт: признак шифрования (4.4.27+),
        // признак «лично нам» (4.4.31+) и — у сообщений, пролежавших на рации
        // без телефона, — возраст в секундах, uint32 LE (4.4.32+). Раньше эти
        // байты читались как канал и адрес получателя, и пометки выходили
        // наугад. Старая прошивка признаков не шлёт: тогда без пометки.
        var isEncrypted = true
        if textEnd + 3 < data.count {
            isEncrypted = data[textEnd + 3] == 1 || appState.keyFingerprint.isEmpty
        }
        let isPrivate = textEnd + 4 < data.count && data[textEnd + 4] == 1
        var ageSec: Int64 = 0
        if textEnd + 8 < data.count {
            ageSec = Int64(data[textEnd + 5]) | Int64(data[textEnd + 6]) << 8 |
                     Int64(data[textEnd + 7]) << 16 | Int64(data[textEnd + 8]) << 24
        }

        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let rxAt = now - ageSec * 1000
        let fmt = DateFormatter()
        fmt.dateFormat = "HH:mm"

        let msg = ChatMessage(
            id: now, text: text, isOutgoing: false, senderId: senderId,
            senderName: senderName, rssi: rssiVal, status: .delivered,
            time: fmt.string(from: Date(timeIntervalSince1970: Double(rxAt) / 1000)),
            timeMs: rxAt,
            encrypted: isEncrypted
        )
        appState.messages.append(msg)
        appState.unreadMessages += 1

        postMessageNotification(senderName: senderName, text: text, isPrivate: isPrivate)
    }

    // MARK: - Encryption handlers

    /// [0x38, есть ключ, отпечаток(4), слушать открытый эфир]. Раньше отпечаток
    /// читался с байта «есть ключ», и без ключа на экране стоял мусор.
    private func handleKeyState(_ data: Data) {
        guard data.count >= 6 else { return }
        let has = data[1] == 1
        let fp = String(data: data[2..<6], encoding: .ascii) ?? ""
        appState.keyFingerprint = has ? fp : ""
        if data.count >= 7 { appState.hearPlaintext = data[6] == 1 }
        log.info("Key: \(has ? "есть, отпечаток \(fp)" : "снят")")
    }

    private func handleFwVersion(_ data: Data) {
        if data.count > 1 {
            let ver = String(data: data[1...], encoding: .utf8)?
                .trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
            appState.firmwareVersion = ver
            log.info("Firmware: \(ver)")
        }
    }

    /// [0x3E, база кГц u32 LE, шаг кГц u16 LE, каналов, макс дБм, мин дБм]
    private func handleBandInfo(_ data: Data) {
        let b = [UInt8](data)
        guard b.count >= 10 else { return }
        let base = Int(b[1]) | Int(b[2]) << 8 | Int(b[3]) << 16 | Int(b[4]) << 24
        let step = Int(b[5]) | Int(b[6]) << 8
        let count = Int(b[7])
        guard (1...64).contains(count) else { return }
        appState.band = Band(baseKhz: base, stepKhz: step, count: count,
                             maxDbm: Int(Int8(bitPattern: b[8])), minDbm: Int(Int8(bitPattern: b[9])))
        log.info("Band: \(base) kHz + n*\(step), \(count) каналов, до \(b[8]) dBm")
    }

    private func handleCryptoAlien(_ data: Data) {
        if data.count >= 3 {
            let count = Int(data.getUInt16LE(at: 1))
            appState.alienPackets = count
        }
    }

    /// [0x35, лучший канал, уровень шума всех каналов сетки] — уровни знаковые.
    /// Раньше байты читались парами «канал–шум», и результат был бессмыслен.
    private func handleScanResult(_ data: Data) {
        guard data.count >= 3 else { return }
        let best = Int(data[1])
        let levels = data[2...].map { Int(Int8(bitPattern: $0)) }
        let quiet = levels.enumerated().sorted { $0.element < $1.element }.prefix(3)
            .map { "CH \($0.offset) (\($0.element) dBm)" }.joined(separator: ", ")
        var text = "Тише всего: \(quiet)"
        if let noisy = levels.enumerated().max(by: { $0.element < $1.element }) {
            text += "\nШумнее всего: CH \(noisy.offset) (\(noisy.element) dBm)"
        }
        text += "\nЧтобы перевести всю группу разом — «Сменить канал у всех»"
        appState.scanResultText = text
        if best < levels.count { appState.scanBestChannel = best }
    }

    // MARK: - Peer discovery

    private func handlePeerSeen(_ data: Data) {
        guard data.count >= 28 else { return }
        let id = String(format: "%02X%02X%02X%02X", data[1], data[2], data[3], data[4])
        let csData = data[5..<14]
        var cs = String(data: csData, encoding: .utf8)?.trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
        if cs.isEmpty { cs = "TX-\(id.suffix(4))" }      // запасной позывной, как в Android
        let isRepeater = (data[27] & 0x08) != 0

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
            lat: lat, lon: lon, isRepeater: isRepeater
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

    /// Вызов от рации: [0x1F, тип, id(4), позывной(9), широта(4), долгота(4), seq]
    /// — 24 байта (call_manager.cpp). Раньше разбиралась старая раскладка
    /// с 2-байтным id: позывной выходил мусором, а seq — чужим байтом.
    private func handleIncomingCall(_ data: Data) {
        guard data.count >= 24 else { return }
        let callType = CallType.fromCode(Int(data[1]))
        let senderId = String(format: "%02X%02X%02X%02X", data[2], data[3], data[4], data[5])
        var callSign = String(data: data[6..<15], encoding: .utf8)?
            .trimmingCharacters(in: CharacterSet(charactersIn: "\0")) ?? ""
        if callSign.isEmpty { callSign = "TX-\(senderId.suffix(4))" }
        let latE7 = data.getInt32LE(at: 15)
        let lonE7 = data.getInt32LE(at: 19)
        let hasPos = latE7 != 0 || lonE7 != 0
        let callSeq = Int(data[23])

        // Как в Android: личный и SOS — всегда, общий и групповой — только
        // в режиме «Слушать всех». И не перебиваем уже открытый вызов.
        let show = callType == .emergency || callType == .`private` || appState.listenMode == .all
        guard show, appState.incomingCall == nil else { return }

        let call = IncomingCall(
            callType: callType,
            senderId: senderId, callSign: callSign,
            lat: hasPos ? Double(latE7) / 1e7 : nil,
            lon: hasPos ? Double(lonE7) / 1e7 : nil,
            callSeq: callSeq, rssi: appState.rssi
        )
        appState.incomingCall = call
        // Личный вызов сам выбирает адресата — отвечать будем ему, как в Android
        if callType == .`private` {
            appState.pttTargetId = String(senderId.suffix(4))
            appState.pttTargetName = callSign
        }
        addRecentCall(deviceId: senderId, callSign: callSign, isOutgoing: false,
                      callType: call.callType == .emergency ? "SOS" : call.callType == .`private` ? "PRIVATE" : "ALL")
        postCallNotification(call: call)
    }

    // MARK: - PIN

    private func handlePinResult(_ data: Data) {
        guard data.count >= 2 else { return }
        if data[1] == 1 {
            log.info("PIN accepted")
            if let addr = bleManager.connectedDeviceIdentifier {
                authorizeDevice(addr)
            }
            bleManager.markConnected()
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
        incomingFileSender = ""
        incomingFileName = ""

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

        if incomingFileOffset >= incomingFileSize, let fileData = incomingFileBuffer {
            incomingFileBuffer = nil
            let sender = incomingFileSender
            var senderName: String? = nil
            if !sender.isEmpty {
                senderName = appState.peers.first { $0.deviceId.hasSuffix(sender) }?.callSign
                if senderName?.isEmpty != false { senderName = "TX-\(sender)" }
            }
            switch incomingFileType {
            case 0x05:
                // Адресная PTT-передача: проигрываем сразу, в «Файлы» не кладём
                playVoice(fileData)
            case 0x04:
                // Голосовое сообщение — в чат, как в Android
                let secs = fileData.count / 400      // 3200 бит/с = 400 байт в секунду
                let now = Int64(Date().timeIntervalSince1970 * 1000)
                let fmt = DateFormatter(); fmt.dateFormat = "HH:mm"
                var msg = ChatMessage(
                    id: now, text: "\u{1F3A4} \(secs)s", isOutgoing: false,
                    senderId: sender.isEmpty ? "??" : sender,
                    senderName: senderName ?? "", rssi: appState.rssi, status: .delivered,
                    time: fmt.string(from: Date()), timeMs: now, isVoice: true
                )
                msg.voiceData = fileData
                appState.messages.append(msg)
                appState.unreadMessages += 1
            default:
                var name = incomingFileName
                if name.isEmpty {
                    name = incomingFileType == 0x01 ? "photo_received.jpg"
                         : incomingFileType == 0x02 ? "text_received.txt" : "file_received.bin"
                } else if !name.contains(".") {
                    name += incomingFileType == 0x01 ? ".jpg" : incomingFileType == 0x02 ? ".txt" : ".bin"
                }
                fileSessionCounter += 1
                var transfer = FileTransfer(
                    sessionId: fileSessionCounter & 0xFF,
                    fileName: name,
                    fileType: incomingFileType,
                    totalSize: incomingFileSize,
                    chunksTotal: 1, chunksDone: 1,
                    isOutgoing: false, status: .done,
                    timeMs: Int64(Date().timeIntervalSince1970 * 1000),
                    data: fileData
                )
                transfer.destName = senderName      // у входящего здесь — отправитель
                appState.fileTransfers.insert(transfer, at: 0)
                log.info("File received: \(name)")
            }
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

// MARK: - Хранение между запусками
//
// Раньше не хранилось ничего: после перезапуска пропадали переписка, абоненты,
// последние вызовы, файлы, позывной и настройки звука. Настройки — в
// UserDefaults, данные — JSON-файлами в Application Support (файлы бывают по
// 100 КБ, в UserDefaults им не место). Сроки хранения — как в Android.

private enum Store {
    static let d = UserDefaults.standard
    static var dir: URL {
        let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
        let u = base.appendingPathComponent("meshtrx", isDirectory: true)
        try? FileManager.default.createDirectory(at: u, withIntermediateDirectories: true)
        return u
    }
    static func save<T: Encodable>(_ v: T, _ name: String) {
        guard let data = try? JSONEncoder().encode(v) else { return }
        try? data.write(to: dir.appendingPathComponent(name), options: .atomic)
    }
    static func load<T: Decodable>(_ t: T.Type, _ name: String) -> T? {
        guard let data = try? Data(contentsOf: dir.appendingPathComponent(name)) else { return nil }
        return try? JSONDecoder().decode(t, from: data)
    }
}

extension MeshTRXController {
    func loadPersisted() {
        let d = Store.d
        if let cs = d.string(forKey: "my_callsign"), !cs.isEmpty { appState.callSign = cs }
        if d.object(forKey: "rx_volume") != nil { appState.rxVolume = d.integer(forKey: "rx_volume") }
        if d.object(forKey: "roger_beep") != nil { appState.rogerBeep = d.bool(forKey: "roger_beep") }
        if d.object(forKey: "ptt_rms") != nil { appState.squelchThreshold = d.integer(forKey: "ptt_rms") }
        if d.object(forKey: "vox_threshold") != nil { appState.voxThreshold = d.integer(forKey: "vox_threshold") }
        if d.object(forKey: "vox_hangtime") != nil { appState.voxHangtimeMs = d.integer(forKey: "vox_hangtime") }
        if d.object(forKey: "notify_mode") != nil { appState.notifyMode = d.integer(forKey: "notify_mode") }
        if d.object(forKey: "peer_timeout_min") != nil { appState.peerTimeoutMin = d.integer(forKey: "peer_timeout_min") }
        if d.object(forKey: "file_history_days") != nil { appState.fileHistoryDays = d.integer(forKey: "file_history_days") }

        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let day: Int64 = 24 * 3600 * 1000
        if let m = Store.load([ChatMessage].self, "messages.json") {
            let keep = Int64(appState.messageHistoryDays) * day
            appState.messages = m.filter { now - $0.timeMs < keep }
        }
        if let p = Store.load([Peer].self, "peers.json") {
            let keep = Int64(appState.peerTimeoutMin) * 60_000
            appState.peers = p.filter { now - $0.lastSeenMs < keep }
        }
        if let r = Store.load([RecentCall].self, "recent_calls.json") { appState.recentCalls = r }
        if let f = Store.load([FileTransfer].self, "files.json") {
            let keep = Int64(appState.fileHistoryDays) * day
            // Незавершённая передача после перезапуска уже не завершится
            appState.fileTransfers = f.filter { now - $0.timeMs < keep }.map {
                var t = $0; if t.status == .transferring || t.status == .pending { t.status = .failed }; return t
            }
        }
    }

    func setupPersistence() {
        // Звук и VOX — применяем сразу, как только человек двинул ползунок
        appState.$rxVolume.sink { [weak self] v in
            self?.audioEngine.volumeBoost = Float(v) / 100
            Store.d.set(v, forKey: "rx_volume")
        }.store(in: &cancellables)
        appState.$rogerBeep.sink { [weak self] v in
            self?.audioEngine.rogerBeep = v
            Store.d.set(v, forKey: "roger_beep")
        }.store(in: &cancellables)
        appState.$squelchThreshold.sink { [weak self] v in
            self?.audioEngine.squelchThreshold = v
            Store.d.set(v, forKey: "ptt_rms")
        }.store(in: &cancellables)
        appState.$voxThreshold.sink { [weak self] v in
            self?.voxEngine.threshold = v
            Store.d.set(v, forKey: "vox_threshold")
        }.store(in: &cancellables)
        appState.$voxHangtimeMs.sink { [weak self] v in
            self?.voxEngine.hangtimeMs = Int64(v)
            Store.d.set(v, forKey: "vox_hangtime")
        }.store(in: &cancellables)
        appState.$notifyMode.sink { Store.d.set($0, forKey: "notify_mode") }.store(in: &cancellables)
        appState.$peerTimeoutMin.sink { Store.d.set($0, forKey: "peer_timeout_min") }.store(in: &cancellables)
        appState.$fileHistoryDays.sink { Store.d.set($0, forKey: "file_history_days") }.store(in: &cancellables)

        // Данные — с задержкой, чтобы не писать на диск на каждое обновление
        let delay = DispatchQueue.main
        appState.$messages.dropFirst().debounce(for: .seconds(1), scheduler: delay)
            .sink { Store.save($0, "messages.json") }.store(in: &cancellables)
        appState.$peers.dropFirst().debounce(for: .seconds(2), scheduler: delay)
            .sink { Store.save($0, "peers.json") }.store(in: &cancellables)
        appState.$recentCalls.dropFirst().debounce(for: .seconds(1), scheduler: delay)
            .sink { Store.save($0, "recent_calls.json") }.store(in: &cancellables)
        appState.$fileTransfers.dropFirst().debounce(for: .seconds(1), scheduler: delay)
            .sink { Store.save($0, "files.json") }.store(in: &cancellables)
    }
}

// MARK: - Нажатие на уведомление

/// Нажатие на уведомление о сообщении открывает чат. Раньше делегата не было,
/// и нажатие просто запускало приложение на той вкладке, где его оставили.
final class NotificationDelegate: NSObject, UNUserNotificationCenterDelegate {
    var onOpenChat: (() -> Void)?

    func userNotificationCenter(_ center: UNUserNotificationCenter,
                                didReceive response: UNNotificationResponse,
                                withCompletionHandler completionHandler: @escaping () -> Void) {
        if response.notification.request.content.userInfo["open"] as? String == "chat" {
            DispatchQueue.main.async { self.onOpenChat?() }
        }
        completionHandler()
    }

    // Приложение открыто — баннер не нужен: сообщение и так видно (как в Android)
    func userNotificationCenter(_ center: UNUserNotificationCenter,
                                willPresent notification: UNNotification,
                                withCompletionHandler completionHandler: @escaping (UNNotificationPresentationOptions) -> Void) {
        completionHandler([])
    }
}
