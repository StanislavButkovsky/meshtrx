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

            VStack(spacing: 0) {
                // MARK: - Network Summary
                networkSummary
                    .padding(.horizontal, 16)
                    .padding(.vertical, 6)
                    .background(AppColors.bgSurface)

                // MARK: - Control Bar
                controlBar
                    .padding(.horizontal, 16)
                    .padding(.vertical, 6)

                // MARK: - Recent Calls label
                Text("ПОСЛЕДНИЕ")
                    .font(.system(size: 11, weight: .medium))
                    .foregroundColor(AppColors.textMuted)
                    .tracking(0.6)
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.horizontal, 16)
                    .padding(.top, 4)

                // MARK: - Recent Calls (fills remaining space)
                recentCallsList

                // MARK: - Call Buttons
                callButtonsRow
                    .padding(.horizontal, 16)
                    .padding(.vertical, 8)

                // MARK: - VOX Status
                if isVox {
                    Text(voxText)
                        .font(.system(size: 12))
                        .foregroundColor(voxColor)
                        .frame(height: 16)
                }

                // MARK: - Status Line
                statusLine
                    .padding(.horizontal, 12)
                    .padding(.vertical, 4)

                // MARK: - PTT Button Frame
                ZStack {
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
                    .frame(width: 200, height: 200)
                    .opacity(isConnected ? 1.0 : 0.4)
                    .allowsHitTesting(isConnected && !isVox)

                    // Speaker button — top-right
                    VStack {
                        HStack {
                            Spacer()
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
                                    .foregroundColor(speakerOn ? AppColors.greenAccent : AppColors.textDim)
                                    .frame(width: 44, height: 44)
                                    .background(speakerOn ? AppColors.greenBg : AppColors.bgElevated)
                                    .clipShape(Circle())
                            }
                            .padding(.trailing, 16)
                        }
                        Spacer()
                    }
                }
                .frame(height: 210)
                .padding(.top, 4)
                .padding(.bottom, 12)
            }
        }
        .sheet(isPresented: $showCallPicker) {
            callPickerSheet
        }
    }

    // MARK: - Network Summary

    private var networkSummary: some View {
        VStack(alignment: .leading, spacing: 2) {
            let count = appState.peers.count
            let repeater = appState.peers.first { $0.callSign.uppercased().contains("RPT") || $0.callSign.uppercased().contains("REP") }
            Text(count == 0 ? "Нет станций в эфире" : "\(count) \(stationsWord(count)) в эфире\(repeater != nil ? " · Ретр: \(repeater!.callSign) \(repeater!.rssi)dBm" : "")")
                .font(.system(size: 12))
                .foregroundColor(Color(hex: 0xaaaaaa))
                .lineLimit(1)

            if let last = appState.peers.sorted(by: { $0.lastSeenMs > $1.lastSeenMs }).first {
                let ago = Int((Date().timeIntervalSince1970 * 1000 - Double(last.lastSeenMs)) / 1000)
                Text("Последний: \(last.callSign) \(formatAgo(ago)) \(last.rssi)dBm/\(last.snr)dB")
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.textDim)
                    .lineLimit(1)
            }
        }
    }

    private func stationsWord(_ n: Int) -> String {
        let mod10 = n % 10; let mod100 = n % 100
        if mod100 >= 11 && mod100 <= 19 { return "станций" }
        if mod10 == 1 { return "станция" }
        if mod10 >= 2 && mod10 <= 4 { return "станции" }
        return "станций"
    }

    private func formatAgo(_ sec: Int) -> String {
        if sec < 60 { return "\(sec)с назад" }
        return "\(sec / 60)мин назад"
    }

    // MARK: - Control Bar

    private var controlBar: some View {
        HStack(spacing: 8) {
            // Listen Mode buttons
            HStack(spacing: 4) {
                listenButton("Слушать всех", mode: .all)
                listenButton("Только мои", mode: .privateOnly)
            }

            Spacer()

            // PTT / VOX switch
            HStack(spacing: 4) {
                Text("PTT")
                    .font(.system(size: 12))
                    .foregroundColor(isVox ? AppColors.textMuted : AppColors.textPrimary)

                Toggle("", isOn: Binding(
                    get: { isVox },
                    set: { controller.setTxMode($0 ? .vox : .ptt) }
                ))
                .toggleStyle(SwitchToggleStyle(tint: AppColors.greenAccent))
                .labelsHidden()
                .disabled(!isConnected)

                Text("VOX")
                    .font(.system(size: 12))
                    .foregroundColor(isVox ? AppColors.textPrimary : AppColors.textMuted)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(AppColors.bgElevated)
            .cornerRadius(8)
        }
    }

    private func listenButton(_ title: String, mode: ListenMode) -> some View {
        let isSelected = appState.listenMode == mode
        return Button {
            appState.listenMode = mode
        } label: {
            Text(title)
                .font(.system(size: 11, weight: .medium))
                .foregroundColor(isSelected ? AppColors.greenAccent : AppColors.textMuted)
                .padding(.horizontal, 10)
                .padding(.vertical, 8)
                .frame(height: 36)
                .background(isSelected ? AppColors.greenBg : AppColors.bgElevated)
                .overlay(
                    RoundedRectangle(cornerRadius: 6)
                        .stroke(isSelected ? AppColors.greenBorder : Color.clear, lineWidth: 1)
                )
                .cornerRadius(6)
        }
    }

    // MARK: - Status Line

    private var statusLine: some View {
        HStack(spacing: 4) {
            Circle()
                .fill(statusDotColor)
                .frame(width: 8, height: 8)
            Text(statusText)
                .font(.system(size: 12))
                .foregroundColor(statusTextColor)
        }
    }

    private var statusDotColor: Color {
        if appState.isPttActive { return AppColors.redTx }
        if isConnected { return AppColors.greenDim }
        return AppColors.textDim
    }

    private var statusTextColor: Color {
        if appState.isPttActive { return AppColors.redTx }
        return AppColors.greenDim
    }

    private var statusText: String {
        if !isConnected { return "не подключено" }
        if appState.isPttActive { return "передача..." }
        return "ожидание"
    }

    // MARK: - VOX state

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

    // MARK: - Call Buttons

    private var callButtonsRow: some View {
        HStack(spacing: 8) {
            Button { controller.callAll() } label: {
                Text("ОБЩИЙ")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(AppColors.blueAccent)
                    .frame(maxWidth: .infinity)
                    .frame(height: 44)
                    .background(AppColors.blueBg)
                    .overlay(
                        RoundedRectangle(cornerRadius: 6)
                            .stroke(AppColors.blueBorder, lineWidth: 1)
                    )
                    .cornerRadius(6)
            }
            .disabled(!isConnected)

            Button { showCallPicker = true } label: {
                Text("ВЫЗВАТЬ")
                    .font(.system(size: 13, weight: .medium))
                    .foregroundColor(AppColors.greenAccent)
                    .frame(maxWidth: .infinity)
                    .frame(height: 44)
                    .background(AppColors.greenBg)
                    .overlay(
                        RoundedRectangle(cornerRadius: 6)
                            .stroke(AppColors.greenBorder, lineWidth: 1)
                    )
                    .cornerRadius(6)
            }
            .disabled(!isConnected)
        }
    }

    // MARK: - Call Picker Sheet

    private var callPickerSheet: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 12) {
                Text("Выбор вызова")
                    .font(.system(size: 16, weight: .bold))
                    .foregroundColor(AppColors.textPrimary)
                    .padding(.top, 20)

                // Private calls to peers
                if appState.peers.isEmpty {
                    Text("Нет пиров в сети")
                        .foregroundColor(AppColors.textDim)
                        .padding()
                } else {
                    ScrollView {
                        LazyVStack(spacing: 4) {
                            ForEach(appState.peers) { peer in
                                Button {
                                    let macBytes = stride(from: 0, to: peer.deviceId.count, by: 2).compactMap {
                                        let start = peer.deviceId.index(peer.deviceId.startIndex, offsetBy: $0)
                                        let end = peer.deviceId.index(start, offsetBy: min(2, peer.deviceId.distance(from: start, to: peer.deviceId.endIndex)))
                                        return UInt8(peer.deviceId[start..<end], radix: 16)
                                    }
                                    controller.callPrivate(macSuffix: Data(macBytes), callSign: peer.callSign)
                                    showCallPicker = false
                                } label: {
                                    HStack {
                                        Text(peer.callSign)
                                            .foregroundColor(AppColors.textPrimary)
                                        Spacer()
                                        Text("\(peer.rssi) dBm")
                                            .font(.system(size: 12, design: .monospaced))
                                            .foregroundColor(AppColors.textDim)
                                    }
                                    .padding(.horizontal, 16)
                                    .padding(.vertical, 12)
                                    .background(AppColors.bgElevated)
                                    .cornerRadius(8)
                                }
                            }
                        }
                        .padding(.horizontal, 16)
                    }
                }

                // SOS button
                Button {
                    controller.callEmergency()
                    showCallPicker = false
                } label: {
                    Text("SOS")
                        .font(.system(size: 15, weight: .bold))
                        .foregroundColor(AppColors.redAccent)
                        .frame(maxWidth: .infinity)
                        .frame(height: 44)
                        .background(AppColors.redBg)
                        .cornerRadius(8)
                }
                .padding(.horizontal, 16)

                Button("Отмена") { showCallPicker = false }
                    .foregroundColor(AppColors.textMuted)
                    .padding(.bottom, 16)
            }
        }
    }

    // MARK: - Recent Calls

    private var recentCallsList: some View {
        VStack(alignment: .leading, spacing: 0) {
            if appState.recentCalls.isEmpty {
                Spacer()
            } else {
                ScrollView {
                    LazyVStack(spacing: 0) {
                        ForEach(appState.recentCalls.prefix(5)) { call in
                            recentCallRow(call)
                        }
                    }
                    .padding(.horizontal, 16)
                }
            }
        }
        .frame(maxHeight: .infinity)
    }

    private func recentCallRow(_ call: RecentCall) -> some View {
        HStack(spacing: 8) {
            // Direction arrow
            Text(call.isOutgoing ? "→" : "←")
                .font(.system(size: 16, design: .monospaced))
                .foregroundColor(callTypeColor(call.callType))
                .frame(width: 24)

            // Content
            VStack(alignment: .leading, spacing: 2) {
                Text(callDisplayName(call))
                    .font(.system(size: 15, weight: .bold))
                    .foregroundColor(AppColors.textSecondary)

                HStack(spacing: 4) {
                    Text(formatCallTime(call.timeMs))
                    Text("·")
                    Text(callTypeShort(call.callType))
                    if let rssi = call.rssi {
                        Text("·")
                        Text("\(rssi) dBm")
                    }
                }
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(AppColors.textMuted)
            }

            Spacer()

            // Redial button
            Button { redial(call) } label: {
                Image(systemName: "phone.fill")
                    .font(.system(size: 14))
                    .foregroundColor(AppColors.greenAccent)
                    .frame(width: 40, height: 40)
            }
        }
        .padding(6)
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

    private func callTypeShort(_ type: String) -> String {
        switch type {
        case "PRIVATE": return "Лич"
        case "ALL": return "Общ"
        case "GROUP": return "Грп"
        case "SOS": return "SOS"
        default: return type
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
