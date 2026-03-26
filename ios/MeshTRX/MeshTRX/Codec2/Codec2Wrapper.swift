import Foundation

class Codec2Wrapper {
    private var codec: OpaquePointer?
    let samplesPerFrame: Int = 160   // 20ms @ 8kHz
    let bytesPerFrame: Int = 8       // 3200 bps mode
    let framesPerPacket: Int = 8     // 8 frames = 64 bytes = 1280 samples

    init(mode: Int32 = 3200) {  // CODEC2_MODE_3200
        codec = codec2_create(mode)
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
        var result = [Int16]()
        for i in 0..<framesPerPacket {
            let offset = i * bytesPerFrame
            let end = min(offset + bytesPerFrame, encoded.count)
            guard offset < encoded.count else { break }
            let frame = encoded[offset..<end]
            result.append(contentsOf: decode(encoded: Data(frame)))
        }
        return result
    }
}
