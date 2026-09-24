import SwiftUI

struct MessagesView: View {
    @EnvironmentObject var appState: AppState
    @EnvironmentObject var controller: MeshTRXController

    @State private var messageText: String = ""
    @State private var destId: String? = nil
    @State private var destName: String? = nil
    @State private var filterName: String? = nil
    @State private var showDestPicker: Bool = false
    @State private var isRecordingVoice: Bool = false
    @State private var voiceRecordStart: Date? = nil

    private var isConnected: Bool { appState.bleState == .connected }

    private var bytesRemaining: Int {
        84 - (messageText.data(using: .utf8)?.count ?? 0)
    }

    private var filteredMessages: [ChatMessage] {
        guard let filter = filterName else { return appState.messages }
        return appState.messages.filter { msg in
            if msg.isOutgoing {
                return msg.destName == filter
            } else {
                let name = msg.senderName.isEmpty ? "TX-\(msg.senderId)" : msg.senderName
                return name == filter
            }
        }
    }

    private var filterOptions: [String] {
        var contacts = Set<String>()
        for msg in appState.messages {
            if !msg.isOutgoing && msg.senderId != "??" {
                let name = msg.senderName.isEmpty ? "TX-\(msg.senderId)" : msg.senderName
                contacts.insert(name)
            }
            if msg.isOutgoing, let name = msg.destName {
                contacts.insert(name)
            }
        }
        return ["Все"] + contacts.sorted()
    }

    var body: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 0) {
                // Filter bar
                filterBar

                // Messages
                ScrollViewReader { proxy in
                    ScrollView {
                        LazyVStack(spacing: 4) {
                            ForEach(filteredMessages) { msg in
                                MessageBubbleView(message: msg)
                                    .id(msg.id)
                            }
                        }
                        .padding(.horizontal, 12)
                        .padding(.vertical, 8)
                    }
                    .onChange(of: appState.messages.count) { _ in
                        if let last = filteredMessages.last {
                            withAnimation {
                                proxy.scrollTo(last.id, anchor: .bottom)
                            }
                        }
                    }
                }

                // Input bar
                inputBar
            }
        }
    }

    // MARK: - Filter bar

    private var filterBar: some View {
        HStack(spacing: 8) {
            // Destination
            Button {
                showDestPicker = true
            } label: {
                HStack(spacing: 4) {
                    Image(systemName: "at")
                        .font(.system(size: 12))
                    Text(destName ?? "Все")
                        .font(.system(size: 12))
                }
                .foregroundColor(AppColors.greenAccent)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(AppColors.greenBg)
                .cornerRadius(6)
            }
            .disabled(!isConnected)

            Spacer()

            // Clear chat
            Button {
                appState.showClearChatConfirm = true
            } label: {
                Image(systemName: "trash")
                    .font(.system(size: 12))
                    .foregroundColor(AppColors.redAccent)
                    .padding(.horizontal, 6)
                    .padding(.vertical, 4)
                    .background(AppColors.redBg)
                    .cornerRadius(6)
            }
            .alert("Очистить чат", isPresented: $appState.showClearChatConfirm) {
                Button("Удалить всю переписку с этого телефона?", role: .destructive) {
                    controller.clearChat()
                }
                Button("Отмена", role: .cancel) {}
            }

            // Filter
            Menu {
                ForEach(filterOptions, id: \.self) { option in
                    Button(option) {
                        filterName = option == "Все" ? nil : option
                    }
                }
            } label: {
                HStack(spacing: 4) {
                    Image(systemName: "line.3.horizontal.decrease")
                        .font(.system(size: 11))
                    Text(filterName ?? "Все")
                        .font(.system(size: 12))
                }
                .foregroundColor(AppColors.blueAccent)
                .padding(.horizontal, 8)
                .padding(.vertical, 4)
                .background(AppColors.blueBg)
                .cornerRadius(6)
            }
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 6)
        .background(AppColors.bgSurface)
    }

    // MARK: - Input bar

    private var inputBar: some View {
        VStack(spacing: 0) {
        HStack(spacing: 8) {
            TextField("Сообщение...", text: $messageText)
                .textFieldStyle(.plain)
                .foregroundColor(AppColors.textPrimary)
                .padding(.horizontal, 12)
                .padding(.vertical, 8)
                .background(AppColors.bgElevated)
                .cornerRadius(8)
                .disabled(!isConnected)

            // Bytes counter
            Text("\(bytesRemaining)")
                .font(.system(size: 11, design: .monospaced))
                .foregroundColor(bytesRemaining < 10 ? AppColors.redAccent : AppColors.textDim)
                .frame(width: 28)

            // Voice message button
            if destId != nil && isConnected {
                Button {
                    if isRecordingVoice {
                        stopVoiceRecording()
                    } else {
                        startVoiceRecording()
                    }
                } label: {
                    Image(systemName: isRecordingVoice ? "stop.circle.fill" : "mic.circle.fill")
                        .font(.system(size: 28))
                        .foregroundColor(isRecordingVoice ? AppColors.redAccent : AppColors.blueAccent)
                }
            }

            Button {
                sendMessage()
            } label: {
                Image(systemName: "arrow.up.circle.fill")
                    .font(.system(size: 28))
                    .foregroundColor(canSend ? AppColors.greenAccent : AppColors.textDim)
            }
            .disabled(!canSend)
        }
        .padding(.horizontal, 12)
        .padding(.vertical, 8)
        .background(AppColors.bgSurface)

        if isRecordingVoice {
            HStack {
                Circle()
                    .fill(AppColors.redAccent)
                    .frame(width: 10, height: 10)
                Text("Запись...")
                    .font(.system(size: 13))
                    .foregroundColor(AppColors.redAccent)
                Spacer()
                Button("Отмена") {
                    cancelVoiceRecording()
                }
                .font(.system(size: 12))
                .foregroundColor(AppColors.textMuted)
            }
            .padding(.horizontal, 16)
            .padding(.vertical, 4)
            .background(AppColors.bgSurface)
        }
        } // VStack
    }

    private var canSend: Bool {
        isConnected && !messageText.trimmingCharacters(in: .whitespaces).isEmpty && bytesRemaining >= 0
    }

    private func startVoiceRecording() {
        isRecordingVoice = true
        voiceRecordStart = Date()
        controller.audioEngine.startBufferedRecording()
    }

    private func stopVoiceRecording() {
        isRecordingVoice = false
        controller.audioEngine.stopRecording()
        // Voice recording via file transfer: encode accumulated audio and send as FILE_TYPE_VOICE
        if let encoded = controller.audioEngine.getRecordedCodec2Data(), encoded.count > 8 {
            let duration = Int(Date().timeIntervalSince(voiceRecordStart ?? Date()))
            let fmt = DateFormatter()
            fmt.dateFormat = "HHmm"
            let fileName = "voice_\(fmt.string(from: Date())).c2"
            controller.sendFile(fileName: fileName, fileType: 0x04, data: encoded,
                              destMac: destId.flatMap { macFromId($0) }, destName: destName)

            let now = Int64(Date().timeIntervalSince1970 * 1000)
            let timeFmt = DateFormatter()
            timeFmt.dateFormat = "HH:mm"
            let msg = ChatMessage(
                id: now, text: "🎤 \(max(1, duration))с", isOutgoing: true, senderId: "me",
                senderName: appState.callSign, destId: destId, destName: destName,
                status: .sending, time: timeFmt.string(from: Date()), isVoice: true
            )
            appState.messages.append(msg)
        }
        voiceRecordStart = nil
    }

    private func cancelVoiceRecording() {
        isRecordingVoice = false
        controller.audioEngine.stopRecording()
        voiceRecordStart = nil
    }

    private func macFromId(_ id: String) -> Data? {
        guard id.count >= 4 else { return nil }
        let hi = UInt8(id.prefix(2), radix: 16) ?? 0
        let lo = UInt8(id.suffix(2), radix: 16) ?? 0
        return Data([hi, lo])
    }

    private func sendMessage() {
        let text = messageText.trimmingCharacters(in: .whitespaces)
        guard !text.isEmpty, bytesRemaining >= 0 else { return }
        controller.sendTextMessage(text, destId: destId, destName: destName)
        messageText = ""
        appState.unreadMessages = 0
    }
}

// MARK: - Message Bubble

struct MessageBubbleView: View {
    let message: ChatMessage

    var body: some View {
        HStack {
            if message.isOutgoing { Spacer(minLength: 48) }

            VStack(alignment: message.isOutgoing ? .trailing : .leading, spacing: 2) {
                // Sender name (incoming only)
                if !message.isOutgoing {
                    Text(message.senderName.isEmpty ? "TX-\(message.senderId)" : message.senderName)
                        .font(.system(size: 11, weight: .medium))
                        .foregroundColor(AppColors.greenAccent)
                }

                // Message text
                Text(message.text)
                    .font(.system(size: 14))
                    .foregroundColor(AppColors.textPrimary)

                // Meta line
                HStack(spacing: 4) {
                    Text(message.time)
                        .font(.system(size: 10, design: .monospaced))
                        .frame(width: 36, alignment: .leading)

                    if message.isOutgoing, let name = message.destName {
                        Text("→ \(name)")
                            .font(.system(size: 10))
                    }

                    if let rssi = message.rssi {
                        Text("\(rssi)dBm")
                            .font(.system(size: 10, design: .monospaced))
                    }

                    if !message.encrypted && !message.isOutgoing {
                        Text("⚠ без шифра")
                            .font(.system(size: 9, weight: .bold))
                            .foregroundColor(AppColors.redAccent)
                    }

                    // Status icon (outgoing)
                    if message.isOutgoing {
                        statusIcon
                    }
                }
                .foregroundColor(AppColors.textDim)
            }
            .padding(.horizontal, 10)
            .padding(.vertical, 6)
            .background(message.isOutgoing ? AppColors.blueBg : AppColors.bgElevated)
            .cornerRadius(12)

            if !message.isOutgoing { Spacer(minLength: 48) }
        }
    }

    @ViewBuilder
    private var statusIcon: some View {
        switch message.status {
        case .sending:
            Image(systemName: "clock")
                .font(.system(size: 9))
        case .sent:
            Image(systemName: "checkmark")
                .font(.system(size: 9))
        case .delivered:
            Image(systemName: "checkmark.circle.fill")
                .font(.system(size: 9))
                .foregroundColor(AppColors.greenAccent)
        case .failed:
            Image(systemName: "exclamationmark.circle")
                .font(.system(size: 9))
                .foregroundColor(AppColors.redAccent)
        }
    }
}
