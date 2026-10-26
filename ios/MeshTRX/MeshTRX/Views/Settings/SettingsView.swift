import SwiftUI

struct SettingsView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController

    // Local state for settings form
    @State private var callSign: String = ""
    @State private var selectedChannel: Int = 0
    @State private var txPower: Double = 14
    @State private var dutyCycle: Bool = false   // как в Android: по умолчанию выключен
    @State private var beaconIndex: Int = 3
    /// Ползунок поверх целой настройки в AppState: меняется сразу, без «Применить»
    private func intBinding(_ kp: ReferenceWritableKeyPath<AppState, Int>) -> Binding<Double> {
        Binding(get: { Double(appState[keyPath: kp]) },
                set: { appState[keyPath: kp] = Int($0) })
    }
    @State private var languageIndex: Int = 0

    // Encryption
    @State private var passphrase: String = ""
    // Подтверждения опасных действий, как в Android
    @State private var showChannelAllPicker = false
    @State private var channelAllTarget: Int? = nil
    @State private var confirmClearKey = false
    @State private var confirmClearPeers = false
    @State private var settingsInfo: String? = nil

    // Repeater
    @State private var repeaterSsid: String = ""
    @State private var repeaterPass: String = ""
    @State private var repeaterIp: String = ""
    @State private var showRepeaterConfirm: Bool = false

    private var isConnected: Bool { appState.bleState == .connected }

    // Список каналов — по сетке рации (868 или 433)
    private var channelList: [String] {
        (0..<appState.band.count).map { appState.band.label($0) }
    }
    private let beaconOptions = ["Никогда", "1 мин", "3 мин", "5 мин", "15 мин", "30 мин", "1 час"]
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
                    encryptionSection
                    audioSection
                    voxSection
                    // Порядок — как в Android: история файлов, потом уведомления
                    historySection
                    notificationSection
                    // Выбор языка спрятан: английского перевода у iOS-версии пока
                    // нет, и переключатель ничего не менял. Вернуть вместе с переводом.
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
            loadDeviceSettings()
        }
        // Мощность, Duty Cycle и маяк — такими, как стоят в рации. Раньше экран
        // открывался с 14 дБм, и «Применить» отправляло их обратно.
        .onChange(of: appState.deviceTxPower) { _ in loadDeviceSettings() }
        .onChange(of: appState.deviceDutyCycle) { _ in loadDeviceSettings() }
        .onChange(of: appState.deviceBeaconSec) { _ in loadDeviceSettings() }
        // Поиск свободного канала сам выставляет самый тихий, как в Android
        .onChange(of: appState.scanBestChannel) { best in
            if let best = best { selectedChannel = best }
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

            Button("Новое устройство") {
                controller.forgetDevice()
            }
            .foregroundColor(AppColors.redAccent)

            // Scan results list
            if appState.bleState == .scanning || !appState.scanResults.isEmpty {
                ForEach(appState.scanResults) { device in
                    Button {
                        controller.connect(device.peripheral)
                    } label: {
                        HStack {
                            Image(systemName: "antenna.radiowaves.left.and.right")
                                .foregroundColor(AppColors.greenAccent)
                            Text(device.peripheral.name ?? "MeshTRX")
                                .foregroundColor(AppColors.textPrimary)
                            Spacer()
                            Text("\(device.rssi) dBm")
                                .font(.system(size: 12, design: .monospaced))
                                .foregroundColor(AppColors.textDim)
                        }
                    }
                }
                if appState.bleState == .scanning {
                    HStack {
                        ProgressView()
                            .progressViewStyle(.circular)
                        Text("Поиск устройств...")
                            .font(.system(size: 13))
                            .foregroundColor(AppColors.textMuted)
                            .padding(.leading, 8)
                    }
                }
            }
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
                TextField("Макс 8 символов", text: $callSign)
                    .multilineTextAlignment(.trailing)
                    .foregroundColor(AppColors.textSecondary)
                    .onChange(of: callSign) { v in
                        if v.count > 8 { callSign = String(v.prefix(8)) }   // как в Android
                    }
            }

            // Канал меняется сразу, как в Android, без «Применить»
            Picker("Канал", selection: Binding(
                get: { selectedChannel },
                set: { selectedChannel = $0; if isConnected { controller.setChannel($0) } })) {
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
                    if txPower > 14 && !appState.band.is433 {
                        Text("⚠️EU")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundColor(AppColors.amberAccent)
                    }
                }
                Slider(value: $txPower, in: 1...Double(appState.band.maxDbm), step: 1)
                    .tint(AppColors.greenAccent)
                    .onChange(of: appState.band) { b in
                        if txPower > Double(b.maxDbm) { txPower = Double(b.maxDbm) }
                    }
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

            Picker("Удалять станции через", selection: Binding(
                get: { timeoutValues.firstIndex(of: appState.peerTimeoutMin) ?? 2 },
                set: { appState.peerTimeoutMin = timeoutValues[$0] })) {
                ForEach(0..<timeoutOptions.count, id: \.self) { i in
                    Text(timeoutOptions[i]).tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)

            HStack {
                // Свой список каналов и подтверждение, как в Android: раньше
                // кнопка сразу рассылала канал из поля выше
                Button("Сменить канал у всех") {
                    showChannelAllPicker = true
                }
                .font(.system(size: 13))
                .disabled(!isConnected)
                .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textDim)

                Spacer()

                Button("Найти свободный канал") {
                    appState.scanResultText = "Слушаю эфир…"
                    controller.scanChannels()
                }
                .font(.system(size: 13))
                .disabled(!isConnected)
                .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textDim)
            }

            if !appState.scanResultText.isEmpty {
                Text(appState.scanResultText)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(AppColors.textDim)
            }

            Button("Применить и сохранить") {
                applySettings()
                settingsInfo = "Настройки применены"
            }
            .disabled(!isConnected)
            .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textDim)
        } header: {
            Text("Радио")
        }
        .confirmationDialog("На какой канал перевести всех?", isPresented: $showChannelAllPicker,
                            titleVisibility: .visible) {
            ForEach(0..<channelList.count, id: \.self) { i in
                Button(channelList[i] + (i == appState.currentChannel ? " (сейчас)" : "")) {
                    channelAllTarget = i
                }
            }
            Button("Отмена", role: .cancel) {}
        }
        .alert("Сменить канал у всех", isPresented: Binding(
            get: { channelAllTarget != nil }, set: { if !$0 { channelAllTarget = nil } })) {
            Button("Сменить") {
                if let ch = channelAllTarget {
                    controller.setChannelAll(ch)
                    settingsInfo = "Команда отправлена"
                }
                channelAllTarget = nil
            }
            Button("Отмена", role: .cancel) { channelAllTarget = nil }
        } message: {
            Text("Все рации, которые сейчас слышат эфир, перейдут на CH \(channelAllTarget ?? 0) через 10 секунд. Те, кто не услышит команду, останутся на прежнем канале.")
        }
        .alert(settingsInfo ?? "", isPresented: Binding(
            get: { settingsInfo != nil }, set: { if !$0 { settingsInfo = nil } })) {
            Button("OK", role: .cancel) {}
        }
    }

    // MARK: - Encryption

    private var encryptionSection: some View {
        Section {
            // Строка состояния, как в Android: что сейчас с эфиром и что делать
            Text(appState.keyFingerprint.isEmpty
                 ? "Эфир открыт: без ключа вас слышит любой с такой же рацией"
                 : "Ключ установлен, отпечаток \(appState.keyFingerprint). Введите то же слово на всех рациях группы — отпечаток должен совпасть")
                .font(.system(size: 13))
                .foregroundColor(appState.keyFingerprint.isEmpty ? AppColors.amberAccent : AppColors.greenAccent)

            HStack {
                Text("Кодовое слово")
                    .foregroundColor(AppColors.textPrimary)
                SecureField("Минимум 8 символов", text: $passphrase)
                    .multilineTextAlignment(.trailing)
                    .foregroundColor(AppColors.textSecondary)
            }

            HStack {
                Button("Применить ключ") {
                    controller.setEncryptionKey(passphrase)
                    passphrase = ""
                    settingsInfo = "Рация выводит ключ из слова — несколько секунд"
                }
                .disabled(!isConnected || passphrase.count < 8)
                .foregroundColor(isConnected && passphrase.count >= 8 ? AppColors.greenAccent : AppColors.textDim)

                Spacer()

                Button("Убрать") {
                    confirmClearKey = true
                }
                .disabled(!isConnected)
                .foregroundColor(AppColors.redAccent)
            }

            if !appState.keyFingerprint.isEmpty {
                HStack {
                    Text("Отпечаток")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text(appState.keyFingerprint)
                        .font(.system(size: 18, weight: .bold, design: .monospaced))
                        .foregroundColor(AppColors.greenAccent)
                }
            }

            if appState.alienPackets > 0 {
                HStack {
                    Image(systemName: "exclamationmark.triangle.fill")
                        .foregroundColor(AppColors.amberAccent)
                    Text("Рядом говорят, но другим ключом: \(appState.alienPackets) пакетов не разобрано. Сверьте отпечаток на всех рациях")
                        .font(.system(size: 13))
                        .foregroundColor(AppColors.amberAccent)
                }
            }

            Toggle(isOn: Binding(
                get: { appState.hearPlaintext },
                set: {
                    controller.setHearPlaintext($0)
                    settingsInfo = $0
                        ? "Открытые сообщения снова видны и помечены «без шифра». Помните: их слышит кто угодно"
                        : "Открытый эфир не слушаем — только свои, под ключом"
                }
            )) {
                Text("Слышать и открытый эфир")
                    .foregroundColor(AppColors.textPrimary)
            }
            .tint(AppColors.greenAccent)
            .disabled(!isConnected)    // без связи переключатель ничего бы не изменил
        } header: {
            Text("Шифрование эфира")
        }
        .alert("Убрать ключ?", isPresented: $confirmClearKey) {
            Button("Убрать", role: .destructive) { controller.clearEncryptionKey() }
            Button("Отмена", role: .cancel) {}
        } message: {
            Text("Эфир станет открытым: вас услышит любой с такой же рацией.")
        }
    }

    // MARK: - Notifications

    private var notificationSection: some View {
        Section {
            Picker("Уведомления", selection: Binding(
                get: { appState.notifyMode },
                set: { appState.notifyMode = $0 }
            )) {
                Text("Выкл").tag(0)
                Text("Только личные").tag(1)
                Text("Все сообщения").tag(2)
            }
            .foregroundColor(AppColors.textPrimary)
        } header: {
            Text("Уведомления")
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
                    Text("\(appState.rxVolume)%")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: intBinding(\.rxVolume), in: 50...300, step: 10)
                    .tint(AppColors.greenAccent)
            }

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("PTT RMS (отсечка шума)")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text(appState.squelchThreshold == 0 ? "Выкл" : "\(appState.squelchThreshold)")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: intBinding(\.squelchThreshold), in: 0...1000, step: 10)
                    .tint(AppColors.greenAccent)
            }

            Toggle(isOn: $appState.rogerBeep) {
                Text("Звук окончания передачи")
                    .foregroundColor(AppColors.textPrimary)
            }
            .tint(AppColors.greenAccent)
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
                    Text("\(appState.voxThreshold)")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: intBinding(\.voxThreshold), in: 0...5000, step: 100)
                    .tint(AppColors.greenAccent)
            }

            VStack(alignment: .leading, spacing: 4) {
                HStack {
                    Text("Задержка VOX")
                        .foregroundColor(AppColors.textPrimary)
                    Spacer()
                    Text("\(appState.voxHangtimeMs) мс")
                        .foregroundColor(AppColors.textSecondary)
                        .font(.system(size: 13, design: .monospaced))
                }
                Slider(value: intBinding(\.voxHangtimeMs), in: 200...2000, step: 100)
                    .tint(AppColors.greenAccent)
            }
        } header: {
            Text("VOX")
        }
    }

    // MARK: - History

    private var historySection: some View {
        Section {
            Picker("История файлов", selection: Binding(
                get: { historyValues.firstIndex(of: appState.fileHistoryDays) ?? 2 },
                set: { appState.fileHistoryDays = historyValues[$0] })) {
                ForEach(0..<historyOptions.count, id: \.self) { i in
                    Text(historyOptions[i]).tag(i)
                }
            }
            .foregroundColor(AppColors.textPrimary)

            Button("Очистить список абонентов") {
                confirmClearPeers = true
            }
            .foregroundColor(AppColors.redAccent)
            .alert("Очистить всех абонентов и последние вызовы?", isPresented: $confirmClearPeers) {
                Button("Очистить", role: .destructive) { controller.clearPeers() }
                Button("Отмена", role: .cancel) {}
            }
        } header: {
            Text("Хранение")
        }
    }

    // MARK: - Language

    private var languageSection: some View {
        Section {
            Picker("Язык", selection: $appState.languageIndex) {
                Text("Русский").tag(0)
                Text("English").tag(1)
            }
            .foregroundColor(AppColors.textPrimary)
        } header: {
            Text("Язык")
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
                    controller.setRepeater(enable: false)
                }
                .foregroundColor(AppColors.redAccent)
                .disabled(!isConnected)
            }
        } header: {
            Text("Ретранслятор")
        }
        .alert("Ретранслятор", isPresented: $showRepeaterConfirm) {
            // Раньше обе кнопки ретранслятора были пустыми и ничего не отправляли
            Button("Включить", role: .destructive) {
                controller.setRepeater(enable: true,
                                       ssid: repeaterSsid.trimmingCharacters(in: .whitespaces),
                                       password: repeaterPass.trimmingCharacters(in: .whitespaces),
                                       ip: repeaterIp.trimmingCharacters(in: .whitespaces))
            }
            Button("Отмена", role: .cancel) {}
        } message: {
            Text("Устройство перезагрузится в режиме ретранслятора. Подключитесь к WiFi для мониторинга.")
        }
    }

    // MARK: - About

    private var aboutSection: some View {
        Section {
            HStack {
                Text("Приложение")
                    .foregroundColor(AppColors.textPrimary)
                Spacer()
                Text(Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? "")
                    .foregroundColor(AppColors.textDim)
            }
            HStack {
                Text("Прошивка рации")
                    .foregroundColor(AppColors.textPrimary)
                Spacer()
                Text(appState.firmwareVersion.isEmpty
                     ? (isConnected ? "не сообщает" : "не подключено")
                     : appState.firmwareVersion)
                    .foregroundColor(appState.firmwareVersion.isEmpty ? AppColors.textDim : AppColors.greenAccent)
            }

            Button("Проверить обновления") {
                controller.checkForUpdates()
            }
            .font(.system(size: 13))
            .foregroundColor(AppColors.greenAccent)

            if !appState.updateStatus.isEmpty {
                Text(appState.updateStatus)
                    .font(.system(size: 12))
                    .foregroundColor(appState.updateAvailable ? AppColors.greenAccent : AppColors.textDim)
            }

            // «Скачать», как в Android: страница с IPA и прошивками
            if appState.updateAvailable {
                Link("Скачать", destination: URL(string: "https://meshtrx.ru/ios/")!)
                    .font(.system(size: 13, weight: .bold))
                    .foregroundColor(AppColors.greenAccent)
            }
        } header: {
            Text("О приложении")
        }
    }

    // MARK: - Actions

    private func applySettings() {
        let beaconSec = beaconSeconds[beaconIndex]

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

    private func loadDeviceSettings() {
        if let p = appState.deviceTxPower {
            txPower = Double(min(max(p, 1), appState.band.maxDbm))
        }
        if let dc = appState.deviceDutyCycle { dutyCycle = dc }
        if let bs = appState.deviceBeaconSec, let i = beaconSeconds.firstIndex(of: bs) { beaconIndex = i }
    }
}
