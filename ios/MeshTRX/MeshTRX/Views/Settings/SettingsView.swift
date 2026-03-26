import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController

    // Local state for settings form
    @State private var callSign: String = ""
    @State private var selectedChannel: Int = 0
    @State private var txPower: Double = 14
    @State private var dutyCycle: Bool = true
    @State private var beaconIndex: Int = 3
    @State private var peerTimeoutIndex: Int = 2
    @State private var rxVolume: Double = 200
    @State private var squelchThreshold: Double = 0
    @State private var voxThreshold: Double = 800
    @State private var voxHangtime: Double = 800
    @State private var fileHistoryIndex: Int = 2
    @State private var languageIndex: Int = 0

    // Repeater
    @State private var repeaterSsid: String = ""
    @State private var repeaterPass: String = ""
    @State private var repeaterIp: String = ""
    @State private var showRepeaterConfirm: Bool = false

    private var isConnected: Bool { appState.bleState == .connected }

    private let channelList: [String] = (0...22).map {
        String(format: "CH %d — %.2f MHz", $0, 863.150 + Double($0) * 0.300)
    }
    private let beaconOptions = ["Выкл", "1 мин", "3 мин", "5 мин", "15 мин", "30 мин", "1 час"]
    private let beaconSeconds = [0, 60, 180, 300, 900, 1800, 3600]
    private let timeoutOptions = ["15 мин", "30 мин", "1 час", "2 часа", "6 часов", "24 часа"]
    private let timeoutValues = [15, 30, 60, 120, 360, 1440]
    private let historyOptions = ["7 дней", "14 дней", "30 дней", "90 дней", "Без ограничений"]
    private let historyValues = [7, 14, 30, 90, 3650]

    var body: some View {
        NavigationView {
            ZStack {
                AppColors.bgPrimary.ignoresSafeArea()

                Form {
                    connectionSection
                    radioSection
                    audioSection
                    voxSection
                    historySection
                    repeaterSection
                    aboutSection
                }
                .onAppear { UITableView.appearance().backgroundColor = .clear }
            }
            .navigationTitle("Настройки")
            .navigationBarTitleDisplayMode(.inline)
        }
        .onAppear {
            callSign = appState.callSign
            selectedChannel = appState.currentChannel
        }
    }

    // MARK: - Connection

    private var connectionSection: some View {
        Section {
            HStack {
                Circle()
                    .fill(statusColor)
                    .frame(width: 10, height: 10)
                Text(statusText)
                    .foregroundColor(AppColors.textPrimary)
                Spacer()
                if !appState.deviceName.isEmpty {
                    Text(appState.deviceName)
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundColor(AppColors.textDim)
                }
            }

            Button(connectButtonText) {
                switch appState.bleState {
                case .disconnected:
                    controller.startScan()
                case .connected:
                    controller.disconnect()
                case .scanning, .connecting:
                    controller.disconnect()
                }
            }

            Button("Забыть устройство") {
                controller.forgetDevice()
            }
            .foregroundColor(AppColors.redAccent)
        } header: {
            Text("Подключение")
        }
    }

    private var statusColor: Color {
        switch appState.bleState {
        case .connected: return AppColors.greenAccent
        case .scanning, .connecting: return AppColors.amberAccent
        case .disconnected: return AppColors.redAccent
        }
    }

    private var statusText: String {
        switch appState.bleState {
        case .disconnected: return "Отключено"
        case .scanning: return "Поиск..."
        case .connecting: return "Подключение..."
        case .connected: return "Подключено"
        }
    }

    private var connectButtonText: String {
        switch appState.bleState {
        case .disconnected: return "Подключить"
        case .scanning, .connecting: return "Отмена"
        case .connected: return "Отключить"
        }
    }

    // MARK: - Radio

    private var radioSection: some View {
        Section {
            HStack {
                Text("Позывной")
                    .foregroundColor(AppColors.textPrimary)
                TextField("Callsign", text: $callSign)
                    .multilineTextAlignment(.trailing)
                    .foregroundColor(AppColors.textSecondary)
            }

            Picker("Канал", selection: $selectedChannel) {
                ForEach(0..<channelList.count, id: \.self) { i in
                    Text(channelList[i])
                        .font(.system(size: 13, design: .monospaced))
                        .tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("TX Power")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text("\(Int(txPower)) dBm")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                    if txPower > 14 {
                        Text("EU!")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundColor(AppColors.amberAccent)
                    }
                }
                Slider(value: $txPower, in: 2...30, step: 1)
                    .tint(AppColors.greenAccent)
            }

            Toggle(isOn: $dutyCycle) {
                Text("Duty Cycle (EU868)")
                    .foregroundColor(AppColors.textPrimary)
            }
            .tint(AppColors.greenAccent)

            Picker("Beacon", selection: $beaconIndex) {
                ForEach(0..<beaconOptions.count, id: \.self) { i in
                    Text(beaconOptions[i]).tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)

            Picker("Таймаут пиров", selection: $peerTimeoutIndex) {
                ForEach(0..<timeoutOptions.count, id: \.self) { i in
                    Text(timeoutOptions[i]).tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)

            Button("Применить") {
                applySettings()
            }
            .disabled(!isConnected)
            .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textDim)
        } header: {
            Text("Радио")
        }
    }

    // MARK: - Audio

    private var audioSection: some View {
        Section {
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Громкость приёма")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text("\(Int(rxVolume))%")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: $rxVolume, in: 50...300, step: 10)
                    .tint(AppColors.greenAccent)
            }

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Шумоподавление PTT")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text(squelchThreshold == 0 ? "Выкл" : "\(Int(squelchThreshold))")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: $squelchThreshold, in: 0...5000, step: 100)
                    .tint(AppColors.greenAccent)
            }
        } header: {
            Text("Аудио")
        }
    }

    // MARK: - VOX

    private var voxSection: some View {
        Section {
            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Порог VOX")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text("\(Int(voxThreshold))")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: $voxThreshold, in: 100...5000, step: 100)
                    .tint(AppColors.greenAccent)
            }

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Hangtime VOX")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text("\(Int(voxHangtime)) мс")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: $voxHangtime, in: 200...3000, step: 100)
                    .tint(AppColors.greenAccent)
            }
        } header: {
            Text("VOX")
        }
    }

    // MARK: - History

    private var historySection: some View {
        Section {
            Picker("История файлов", selection: $fileHistoryIndex) {
                ForEach(0..<historyOptions.count, id: \.self) { i in
                    Text(historyOptions[i]).tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)
        } header: {
            Text("Хранение")
        }
    }

    // MARK: - Repeater

    private var repeaterSection: some View {
        Section {
            TextField("SSID", text: $repeaterSsid)
                .foregroundColor(AppColors.textSecondary)
            SecureField("Пароль", text: $repeaterPass)
                .foregroundColor(AppColors.textSecondary)
            TextField("IP адрес", text: $repeaterIp)
                .foregroundColor(AppColors.textSecondary)

            HStack {
                Button("Включить") {
                    showRepeaterConfirm = true
                }
                .foregroundColor(AppColors.greenAccent)
                .disabled(!isConnected)

                Spacer()

                Button("Выключить") {
                    // Send repeater off
                }
                .foregroundColor(AppColors.redAccent)
                .disabled(!isConnected)
            }
        } header: {
            Text("Ретранслятор")
        }
        .alert("Ретранслятор", isPresented: $showRepeaterConfirm) {
            Button("Активировать", role: .destructive) {
                // Send repeater config
            }
            Button("Отмена", role: .cancel) {}
        } message: {
            Text("Устройство перейдёт в режим ретранслятора. Для возврата потребуется перезагрузка.")
        }
    }

    // MARK: - About

    private var aboutSection: some View {
        Section {
            HStack {
                Text("Версия")
                    .foregroundColor(AppColors.textPrimary)
                Spacer()
                Text("1.0.0")
                    .foregroundColor(AppColors.textDim)
            }
            HStack {
                Text("Сборка")
                    .foregroundColor(AppColors.textPrimary)
                Spacer()
                Text("iOS / SwiftUI")
                    .foregroundColor(AppColors.textDim)
            }
        } header: {
            Text("О приложении")
        }
    }

    // MARK: - Actions

    private func applySettings() {
        let beaconSec = beaconSeconds[beaconIndex]
        appState.peerTimeoutMin = timeoutValues[peerTimeoutIndex]
        appState.fileHistoryDays = historyValues[fileHistoryIndex]
        appState.rxVolume = Int(rxVolume)
        controller.audioEngine.volumeBoost = Float(rxVolume) / 100.0
        controller.audioEngine.squelchThreshold = Int(squelchThreshold)
        controller.voxEngine.threshold = Int(voxThreshold)
        controller.voxEngine.hangtimeMs = Int64(voxHangtime)

        if selectedChannel != appState.currentChannel {
            controller.setChannel(selectedChannel)
        }

        if !callSign.isEmpty {
            controller.setCallSign(callSign)
        }

        var json = "{\"tx_power\":\(Int(txPower)),\"duty_cycle\":\(dutyCycle)"
        json += ",\"beacon_interval\":\(beaconSec)"
        if !callSign.isEmpty {
            json += ",\"callsign\":\"\(callSign)\""
        }
        json += "}"
        controller.applySettings(json: json)
    }
}
