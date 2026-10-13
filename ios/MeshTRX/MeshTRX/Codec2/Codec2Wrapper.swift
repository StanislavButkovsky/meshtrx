import Foundation

class Codec2Wrapper {
    private var codec: OpaquePointer?
    let samplesPerFrame: Int = 160   // 20ms @ 8kHz
    let bytesPerFrame: Int = 8       // 3200 bps mode
    // 4 кадра = 32 байта = 80 мс — ровно столько прошивка кладёт в один
    // LoRa-пакет (CODEC2_FRAMES_PER_PKT). Было 8: рация брала первую половину
    // пакета, а принятые 32 байта iPhone не узнавал и выбрасывал.
    let framesPerPacket: Int = 4

    // Номер режима, а не битрейт: CODEC2_MODE_3200 = 0. Здесь стояло 3200 —
    // такого режима нет, codec2_create возвращал nil, и iPhone не кодировал и
    // не декодировал ни одного кадра: в эфир уходила пустота, а с приёма
    // звучал только сигнал конца передачи.
    init(mode: Int32 = CODEC2_MODE_3200) {
        codec = codec2_create(mode)
        if codec == nil { NSLog("Codec2: не удалось создать кодек, режим %d", mode) }
    }

    deinit {
        if let codec = codec {
            codec2_destroy(codec)
        }
    }

    /// Encode single frame: 160 samples → 8 bytes
    func encode(pcm: [Int16]) -> Data {
        guard let codec = codec else { return Data() }
        var samples = pcm
        var encoded = [UInt8](repeating: 0, count: bytesPerFrame)
        codec2_encode(codec, &encoded, &samples)
        return Data(encoded)
    }

    /// Decode single frame: 8 bytes → 160 samples
    func decode(encoded: Data) -> [Int16] {
        guard let codec = codec else { return [] }
        var samples = [Int16](repeating: 0, count: samplesPerFrame)
        var bytes = [UInt8](encoded)
        codec2_decode(codec, &samples, &bytes)
        return samples
    }

    /// Encode full packet: 1280 samples → 64 bytes (8 frames)
    func encodePacket(pcm: [Int16]) -> Data {
        var result = Data()
        for i in 0..<framesPerPacket {
            let offset = i * samplesPerFrame
            let end = min(offset + samplesPerFrame, pcm.count)
            guard offset < pcm.count else { break }
            let frame = Array(pcm[offset..<end])
            result.append(encode(pcm: frame))
        }
        return result
    }

    /// Decode full packet: 64 bytes → 1280 samples (8 frames)
    func decodePacket(encoded: Data) -> [Int16] {
        // Сначала в массив: срез Data хранит индексы исходника, и обращение
        // к нему с нуля роняло приложение при проигрывании голосовой записи.
        let bytes = [UInt8](encoded)
        var result = [Int16]()
        for i in 0..<framesPerPacket {
            let offset = i * bytesPerFrame
            let end = min(offset + bytesPerFrame, bytes.count)
            guard offset < bytes.count else { break }
            result.append(contentsOf: decode(encoded: Data(bytes[offset..<end])))
        }
        return result
    }
}
