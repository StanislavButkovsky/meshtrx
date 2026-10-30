import SwiftUI

/// Оформление, как в Android 4.4.31: основное сине-жёлтое и «Хаки».
/// Выбор хранится в UserDefaults («ui_theme»), корень приложения
/// перестраивается при смене, поэтому цвета — вычисляемые.
enum AppTheme: String, CaseIterable {
    case main, khaki
    static let key = "ui_theme"
    static var current: AppTheme { AppTheme(rawValue: UserDefaults.standard.string(forKey: key) ?? "") ?? .main }
}

private func pick(_ main: UInt32, _ khaki: UInt32) -> Color {
    Color(hex: AppTheme.current == .khaki ? khaki : main)
}

enum AppColors {
    // Роли
    static var bg: Color            { pick(0x0E1A33, 0x1F2318) }
    static var bgDeep: Color        { pick(0x0B152B, 0x191C13) }
    static var surface: Color       { pick(0x14233F, 0x2A2F20) }
    static var elevated: Color      { pick(0x1B2C4E, 0x343A28) }
    static var border: Color        { pick(0x22355A, 0x3E4530) }
    static var borderStrong: Color  { pick(0x2A3F66, 0x4A523A) }
    static var accent: Color        { pick(0xFABE14, 0xCE9C3A) }   // «Говорить», выбранное
    static var onAccent: Color      { pick(0x101E3C, 0x1F2318) }
    static var secondary: Color     { pick(0x5B9BFF, 0xA3B56A) }
    static var secondaryBg: Color   { pick(0x1D3566, 0x3A4425) }
    static var secondaryBorder: Color { pick(0x2F55A0, 0x5C6B38) }
    static var text: Color          { pick(0xEEF2FA, 0xEAE2C8) }
    static var text2: Color         { pick(0xC7D0E2, 0xD2CAB0) }
    static var textFaint: Color     { pick(0x7F8FB0, 0x8A846C) }

    // Прежние имена — на роли, как в Android
    static var bgPrimary: Color     { bg }
    static var bgSurface: Color     { surface }
    static var bgElevated: Color    { elevated }
    static var bgBorder: Color      { border }
    static var borderSubtle: Color  { borderStrong }
    static var borderMuted: Color   { border }
    static var greenAccent: Color   { accent }
    static var greenBg: Color       { secondaryBg }
    static var greenBorder: Color   { secondaryBorder }
    static var greenDim: Color      { textFaint }
    static var blueAccent: Color    { secondary }
    static var blueBg: Color        { secondaryBg }
    static var blueBorder: Color    { secondaryBorder }
    static var textPrimary: Color   { text }
    static var textSecondary: Color { text2 }
    static var textMuted: Color     { pick(0x8E9AB3, 0xA39C82) }
    static var textDim: Color       { pick(0x5C6A88, 0x6F6A57) }
    static var textLabel: Color     { textFaint }
    static var navBg: Color         { bgDeep }
    static var navBorder: Color     { elevated }

    static let okGreen      = Color(hex: 0x5CCB8A)   // хороший сигнал

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

    static func rssiColor(_ rssi: Int) -> Color {
        if rssi >= -60 { return okGreen }
        if rssi >= -90 { return amberAccent }
        return redAccent
    }

    static func rssiBg(_ rssi: Int) -> Color {
        if rssi >= -60 { return secondaryBg }
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
