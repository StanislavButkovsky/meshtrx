import SwiftUI

struct MainTabView: View {
    @EnvironmentObject var appState: AppState
    // Стартовая вкладка из аргумента запуска (-startTab N): нужна для снимков
    // экрана с симулятора, где нажимать некому. Без аргумента — первая вкладка.
    @State private var selectedTab = UserDefaults.standard.integer(forKey: "startTab")

    private var isConnected: Bool { appState.bleState == .connected }

    var body: some View {
        VStack(spacing: 0) {
            // MARK: - Fixed Header
            headerBar
                .padding(.horizontal, 16)
                .padding(.top, 8)
                .padding(.bottom, 8)
                .background(AppColors.bgPrimary)

            // MARK: - Tab Content
            TabView(selection: $selectedTab) {
                VoiceView()
                    .tabItem {
                        Image(systemName: "mic.fill")
                        Text("PTT")
                    }
                    .badge(appState.missedCalls)
                    .tag(0)

                MessagesView()
                    .tabItem {
                        Image(systemName: "envelope.fill")
                        Text("Чат")
                    }
                    .badge(appState.unreadMessages)
                    .tag(1)

                FilesView()
                    .tabItem {
                        Image(systemName: "doc.fill")
                        Text("Файлы")
                    }
                    .tag(2)

                MapTabView()
                    .tabItem {
                        Image(systemName: "map.fill")
                        Text("Карта")
                    }
                    .tag(3)

                SettingsView()
                    .tabItem {
                        Image(systemName: "wrench.fill")
                        Text("Настр.")
                    }
                    .tag(4)
            }
            .accentColor(AppColors.accent)
            // Счётчик непрочитанных обнуляется, когда чат на экране. Раньше —
            // только при отправке сообщения или очистке, и цифра висела.
            .onChange(of: selectedTab) { tab in
                if tab == 1 { appState.unreadMessages = 0 }
                // Пропущенные вызовы видны в «Последних» на PTT — открыли, сбросили
                if tab == 0 { appState.missedCalls = 0 }
            }
            .onChange(of: appState.requestedTab) { tab in
                if let tab = tab { selectedTab = tab; appState.requestedTab = nil }
            }
            .onChange(of: appState.unreadMessages) { n in
                if selectedTab == 1 && n > 0 { appState.unreadMessages = 0 }
            }
        }
    }

    // MARK: - Header Bar

    private var headerBar: some View {
        HStack {
            // Left: callsign + device name
            VStack(alignment: .leading, spacing: 2) {
                Text(appState.callSign.isEmpty ? "MeshTRX" : appState.callSign)
                    .font(.system(size: 18, weight: .bold))
                    .foregroundColor(AppColors.textPrimary)
                Text(appState.deviceName)
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.textMuted)
            }

            Spacer()

            // Справа, как в Android: значок Bluetooth, батарея рации, канал
            VStack(alignment: .trailing, spacing: 2) {
                HStack(spacing: 6) {
                    if appState.bleState == .connecting || appState.bleState == .scanning {
                        Text(connectionStatusText)
                            .font(.system(size: 12))
                            .foregroundColor(AppColors.textMuted)
                    }
                    Image(systemName: "dot.radiowaves.left.and.right")
                        .font(.system(size: 13))
                        .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textDim)
                    if isConnected && appState.batteryVoltage > 0.5 {
                        Image(systemName: batteryIcon)
                            .font(.system(size: 13))
                            .foregroundColor(batteryColor)
                        Text(String(format: "%.1fv", appState.batteryVoltage))
                            .font(.system(size: 12, design: .monospaced))
                            .foregroundColor(batteryColor)
                    }
                }
                Text("CH \(appState.currentChannel) · \(appState.band.mhzText(appState.currentChannel)) MHz")
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(AppColors.greenAccent)
            }
        }
    }

    // Пороги — как в Android
    private var batteryIcon: String {
        let v = appState.batteryVoltage
        return v >= 4.0 ? "battery.100" : v >= 3.8 ? "battery.75" : v >= 3.6 ? "battery.50" : v >= 3.4 ? "battery.25" : "battery.0"
    }
    private var batteryColor: Color {
        let v = appState.batteryVoltage
        return v >= 3.8 ? AppColors.greenAccent : v >= 3.6 ? AppColors.amberAccent : AppColors.redTx
    }

    private var connectionStatusText: String {
        switch appState.bleState {
        case .connected: return "Подключено"
        case .scanning: return "Поиск..."
        case .connecting: return "Соединение..."
        case .disconnected: return "Отключено"
        }
    }
}
