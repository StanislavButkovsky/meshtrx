import SwiftUI

struct VoiceView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController
    @State private var speakerOn = true
    @State private var isPttPressed = false

    private var isConnected: Bool { appState.bleState == .connected }
    private var isVox: Bool { appState.txMode == .vox }

    private var pttState: PttState {
        if appState.isPttActive {
            return isVox ? .voxTx : .tx
        } else if appState.rxCallSign != nil {
            return .rx      // «ПРИЁМ»: состояние было описано, но не включалось никогда
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
    }

    // MARK: - Network Summary

    // Сводка, как в Android: слышимые станции (по сроку списка), ретранслятор по
    // признаку маяка, последнее движение в эфире. Пересчёт раз в 15 с: «минуту
    // назад» стареет само, без новых событий.
    private var networkSummary: some View {
        TimelineView(.periodic(from: .now, by: 15)) { _ in
            let now = Int64(Date().timeIntervalSince1970 * 1000)
            let live = appState.peers.filter { now - $0.lastSeenMs < Int64(appState.peerTimeoutMin) * 60_000 }
            let repeater = live.first { $0.isRepeater }
            VStack(alignment: .leading, spacing: 2) {
                Text("слышно \(live.count) \(stationsWord(live.count)) · " +
                     (repeater.map { "ретранслятор \($0.callSign) (\($0.rssi) dBm)" } ?? "ретранслятора не слышно"))
                    .font(.system(size: 12))
                    .foregroundColor(Color(hex: 0xaaaaaa))
                    .lineLimit(1)
                if let last = live.max(by: { $0.lastSeenMs < $1.lastSeenMs }) {
                    Text("последний: \(last.callSign), \(formatAgo(Int((now - last.lastSeenMs) / 1000))) · \(last.rssi)dBm, SNR \(last.snr)")
                        .font(.system(size: 12))
                        .foregroundColor(AppColors.textDim)
                        .lineLimit(1)
                } else {
                    Text("в эфире тихо")
                        .font(.system(size: 12))
                        .foregroundColor(AppColors.textDim)
                }
            }
            .frame(maxWidth: .infinity, alignment: .leading)
        }
    }

    private func stationsWord(_ n: Int) -> String {
        let mod10 = n % 10; let mod100 = n % 100
        if mod100 >= 11 && mod100 <= 19 { return "станций" }
        if mod10 == 1 { return "станцию" }
        if mod10 >= 2 && mod10 <= 4 { return "станции" }
        return "станций"
    }

    private func formatAgo(_ sec: Int) -> String {
        if sec < 60 { return "\(sec) с назад" }
        if sec < 3600 { return "\(sec / 60) мин назад" }
        return "\(sec / 3600) ч назад"
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
        if appState.rxCallSign != nil { return AppColors.greenAccent }
        if isConnected { return AppColors.greenDim }
        return AppColors.textDim
    }

    private var statusTextColor: Color {
        if appState.isPttActive { return AppColors.redTx }
        return AppColors.greenDim
    }

    private var statusText: String {
        if !isConnected { return "не подключено" }
        if appState.isPttActive {
            if let left = appState.pttSecondsLeft, left <= 3 {
                return left == 0 ? "предел \(MeshTRXController.pttMaxSeconds) с — отпустите кнопку"
                                 : "передача · осталось \(left) с"
            }
            return "передача..."
        }
        if let who = appState.rxCallSign { return "приём: \(who)" }
        if let d = appState.pttDeliveryText { return d }
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

    // Строка адресата, как в Android: «@», кому говорим, и одна кнопка вызова.
    // Раньше были «ОБЩИЙ» и «ВЫЗВАТЬ» с листом и кнопкой SOS, а адресата не было.
    @State private var showTargetPicker = false

    private var callButtonsRow: some View {
        HStack(spacing: 10) {
            Button { showTargetPicker = true } label: {
                Image(systemName: "at")
                    .font(.system(size: 18, weight: .bold))
                    .foregroundColor(AppColors.blueAccent)
                    .frame(width: 48, height: 48)
                    .background(AppColors.blueBg)
                    .overlay(RoundedRectangle(cornerRadius: 8).stroke(AppColors.blueBorder, lineWidth: 1))
                    .cornerRadius(8)
            }
            .disabled(!isConnected)
            .confirmationDialog("Выбрать абонента", isPresented: $showTargetPicker, titleVisibility: .visible) {
                Button("Общий канал") { appState.pttTargetId = nil; appState.pttTargetName = nil }
                ForEach(appState.directRecipients, id: \.id) { r in
                    Button(r.name) { appState.pttTargetId = r.id; appState.pttTargetName = r.name }
                }
                Button("Отмена", role: .cancel) {}
            }

            VStack(alignment: .leading, spacing: 2) {
                Text(appState.pttTargetName ?? "Общий канал")
                    .font(.system(size: 16, weight: .medium))
                    .foregroundColor(appState.pttTargetId == nil ? AppColors.blueAccent : AppColors.greenAccent)
                if let id = appState.pttTargetId,
                   let p = appState.peers.first(where: { $0.deviceId.hasSuffix(id) }) {
                    Text("\(p.rssi) dBm · SNR \(p.snr)")
                        .font(.system(size: 11, design: .monospaced))
                        .foregroundColor(AppColors.textDim)
                }
            }

            Spacer()

            Button { controller.callTarget() } label: {
                Image(systemName: "bell.fill")
                    .font(.system(size: 18))
                    .foregroundColor(AppColors.greenAccent)
                    .frame(width: 48, height: 48)
                    .background(AppColors.greenBg)
                    .clipShape(Circle())
            }
            .disabled(!isConnected)
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
                        ForEach(appState.recentCalls.prefix(20)) { call in
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
                // ✗ — абонент молчит дольше 15 минут: дозвониться вряд ли выйдет
                HStack(spacing: 4) {
                    Text(callDisplayName(call))
                        .font(.system(size: 15, weight: .bold))
                        .foregroundColor(AppColors.textSecondary)
                    if isSilent(call) {
                        Text("✗").font(.system(size: 13, weight: .bold)).foregroundColor(AppColors.redAccent)
                    }
                }

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
        .contentShape(Rectangle())
        .onTapGesture { redial(call) }     // нажатие на строку — повторный вызов, как в Android
    }

    private func isSilent(_ call: RecentCall) -> Bool {
        guard call.callType == "PRIVATE" else { return false }
        let short = String(call.deviceId.suffix(4))
        guard let p = appState.peers.first(where: { $0.deviceId.hasSuffix(short) }) else { return true }
        return Int64(Date().timeIntervalSince1970 * 1000) - p.lastSeenMs > 15 * 60_000
    }

    // MARK: - Helpers

    private func redial(_ call: RecentCall) {
        switch call.callType {
        case "ALL":
            controller.callAll()
        case "PRIVATE":
            // Прошивке нужен полный 4-байтный id. Если в истории только хвост
            // из 4 знаков — достраиваем по списку абонентов.
            var fullId = call.deviceId
            if fullId.count < 8,
               let p = appState.peers.first(where: { $0.deviceId.hasSuffix(fullId) }) {
                fullId = p.deviceId
            }
            guard fullId.count == 8 else { return }
            let call = RecentCall(deviceId: fullId, callSign: call.callSign,
                                  isOutgoing: call.isOutgoing, callType: call.callType)
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
