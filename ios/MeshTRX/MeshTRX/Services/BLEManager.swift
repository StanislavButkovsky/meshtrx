import Foundation
import CoreBluetooth
import os.log

class BLEManager: NSObject, ObservableObject {

    // MARK: - Callbacks

    var onConnected: (() -> Void)?
    var onDisconnected: (() -> Void)?
    var onDataReceived: ((Data) -> Void)?
    var onScanResult: ((CBPeripheral, Int) -> Void)?
    var onNeedPin: (() -> Void)?

    // MARK: - State

    @Published private(set) var isScanning = false

    private var centralManager: CBCentralManager!
    private var connectedPeripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?  // write (to device)
    private var txCharacteristic: CBCharacteristic?  // notify (from device)
    private var mtuSize: Int = 20

    // Reconnect
    private var lastPeripheralIdentifier: UUID?
    private var reconnectAttempts = 0
    private let maxReconnectAttempts = 10
    private let reconnectDelays: [TimeInterval] = [3, 5, 7, 10, 10, 10, 10, 10, 10, 10]
    private var reconnectTimer: Timer?
    private var wasConnected = false

    // Connect timeout
    private var connectTimer: Timer?
    private let connectTimeout: TimeInterval = 10

    // Scan timeout
    private var scanTimer: Timer?
    private let scanTimeout: TimeInterval = 10

    private let log = Logger(subsystem: "com.meshtrx.app", category: "BLE")

    // MARK: - Init

    override init() {
        super.init()
        centralManager = CBCentralManager(
            delegate: self,
            queue: nil,
            options: [CBCentralManagerOptionRestoreIdentifierKey: "com.meshtrx.ble"]
        )
    }

    // MARK: - Scan

    func startScan() {
        guard centralManager.state == .poweredOn, !isScanning else { return }
        centralManager.scanForPeripherals(
            withServices: [BLEUUID.service],
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false]
        )
        isScanning = true
        log.info("Scan started")

        scanTimer?.invalidate()
        scanTimer = Timer.scheduledTimer(withTimeInterval: scanTimeout, repeats: false) { [weak self] _ in
            self?.stopScan()
        }
    }

    func stopScan() {
        guard isScanning else { return }
        centralManager.stopScan()
        isScanning = false
        scanTimer?.invalidate()
        scanTimer = nil
        log.info("Scan stopped")
    }

    // MARK: - Connect / Disconnect

    func connect(_ peripheral: CBPeripheral) {
        stopScan()
        connectTimer?.invalidate()

        connectedPeripheral = peripheral
        peripheral.delegate = self
        lastPeripheralIdentifier = peripheral.identifier

        centralManager.connect(peripheral, options: nil)
        log.info("Connecting to \(peripheral.name ?? "unknown")")

        connectTimer = Timer.scheduledTimer(withTimeInterval: connectTimeout, repeats: false) { [weak self] _ in
            guard let self = self else { return }
            if self.rxCharacteristic == nil {
                self.log.warning("Connect timeout")
                self.centralManager.cancelPeripheralConnection(peripheral)
            }
        }
    }

    func disconnect() {
        wasConnected = false
        reconnectTimer?.invalidate()
        reconnectTimer = nil
        reconnectAttempts = 0

        if let peripheral = connectedPeripheral {
            centralManager.cancelPeripheralConnection(peripheral)
        }
        cleanup()
    }

    func forgetDevice() {
        disconnect()
        lastPeripheralIdentifier = nil
        UserDefaults.standard.removeObject(forKey: "lastBLEDeviceId")
        UserDefaults.standard.removeObject(forKey: "lastBLEDeviceName")
    }

    var isConnected: Bool {
        connectedPeripheral != nil && rxCharacteristic != nil
    }

    // MARK: - Auto-reconnect

    func autoConnect() {
        guard let idStr = UserDefaults.standard.string(forKey: "lastBLEDeviceId"),
              let uuid = UUID(uuidString: idStr) else { return }
        lastPeripheralIdentifier = uuid
        let known = centralManager.retrievePeripherals(withIdentifiers: [uuid])
        if let peripheral = known.first {
            connect(peripheral)
        } else {
            startScan()
        }
    }

    private func scheduleReconnect() {
        guard reconnectAttempts < maxReconnectAttempts else {
            log.info("Max reconnect attempts reached")
            return
        }
        let delay = reconnectDelays[min(reconnectAttempts, reconnectDelays.count - 1)]
        reconnectAttempts += 1
        log.info("Reconnect attempt \(self.reconnectAttempts) in \(delay)s")

        reconnectTimer?.invalidate()
        reconnectTimer = Timer.scheduledTimer(withTimeInterval: delay, repeats: false) { [weak self] _ in
            self?.autoConnect()
        }
    }

    // MARK: - Send

    @discardableResult
    func send(_ data: Data) -> Bool {
        guard let peripheral = connectedPeripheral,
              let rx = rxCharacteristic else { return false }
        peripheral.writeValue(data, for: rx, type: .withoutResponse)
        return true
    }

    // Convenience senders
    func sendAudioData(_ codec2Data: Data)  { send(BLEPacket.audioTx(codec2Data)) }
    func sendPttStart()                      { send(BLEPacket.pttStart()) }
    func sendPttEnd()                        { send(BLEPacket.pttEnd()) }
    func setChannel(_ ch: Int)               { send(BLEPacket.setChannel(ch)) }
    func sendMessage(seq: Int, text: String) { send(BLEPacket.sendMessage(seq: seq, text: text)) }
    func sendSettings(json: String)          { send(BLEPacket.setSettings(json: json)) }
    func requestSettings()                   { send(BLEPacket.getSettings()) }
    func sendPinCheck(_ pin: Int)            { send(BLEPacket.pinCheck(pin)) }
    func setTxMode(_ mode: TxMode)           { send(BLEPacket.setTxMode(mode)) }

    func sendLocationUpdate(latE7: Int32, lonE7: Int32, altM: Int16) {
        send(BLEPacket.locationUpdate(latE7: latE7, lonE7: lonE7, altM: altM))
    }

    func sendRepeaterConfig(enable: Bool, ssid: String = "", password: String = "", ip: String = "") {
        send(BLEPacket.setRepeater(enable: enable, ssid: ssid, password: password, ip: ip))
    }

    // MARK: - Private

    private func cleanup() {
        connectedPeripheral = nil
        rxCharacteristic = nil
        txCharacteristic = nil
        connectTimer?.invalidate()
        connectTimer = nil
    }

    private func saveDevice(_ peripheral: CBPeripheral) {
        UserDefaults.standard.set(peripheral.identifier.uuidString, forKey: "lastBLEDeviceId")
        UserDefaults.standard.set(peripheral.name ?? "", forKey: "lastBLEDeviceName")
    }
}

// MARK: - CBCentralManagerDelegate

extension BLEManager: CBCentralManagerDelegate {

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        log.info("BLE state: \(central.state.rawValue)")
        if central.state == .poweredOn {
            // Try auto-connect on power on
            autoConnect()
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                         advertisementData: [String: Any], rssi RSSI: NSNumber) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? ""
        guard name.hasPrefix("MeshTRX-") else { return }

        log.info("Found: \(name) RSSI=\(RSSI.intValue)")
        onScanResult?(peripheral, RSSI.intValue)

        // Auto-connect to last known device
        if let lastId = lastPeripheralIdentifier, peripheral.identifier == lastId {
            connect(peripheral)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log.info("Connected to \(peripheral.name ?? "?")")
        connectTimer?.invalidate()
        peripheral.discoverServices([BLEUUID.service])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        log.error("Failed to connect: \(error?.localizedDescription ?? "unknown")")
        cleanup()
        onDisconnected?()
        if wasConnected { scheduleReconnect() }
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        log.info("Disconnected: \(error?.localizedDescription ?? "clean")")
        let shouldReconnect = wasConnected
        cleanup()
        onDisconnected?()
        if shouldReconnect { scheduleReconnect() }
    }

    // MARK: - State Restoration

    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral],
           let peripheral = peripherals.first {
            log.info("Restoring connection to \(peripheral.name ?? "?")")
            connectedPeripheral = peripheral
            peripheral.delegate = self
        }
    }
}

// MARK: - CBPeripheralDelegate

extension BLEManager: CBPeripheralDelegate {

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let service = peripheral.services?.first(where: { $0.uuid == BLEUUID.service }) else { return }
        peripheral.discoverCharacteristics([BLEUUID.rxChar, BLEUUID.txChar], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            if char.uuid == BLEUUID.rxChar {
                rxCharacteristic = char
            } else if char.uuid == BLEUUID.txChar {
                txCharacteristic = char
                peripheral.setNotifyValue(true, for: char)
            }
        }

        // Negotiate MTU
        if #available(iOS 16.0, *) {
            mtuSize = peripheral.maximumWriteValueLength(for: .withoutResponse)
        }

        log.info("Services ready, MTU=\(self.mtuSize)")
        // Request PIN (same flow as Android)
        onNeedPin?()
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard characteristic.uuid == BLEUUID.txChar,
              let data = characteristic.value, !data.isEmpty else { return }
        onDataReceived?(data)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: Error?) {
        if let error = error {
            log.error("Notify error: \(error.localizedDescription)")
        } else {
            log.info("Notify enabled for \(characteristic.uuid)")
        }
    }
}
