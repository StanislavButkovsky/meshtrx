import Foundation
import os.log

/// VOX (Voice Operated Exchange) — автоматическое управление TX по голосу.
/// State machine: IDLE → ATTACK → ACTIVE → HANGTIME → IDLE
class VoxEngine {

    static let defaultThreshold: Int = 800     // 0-32767
    static let defaultHangtimeMs: Int64 = 800
    static let defaultAttackMs: Int64 = 50

    var threshold: Int = defaultThreshold
    var hangtimeMs: Int64 = defaultHangtimeMs
    var attackMs: Int64 = defaultAttackMs

    var onTxActivated: (() -> Void)?
    var onTxDeactivated: (() -> Void)?
    var onStateChanged: ((VoxState) -> Void)?

    private(set) var state: VoxState = .idle
    private var timerStart: Int64 = 0

    var isActive: Bool { state == .active || state == .hangtime }

    private let log = Logger(subsystem: "com.meshtrx.app", category: "VOX")

    func process(rms: Int) {
        let now = Int64(Date().timeIntervalSince1970 * 1000)
        let prevActive = isActive

        switch state {
        case .idle:
            if rms > threshold {
                timerStart = now
                state = .attack
                onStateChanged?(state)
            }

        case .attack:
            if rms < threshold {
                state = .idle
                onStateChanged?(state)
            } else if now - timerStart >= attackMs {
                state = .active
                onStateChanged?(state)
                log.info("TX activated (RMS=\(rms), threshold=\(self.threshold))")
            }

        case .active:
            if rms < threshold {
                timerStart = now
                state = .hangtime
                onStateChanged?(state)
            }

        case .hangtime:
            if rms > threshold {
                state = .active
                onStateChanged?(state)
            } else if now - timerStart >= hangtimeMs {
                state = .idle
                onStateChanged?(state)
                log.info("TX deactivated (hangtime expired)")
            }
        }

        if !prevActive && isActive {
            onTxActivated?()
        } else if prevActive && !isActive {
            onTxDeactivated?()
        }
    }

    func reset() {
        state = .idle
        timerStart = 0
    }
}
