import SwiftUI

@main
struct MeshTRXApp: App {
    @StateObject private var appState = AppState()
    @StateObject private var controller: MeshTRXController
    // Смена оформления перестраивает экраны: цвета вычисляются при отрисовке
    @AppStorage(AppTheme.key) private var theme = AppTheme.main.rawValue

    init() {
        let state = AppState()
        _appState = StateObject(wrappedValue: state)
        _controller = StateObject(wrappedValue: MeshTRXController(appState: state))
    }

    /// Нижняя панель вкладок в цветах темы
    static func styleBars() {
        let a = UITabBarAppearance()
        a.configureWithOpaqueBackground()
        a.backgroundColor = UIColor(AppColors.navBg)
        let normal = UIColor(AppColors.textFaint)
        a.stackedLayoutAppearance.normal.iconColor = normal
        a.stackedLayoutAppearance.normal.titleTextAttributes = [.foregroundColor: normal]
        a.stackedLayoutAppearance.normal.badgeBackgroundColor = UIColor(AppColors.redTx)
        UITabBar.appearance().standardAppearance = a
        UITabBar.appearance().scrollEdgeAppearance = a
    }

    var body: some Scene {
        WindowGroup {
            SplashView()
                .id(theme)
                .onAppear { Self.styleBars() }
                .onChange(of: theme) { _ in Self.styleBars() }
                .environmentObject(appState)
                .environmentObject(controller)
                .preferredColorScheme(.dark)
                .fullScreenCover(item: $appState.incomingCall) { call in
                    IncomingCallView(
                        call: call,
                        onDismiss: { appState.incomingCall = nil },
                        onMissed: { appState.missedCalls += 1 }
                    )
                }
                .sheet(isPresented: $appState.showPinDialog) {
                    PinEntryView(
                        onSubmit: { pin in
                            controller.submitPin(pin)
                            appState.showPinDialog = false
                        },
                        onCancel: {
                            controller.disconnect()
                            appState.showPinDialog = false
                        }
                    )
                    // Смахнуть окно PIN нельзя: иначе «Отмена» не срабатывала,
                    // и приложение навсегда оставалось в «соединении».
                    .interactiveDismissDisabled()
                }
        }
    }
}

// MARK: - PIN Entry View

struct PinEntryView: View {
    var onSubmit: (Int) -> Void
    var onCancel: () -> Void

    @State private var pinText: String = ""

    var body: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 20) {
                Spacer()

                Image(systemName: "lock.fill")
                    .font(.system(size: 40))
                    .foregroundColor(AppColors.greenAccent)

                Text("PIN устройства")
                    .font(.system(size: 22, weight: .bold))
                    .foregroundColor(AppColors.textPrimary)

                Text("Введите 4-значный код с OLED дисплея")
                    .font(.system(size: 14))
                    .foregroundColor(AppColors.textMuted)
                    .multilineTextAlignment(.center)

                TextField("0000", text: $pinText)
                    .keyboardType(.numberPad)
                    .font(.system(size: 32, weight: .bold, design: .monospaced))
                    .foregroundColor(AppColors.textPrimary)
                    .multilineTextAlignment(.center)
                    .padding()
                    .background(AppColors.bgElevated)
                    .cornerRadius(12)
                    .frame(width: 200)

                HStack(spacing: 20) {
                    Button {
                        onCancel()
                    } label: {
                        Text("Отмена")
                            .font(.system(size: 16, weight: .medium))
                            .foregroundColor(AppColors.redAccent)
                            .frame(width: 120, height: 44)
                            .background(AppColors.redBg)
                            .cornerRadius(10)
                    }

                    Button {
                        let pin = Int(pinText) ?? 0
                        onSubmit(pin)
                    } label: {
                        Text("OK")
                            .font(.system(size: 16, weight: .bold))
                            .foregroundColor(pinText.isEmpty ? AppColors.textMuted : AppColors.onAccent)
                            .frame(width: 120, height: 44)
                            .background(pinText.isEmpty ? AppColors.bgElevated : AppColors.accent)
                            .cornerRadius(10)
                    }
                    .disabled(pinText.isEmpty)
                }

                Spacer()
            }
            .padding(.horizontal, 32)
        }
    }
}
