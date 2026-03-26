import SwiftUI

enum AppColors {
    // Background
    static let bgPrimary    = Color(hex: 0x141414)
    static let bgSurface    = Color(hex: 0x1a1a1a)
    static let bgElevated   = Color(hex: 0x222222)
    static let bgBorder     = Color(hex: 0x2a2a2a)
    static let borderSubtle = Color(hex: 0x333333)
    static let borderMuted  = Color(hex: 0x252525)

    // Green
    static let greenAccent  = Color(hex: 0x4ade80)
    static let greenBg      = Color(hex: 0x1a3a1a)
    static let greenBorder  = Color(hex: 0x2a5a2a)
    static let greenDim     = Color(hex: 0x2d5a2d)

    // Blue
    static let blueAccent   = Color(hex: 0x5ba3e8)
    static let blueBg       = Color(hex: 0x1e3a5f)
    static let blueBorder   = Color(hex: 0x2a5a8f)

    // Red
    static let redAccent    = Color(hex: 0xf87171)
    static let redTx        = Color(hex: 0xff6b6b)
    static let redBg        = Color(hex: 0x3a0e0e)
    static let redBorder    = Color(hex: 0x6b1a1a)
    static let redTxBg      = Color(hex: 0x4a0a0a)
    static let redTxBorder  = Color(hex: 0xaa2a2a)

    // Amber
    static let amberAccent  = Color(hex: 0xf59e0b)
    static let amberBg      = Color(hex: 0x2a1f0a)

    // Text
    static let textPrimary   = Color(hex: 0xe8e8e8)
    static let textSecondary = Color(hex: 0xcccccc)
    static let textMuted     = Color(hex: 0x888888)
    static let textDim       = Color(hex: 0x555555)
    static let textLabel     = Color(hex: 0x444444)

    // Nav
    static let navBg         = Color(hex: 0x111111)
    static let navBorder     = Color(hex: 0x222222)

    static func rssiColor(_ rssi: Int) -> Color {
        if rssi >= -60 { return greenAccent }
        if rssi >= -90 { return amberAccent }
        return redAccent
    }

    static func rssiBg(_ rssi: Int) -> Color {
        if rssi >= -60 { return greenBg }
        if rssi >= -90 { return amberBg }
        return redBg
    }
}

// MARK: - Color hex init

extension Color {
    init(hex: UInt32, alpha: Double = 1.0) {
        self.init(
            .sRGB,
            red: Double((hex >> 16) & 0xFF) / 255,
            green: Double((hex >> 8) & 0xFF) / 255,
            blue: Double(hex & 0xFF) / 255,
            opacity: alpha
        )
    }
}
