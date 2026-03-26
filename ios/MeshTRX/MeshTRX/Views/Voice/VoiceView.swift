import SwiftUI

struct VoiceView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController
    @State private var speakerOn = true
    @State private var showCallPicker = false
    @State private var isPttPressed = false

    private var isConnected: Bool { appState.bleState == .connected }
    private var isVox: Bool { appState.txMode == .vox }

    private var pttState: PttState {
        if appState.isPttActive {
            return isVox ? .voxTx : .tx
        } else {
            return isVox ? .voxIdle : .idle
        }
    }

    var body: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 12) {
                statusBar
                Spacer()

                if isVox { voxStateLabel }

                // PTT Button — wired to controller
                PttButtonView(
                    state: pttState,
                    rmsLevel: Float(appState.rmsLevel) / 5000.0,
                    onPttDown: {
                        guard !isPttPressed else { return }
                        isPttPressed = true
                        controller.pttDown()
                    },
                    onPttUp: {
                        guard isPttPressed else { return }
                        isPttPressed = false
                        controller.pttUp()
                    }
                )
                .opacity(isConnected ? 1.0 : 0.4)
                .allowsHitTesting(isConnected && !isVox)

                Spacer()
                controlsRow
                listenModeRow
                callButtonsRow

                if !appState.recentCalls.isEmpty {
                    recentCallsList
                }
            }
            .padding(.horizontal, 16)
            .padding(.bottom, 8)
        }
    }

    // MARK: - Status bar

    private var statusBar: some View {
        HStack {
            Circle()
                .fill(isConnected ? AppColors.greenAccent : AppColors.redAccent)
                .frame(width: 8, height: 8)
            Text(appState.statusMessage)
                .font(.system(size: 13))
                .foregroundColor(AppColors.textMuted)
            Spacer()
            if isConnected {
                Text("CH \(appState.currentChannel)")
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(AppColors.textDim)
                Text("\(appState.rssi)dBm")
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(AppColors.rssiColor(appState.rssi))
            }
        }
        .padding(.horizontal, 4)
        .padding(.top, 8)
    }

    // MARK: - VOX state

    private var voxStateLabel: some View {
        Text(voxText)
            .font(.system(size: 14, weight: .medium, design: .monospaced))
            .foregroundColor(voxColor)
            .frame(height: 20)
    }

    private var voxText: String {
        switch appState.voxState {
        case .idle: return ""
        case .attack: return "..."
        case .active: return ">>> TX <<<"
        case .hangtime: return "TX (пауза)"
        }
    }

    private var voxColor: Color {
        switch appState.voxState {
        case .active: return AppColors.redTx
        case .hangtime: return AppColors.amberAccent
        default: return AppColors.textDim
        }
    }

    // MARK: - Controls

    private var controlsRow: some View {
        HStack(spacing: 20) {
            // Speaker toggle
            Button {
                speakerOn.toggle()
                if speakerOn {
                    controller.audioEngine.routeToSpeaker()
                } else {
                    controller.audioEngine.routeToEarpiece()
                }
            } label: {
                Image(systemName: speakerOn ? "speaker.wave.2.fill" : "speaker.slash.fill")
                    .font(.system(size: 18))
                    .foregroundColor(speakerOn ? AppColors.greenAccent : AppColors.textMuted)
                    .frame(width: 44, height: 44)
                    .background(speakerOn ? AppColors.greenBg : AppColors.bgElevated)
                    .clipShape(Circle())
            }

            Spacer()

            // PTT / VOX toggle
            HStack(spacing: 8) {
                Text("PTT")
                    .font(.system(size: 13, weight: isVox ? .regular : .bold))
                    .foregroundColor(isVox ? AppColors.textMuted : AppColors.greenAccent)

                Toggle("", isOn: Binding(
                    get: { isVox },
                    set: { controller.setTxMode($0 ? .vox : .ptt) }
                ))
                .toggleStyle(SwitchToggleStyle(tint: AppColors.greenAccent))
                .labelsHidden()
                .disabled(!isConnected)

                Text("VOX")
                    .font(.system(size: 13, weight: isVox ? .bold : .regular))
                    .foregroundColor(isVox ? AppColors.greenAccent : AppColors.textMuted)
            }

            Spacer()

            VStack(spacing: 2) {
                Text(appState.isPttActive ? "TX" : "RX")
                    .font(.system(size: 12, weight: .bold, design: .monospaced))
                    .foregroundColor(appState.isPttActive ? AppColors.redTx : AppColors.greenDim)
            }
            .frame(width: 44, height: 44)
            .background(appState.isPttActive ? AppColors.redBg : AppColors.bgElevated)
            .clipShape(Circle())
        }
        .padding(.horizontal, 4)
    }

    // MARK: - Listen mode

    private var listenModeRow: some View {
        HStack(spacing: 8) {
            listenButton("ALL", mode: .all)
            listenButton("PRIVATE", mode: .privateOnly)
        }
    }

    private func listenButton(_ title: String, mode: ListenMode) -> some View {
        let isSelected = appState.listenMode == mode
        return Button {
            appState.listenMode = mode
        } label: {
            Text(title)
                .font(.system(size: 12, weight: .medium))
                .foregroundColor(isSelected ? AppColors.greenAccent : AppColors.textMuted)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(isSelected ? AppColors.greenBg : AppColors.bgElevated)
                .cornerRadius(8)
        }
    }

    // MARK: - Call buttons

    private var callButtonsRow: some View {
        HStack(spacing: 8) {
            Button { controller.callAll() } label: {
                Label("Общий", systemImage: "phone.fill")
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.blueAccent)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(AppColors.blueBg)
                    .cornerRadius(8)
            }
            .disabled(!isConnected)

            Button { showCallPicker = true } label: {
                Label("Приватный", systemImage: "person.fill")
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.greenAccent)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(AppColors.greenBg)
                    .cornerRadius(8)
            }
            .disabled(!isConnected)

            Button { controller.callEmergency() } label: {
                Label("SOS", systemImage: "exclamationmark.triangle.fill")
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.redAccent)
                    .frame(maxWidth: .infinity)
                    .padding(.vertical, 8)
                    .background(AppColors.redBg)
                    .cornerRadius(8)
            }
            .disabled(!isConnected)
        }
    }

    // MARK: - Recent calls

    private var recentCallsList: some View {
        VStack(alignment: .leading, spacing: 0) {
            Text("Недавние")
                .font(.system(size: 11))
                .foregroundColor(AppColors.textDim)
                .padding(.bottom, 4)

            ForEach(appState.recentCalls.prefix(5)) { call in
                HStack(spacing: 8) {
                    Text(call.isOutgoing ? "→" : "←")
                        .font(.system(size: 14, design: .monospaced))
                        .foregroundColor(callTypeColor(call.callType))

                    Text(callDisplayName(call))
                        .font(.system(size: 13))
                        .foregroundColor(AppColors.textSecondary)

                    Spacer()

                    Text(formatCallTime(call.timeMs))
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(AppColors.textDim)

                    Button {
                        redial(call)
                    } label: {
                        Image(systemName: "phone.fill")
                            .font(.system(size: 12))
                            .foregroundColor(AppColors.greenAccent)
                    }
                }
                .padding(.vertical, 6)
                .padding(.horizontal, 8)
                .background(AppColors.bgSurface)
                .cornerRadius(6)
                .padding(.vertical, 1)
            }
        }
    }

    // MARK: - Helpers

    private func redial(_ call: RecentCall) {
        switch call.callType {
        case "ALL":
            controller.callAll()
        case "PRIVATE":
            let macBytes = stride(from: 0, to: call.deviceId.count, by: 2).compactMap {
                let start = call.deviceId.index(call.deviceId.startIndex, offsetBy: $0)
                let end = call.deviceId.index(start, offsetBy: min(2, call.deviceId.distance(from: start, to: call.deviceId.endIndex)))
                return UInt8(call.deviceId[start..<end], radix: 16)
            }
            controller.callPrivate(macSuffix: Data(macBytes), callSign: call.callSign)
        case "SOS":
            controller.callEmergency()
        default:
            break
        }
    }

    private func callTypeColor(_ type: String) -> Color {
        switch type {
        case "PRIVATE": return AppColors.greenAccent
        case "ALL": return AppColors.blueAccent
        case "GROUP": return AppColors.amberAccent
        case "SOS": return AppColors.redAccent
        default: return AppColors.textDim
        }
    }

    private func callDisplayName(_ call: RecentCall) -> String {
        switch call.callType {
        case "ALL": return "Общий канал"
        case "SOS": return "SOS"
        default: return call.callSign
        }
    }

    private func formatCallTime(_ timeMs: Int64) -> String {
        let date = Date(timeIntervalSince1970: Double(timeMs) / 1000)
        let fmt = DateFormatter()
        fmt.dateFormat = "HH:mm"
        return fmt.string(from: date)
    }
}
