import Foundation
import AVFoundation
import os.log

class AudioEngine {

    static let sampleRate: Double = 8000
    static let frameSamples: Int = 160       // 20ms (Codec2 3200)
    static let framesPerPacket: Int = 8
    static let packetSamples: Int = frameSamples * framesPerPacket  // 1280

    // MARK: - Callbacks

    var onAudioEncoded: ((Data) -> Void)?
    var onRmsLevel: ((Int) -> Void)?

    // MARK: - Settings

    var volumeBoost: Float = 2.0     // 1.0=normal, 3.0=max
    var squelchThreshold: Int = 0    // RMS threshold, 0=disabled

    // MARK: - State

    @Volatile var sendAudio = false
    private var isRecording = false
    private var isMonitoring = false
    private var isPlaying = false

    // MARK: - Audio components

    private let codec2 = Codec2Wrapper()
    private var engine: AVAudioEngine?
    private var playerNode: AVAudioPlayerNode?
    private var inputConverter: AVAudioConverter?

    // Record buffer
    private var packetBuffer = [Int16](repeating: 0, count: packetSamples)
    private var packetOffset = 0

    // Playback queue
    private let playbackLock = NSLock()
    private var playbackQueue: [AVAudioPCMBuffer] = []
    private var isSchedulingPlayback = false

    // Format
    private let monoFormat = AVAudioFormat(
        commonFormat: .pcmFormatInt16,
        sampleRate: sampleRate,
        channels: 1,
        interleaved: true
    )!

    private let playerFormat = AVAudioFormat(
        commonFormat: .pcmFormatFloat32,
        sampleRate: sampleRate,
        channels: 1,
        interleaved: true
    )!

    private let log = Logger(subsystem: "com.meshtrx.app", category: "Audio")

    // MARK: - Init

    func setup() {
        configureAudioSession()
        log.info("AudioEngine initialized")
    }

    private func configureAudioSession() {
        let session = AVAudioSession.sharedInstance()
        do {
            try session.setCategory(.playAndRecord, mode: .voiceChat, options: [.defaultToSpeaker, .allowBluetooth])
            try session.setPreferredSampleRate(AudioEngine.sampleRate)
            try session.setPreferredIOBufferDuration(0.02) // 20ms
            try session.setActive(true)
        } catch {
            log.error("Audio session setup failed: \(error.localizedDescription)")
        }

        // Handle interruptions
        NotificationCenter.default.addObserver(
            self, selector: #selector(handleInterruption(_:)),
            name: AVAudioSession.interruptionNotification, object: nil
        )
        NotificationCenter.default.addObserver(
            self, selector: #selector(handleRouteChange(_:)),
            name: AVAudioSession.routeChangeNotification, object: nil
        )
    }

    // MARK: - Recording (PTT)

    func startRecording() {
        guard !isRecording else { return }
        sendAudio = true
        startMicCapture()
        log.info("Recording started (PTT)")
    }

    func stopRecording() {
        sendAudio = false
        if !isMonitoring {
            stopMicCapture()
        }
        log.info("Recording stopped (PTT)")
    }

    // MARK: - VOX Monitoring

    func startVoxMonitoring() {
        guard !isMonitoring, !isRecording else { return }
        isMonitoring = true
        sendAudio = false
        startMicCapture()
        log.info("VOX monitoring started")
    }

    func stopVoxMonitoring() {
        isMonitoring = false
        sendAudio = false
        stopMicCapture()
        log.info("VOX monitoring stopped")
    }

    // MARK: - Mic capture

    private func startMicCapture() {
        guard !isRecording else { return }
        isRecording = true
        packetOffset = 0

        let engine = AVAudioEngine()
        self.engine = engine

        let inputNode = engine.inputNode
        let inputFormat = inputNode.outputFormat(forBus: 0)

        // Converter: device sample rate → 8kHz Int16 mono
        guard let converter = AVAudioConverter(from: inputFormat, to: monoFormat) else {
            log.error("Cannot create audio converter")
            return
        }
        self.inputConverter = converter

        // Tap at native rate, convert in callback
        let bufferSize = AVAudioFrameCount(inputFormat.sampleRate * 0.02) // 20ms
        inputNode.installTap(onBus: 0, bufferSize: bufferSize, format: inputFormat) { [weak self] buffer, _ in
            self?.processInputBuffer(buffer)
        }

        // Setup playback node on same engine
        let player = AVAudioPlayerNode()
        self.playerNode = player
        engine.attach(player)
        engine.connect(player, to: engine.mainMixerNode, format: playerFormat)

        do {
            try engine.start()
            if isPlaying { player.play() }
        } catch {
            log.error("Engine start failed: \(error.localizedDescription)")
        }
    }

    private func stopMicCapture() {
        isRecording = false
        engine?.inputNode.removeTap(onBus: 0)
        playerNode?.stop()
        engine?.stop()
        engine = nil
        playerNode = nil
        inputConverter = nil
        packetOffset = 0
    }

    private func processInputBuffer(_ buffer: AVAudioPCMBuffer) {
        guard let converter = inputConverter else { return }

        // Convert to 8kHz mono Int16
        let ratio = monoFormat.sampleRate / buffer.format.sampleRate
        let outputFrameCount = AVAudioFrameCount(Double(buffer.frameLength) * ratio)
        guard let outputBuffer = AVAudioPCMBuffer(pcmFormat: monoFormat, frameCapacity: outputFrameCount) else { return }

        var error: NSError?
        let status = converter.convert(to: outputBuffer, error: &error) { _, outStatus in
            outStatus.pointee = .haveData
            return buffer
        }

        guard status != .error, error == nil else { return }

        // Extract Int16 samples
        let frameCount = Int(outputBuffer.frameLength)
        guard frameCount > 0, let int16Data = outputBuffer.int16ChannelData else { return }

        let samples = Array(UnsafeBufferPointer(start: int16Data[0], count: frameCount))

        // RMS — always compute for VOX and VU meter
        var sum: Int64 = 0
        for s in samples { sum += Int64(s) * Int64(s) }
        let rms = Int(sqrt(Double(sum) / Double(frameCount)))
        onRmsLevel?(rms)

        // Encode only if sendAudio and above squelch
        if sendAudio {
            if squelchThreshold == 0 || rms > squelchThreshold {
                // Accumulate into packet buffer
                var remaining = samples
                while !remaining.isEmpty {
                    let space = AudioEngine.packetSamples - packetOffset
                    let toCopy = min(space, remaining.count)
                    packetBuffer.replaceSubrange(packetOffset..<packetOffset + toCopy,
                                                  with: remaining.prefix(toCopy))
                    packetOffset += toCopy
                    remaining = Array(remaining.dropFirst(toCopy))

                    if packetOffset >= AudioEngine.packetSamples {
                        let encoded = codec2.encodePacket(pcm: packetBuffer)
                        onAudioEncoded?(encoded)
                        packetOffset = 0
                    }
                }
            } else {
                packetOffset = 0 // silence — reset buffer
            }
        } else {
            packetOffset = 0
        }
    }

    // MARK: - Playback

    func startPlayback() {
        isPlaying = true
        if let player = playerNode, engine?.isRunning == true {
            player.play()
        }
        log.info("Playback started")
    }

    func stopPlayback() {
        isPlaying = false
        playerNode?.stop()
        playbackLock.lock()
        playbackQueue.removeAll()
        playbackLock.unlock()
        log.info("Playback stopped")
    }

    func playEncodedPacket(_ codec2Data: Data, isLastPacket: Bool = false) {
        if isLastPacket {
            playRogerBeep()
            return
        }

        let pcm = codec2.decodePacket(encoded: codec2Data)
        schedulePlayback(samples: applyVolumeBoost(pcm))
    }

    private func applyVolumeBoost(_ samples: [Int16]) -> [Int16] {
        guard volumeBoost != 1.0 else { return samples }
        return samples.map { sample in
            let amplified = Int(Float(sample) * volumeBoost)
            return Int16(clamping: amplified)
        }
    }

    private func playRogerBeep() {
        let duration = 0.08 // 80ms
        let sampleCount = Int(AudioEngine.sampleRate * duration)
        var beep = [Int16](repeating: 0, count: sampleCount)
        let freq1 = 1200.0
        let freq2 = 1600.0

        for i in 0..<sampleCount {
            let t = Double(i) / AudioEngine.sampleRate
            let fadeLen = sampleCount / 10
            let envelope: Float
            if i < fadeLen {
                envelope = Float(i) / Float(fadeLen)
            } else if i > sampleCount - fadeLen {
                envelope = Float(sampleCount - i) / Float(fadeLen)
            } else {
                envelope = 1.0
            }
            let sample = (sin(2 * .pi * freq1 * t) * 0.5 +
                          sin(2 * .pi * freq2 * t) * 0.5) * Double(envelope) * 16000 * Double(volumeBoost)
            beep[i] = Int16(clamping: Int(sample))
        }

        schedulePlayback(samples: beep)
    }

    private func schedulePlayback(samples: [Int16]) {
        guard let player = playerNode, isPlaying else { return }

        let frameCount = AVAudioFrameCount(samples.count)
        guard let buffer = AVAudioPCMBuffer(pcmFormat: playerFormat, frameCapacity: frameCount) else { return }
        buffer.frameLength = frameCount

        // Convert Int16 → Float32
        guard let floatData = buffer.floatChannelData else { return }
        for i in 0..<samples.count {
            floatData[0][i] = Float(samples[i]) / 32768.0
        }

        player.scheduleBuffer(buffer)
    }

    // MARK: - Route

    func routeToSpeaker() {
        try? AVAudioSession.sharedInstance().overrideOutputAudioPort(.speaker)
    }

    func routeToEarpiece() {
        try? AVAudioSession.sharedInstance().overrideOutputAudioPort(.none)
    }

    // MARK: - Interruptions

    @objc private func handleInterruption(_ notification: Notification) {
        guard let info = notification.userInfo,
              let typeValue = info[AVAudioSessionInterruptionTypeKey] as? UInt,
              let type = AVAudioSession.InterruptionType(rawValue: typeValue) else { return }

        switch type {
        case .began:
            log.info("Audio interruption began")
        case .ended:
            log.info("Audio interruption ended")
            try? AVAudioSession.sharedInstance().setActive(true)
            if isRecording {
                // Restart engine after interruption
                stopMicCapture()
                startMicCapture()
            }
        @unknown default: break
        }
    }

    @objc private func handleRouteChange(_ notification: Notification) {
        log.info("Audio route changed")
    }

    // MARK: - Cleanup

    func destroy() {
        stopVoxMonitoring()
        stopRecording()
        stopPlayback()
        engine?.stop()
        engine = nil
        NotificationCenter.default.removeObserver(self)
    }
}

// MARK: - @Volatile property wrapper (thread-safe Bool)

@propertyWrapper
struct Volatile {
    private var value: Bool
    private let lock = NSLock()

    var wrappedValue: Bool {
        get { lock.lock(); defer { lock.unlock() }; return value }
        set { lock.lock(); defer { lock.unlock() }; value = newValue }
    }

    init(wrappedValue: Bool) {
        self.value = wrappedValue
    }
}
