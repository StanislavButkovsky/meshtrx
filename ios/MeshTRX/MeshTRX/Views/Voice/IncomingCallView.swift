import SwiftUI
import AudioToolbox

struct IncomingCallView: View {
    let call: IncomingCall
    var onAccept: () -> Void = {}
    var onReject: () -> Void = {}

    @State private var secondsLeft: Int = 30
    @State private var timer: Timer?
    @State private var pulseScale: CGFloat = 1.0

    private var callColor: Color {
        switch call.callType {
        case .all: return AppColors.blueAccent
        case .private: return AppColors.greenAccent
        case .group: return AppColors.amberAccent
        case .emergency: return AppColors.redAccent
        }
    }

    private var callBgColor: Color {
        switch call.callType {
        case .all: return AppColors.blueBg
        case .private: return AppColors.greenBg
        case .group: return AppColors.amberBg
        case .emergency: return AppColors.redBg
        }
    }

    private var callTypeLabel: String {
        switch call.callType {
        case .all: return "Общий вызов"
        case .private: return "Приватный вызов"
        case .group: return "Групповой вызов"
        case .emergency: return "SOS"
        }
    }

    var body: some View {
        ZStack {
            Color.black.opacity(0.95).ignoresSafeArea()

            VStack(spacing: 24) {
                Spacer()

                // Call type
                Text(callTypeLabel)
                    .font(.system(size: 16, weight: .medium))
                    .foregroundColor(callColor)
                    .textCase(.uppercase)

                // Caller info
                Text(call.callSign)
                    .font(.system(size: 36, weight: .bold))
                    .foregroundColor(.white)

                // Pulsating circle
                Circle()
                    .fill(callBgColor)
                    .frame(width: 120, height: 120)
                    .overlay(
                        Circle().stroke(callColor, lineWidth: 3)
                    )
                    .overlay(
                        Image(systemName: callIcon)
                            .font(.system(size: 40))
                            .foregroundColor(callColor)
                    )
                    .scaleEffect(pulseScale)
                    .animation(.easeInOut(duration: 0.8).repeatForever(autoreverses: true),
                               value: pulseScale)

                // RSSI
                if call.rssi != 0 {
                    Text("\(call.rssi) dBm")
                        .font(.system(size: 14, design: .monospaced))
                        .foregroundColor(AppColors.textMuted)
                }

                // Countdown
                Text("\(secondsLeft)с")
                    .font(.system(size: 14, design: .monospaced))
                    .foregroundColor(AppColors.textDim)

                Spacer()

                // Accept / Reject buttons
                HStack(spacing: 60) {
                    // Reject
                    Button {
                        stopTimer()
                        onReject()
                    } label: {
                        Image(systemName: "phone.down.fill")
                            .font(.system(size: 28))
                            .foregroundColor(.white)
                            .frame(width: 64, height: 64)
                            .background(AppColors.redAccent)
                            .clipShape(Circle())
                    }

                    // Accept
                    Button {
                        stopTimer()
                        onAccept()
                    } label: {
                        Image(systemName: "phone.fill")
                            .font(.system(size: 28))
                            .foregroundColor(.white)
                            .frame(width: 64, height: 64)
                            .background(AppColors.greenAccent)
                            .clipShape(Circle())
                    }
                }

                Spacer().frame(height: 40)
            }
        }
        .onAppear {
            pulseScale = 1.15
            startTimer()
            vibrate()
        }
        .onDisappear {
            stopTimer()
        }
    }

    private var callIcon: String {
        switch call.callType {
        case .emergency: return "exclamationmark.triangle.fill"
        case .private: return "person.fill"
        case .group: return "person.3.fill"
        case .all: return "megaphone.fill"
        }
    }

    private func startTimer() {
        secondsLeft = 30
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { _ in
            if secondsLeft > 0 {
                secondsLeft -= 1
                if secondsLeft % 5 == 0 { vibrate() }
            } else {
                stopTimer()
                onReject() // auto-reject after 30s
            }
        }
    }

    private func stopTimer() {
        timer?.invalidate()
        timer = nil
    }

    private func vibrate() {
        let pattern: Int
        switch call.callType {
        case .emergency: pattern = 7
        case .private: pattern = 3
        case .group: pattern = 2
        case .all: pattern = 1
        }
        for i in 0..<pattern {
            DispatchQueue.main.asyncAfter(deadline: .now() + Double(i) * 0.3) {
                AudioServicesPlaySystemSound(kSystemSoundID_Vibrate)
            }
        }
    }
}
