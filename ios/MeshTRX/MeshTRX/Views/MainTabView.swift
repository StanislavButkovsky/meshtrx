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
            .accentColor(.green)
            // Счётчик непрочитанных обнуляется, когда чат на экране. Раньше —
            // только при отправке сообщения или очистке, и цифра висела.
            .onChange(of: selectedTab) { tab in
                if tab == 1 { appState.unreadMessages = 0 }
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
                Text(appState.deviceName.isEmpty ? "—" : appState.deviceName)
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.textMuted)
            }

            Spacer()

            // Right: connection status + channel
            VStack(alignment: .trailing, spacing: 2) {
                HStack(spacing: 4) {
                    Circle()
                        .fill(isConnected ? AppColors.greenAccent : AppColors.redAccent)
                        .frame(width: 8, height: 8)
                    Text(connectionStatusText)
                        .font(.system(size: 13))
                        .foregroundColor(isConnected ? AppColors.greenAccent : AppColors.textMuted)
                }
                if isConnected {
                    Text(String(format: "CH %d · %.2f MHz", appState.currentChannel, 863.15 + Double(appState.currentChannel) * 0.3))
                        .font(.system(size: 12, design: .monospaced))
                        .foregroundColor(AppColors.textDim)
                }
            }
        }
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
