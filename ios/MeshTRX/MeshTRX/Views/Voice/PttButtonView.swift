import SwiftUI

enum PttState {
    case idle, tx, rx, voxIdle, voxTx
}

struct PttButtonView: View {
    let state: PttState
    let rmsLevel: Float  // 0.0 - 1.0

    var onPttDown: () -> Void = {}
    var onPttUp: () -> Void = {}

    @State private var waves: [Wave] = []
    @State private var lastWaveTime: Date = .distantPast

    private struct Wave: Identifiable {
        let id = UUID()
        let startTime: Date
        var initialAlpha: Double
    }

    var body: some View {
        let scheme = colorScheme

        TimelineView(.animation(minimumInterval: 1.0 / 60)) { timeline in
            Canvas { context, size in
                let cx = size.width / 2
                let cy = size.height / 2
                let outerR = min(cx, cy) - 4
                let innerR = outerR - 8
                let now = timeline.date

                // Waves (behind button)
                if scheme.waveColor != .clear {
                    for wave in waves {
                        let age = now.timeIntervalSince(wave.startTime)
                        let progress = age / 1.5
                        guard progress <= 1.0 else { continue }
                        let waveR = outerR + CGFloat(progress) * outerR * 1.5
                        let alpha = (1.0 - progress) * wave.initialAlpha
                        var path = Path()
                        path.addArc(center: CGPoint(x: cx, y: cy), radius: waveR,
                                    startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                        context.stroke(path, with: .color(scheme.waveColor.opacity(alpha)), lineWidth: 2)
                    }
                }

                // Outer ring
                var ringPath = Path()
                ringPath.addArc(center: CGPoint(x: cx, y: cy), radius: outerR,
                                startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                context.stroke(ringPath, with: .color(scheme.ringColor), lineWidth: 3)

                // Внутренний круг — «стеклянная» кнопка, как в Android 4.4.31:
                // тень снизу, тёмный ободок, светлый диск с окантовкой, блик сверху
                let base = scheme.bgColor
                let shadowR = innerR + 9
                var shadow = Path()
                shadow.addArc(center: CGPoint(x: cx, y: cy + 5), radius: shadowR,
                              startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                context.fill(shadow, with: .radialGradient(
                    Gradient(stops: [.init(color: .black.opacity(0.33), location: 0.85),
                                     .init(color: .clear, location: 1)]),
                    center: CGPoint(x: cx, y: cy + 5), startRadius: 0, endRadius: shadowR))
                var innerPath = Path()
                innerPath.addArc(center: CGPoint(x: cx, y: cy), radius: innerR,
                                 startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                context.fill(innerPath, with: .linearGradient(
                    Gradient(colors: [base.shade(1.02), base.shade(0.70)]),
                    startPoint: CGPoint(x: cx, y: cy - innerR), endPoint: CGPoint(x: cx, y: cy + innerR)))
                let discR = innerR * 0.88
                var disc = Path()
                disc.addArc(center: CGPoint(x: cx, y: cy), radius: discR,
                            startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                context.fill(disc, with: .radialGradient(
                    Gradient(colors: [base.shade(1.10), base.shade(0.92)]),
                    center: CGPoint(x: cx, y: cy + discR * 0.25), startRadius: 0, endRadius: discR * 1.05))
                context.stroke(disc, with: .color(base.shade(1.35).opacity(0.67)), lineWidth: 1.5)
                let gTop = cy - discR * 0.92, gBot = cy - discR * 0.22
                let gloss = Path(ellipseIn: CGRect(x: cx - discR * 0.62, y: gTop,
                                                   width: discR * 1.24, height: gBot - gTop))
                context.fill(gloss, with: .linearGradient(
                    Gradient(colors: [.white.opacity(0.44), .white.opacity(0)]),
                    startPoint: CGPoint(x: cx, y: gTop), endPoint: CGPoint(x: cx, y: gBot)))

                // Main label
                let mainFont = Font.system(size: 17, weight: .bold)
                context.drawText(scheme.label, at: CGPoint(x: cx, y: cy - 4),
                                 color: scheme.textColor, font: mainFont)

                // Sub label
                if !scheme.subLabel.isEmpty {
                    let subFont = Font.system(size: 11)
                    context.drawText(scheme.subLabel, at: CGPoint(x: cx, y: cy + 16),
                                     color: scheme.subTextColor, font: subFont)
                }
            }
            .onChange(of: timeline.date) { newDate in
                updateWaves(now: newDate)
            }
        }
        .frame(width: 200, height: 200)
        .gesture(
            DragGesture(minimumDistance: 0)
                .onChanged { _ in onPttDown() }
                .onEnded { _ in onPttUp() }
        )
    }

    private func updateWaves(now: Date) {
        // Remove expired waves
        waves.removeAll { now.timeIntervalSince($0.startTime) > 1.5 }

        // Add new waves during TX
        if (state == .tx || state == .voxTx) && rmsLevel > 0.05 {
            let interval = max(0.08, 0.3 - Double(rmsLevel) * 0.2)
            if now.timeIntervalSince(lastWaveTime) > interval {
                let alpha = Double(min(max(rmsLevel * 0.8, 0.15), 0.8))
                waves.append(Wave(startTime: now, initialAlpha: alpha))
                lastWaveTime = now
            }
        }
    }

    private var colorScheme: PttColorScheme {
        switch state {
        case .idle:
            return PttColorScheme(
                bgColor: AppColors.accent, borderColor: AppColors.accent,
                ringColor: AppColors.bgBorder, textColor: AppColors.onAccent,
                label: "ГОВОРИТЬ", subLabel: "удержать", subTextColor: AppColors.onAccent,
                waveColor: .clear)
        case .tx:
            return PttColorScheme(
                bgColor: AppColors.redTxBg, borderColor: AppColors.redTxBorder,
                ringColor: AppColors.redBorder, textColor: AppColors.redTx,
                label: "ПЕРЕДАЧА", subLabel: "отпустить", subTextColor: AppColors.redBorder,
                waveColor: AppColors.redTx)
        case .rx:
            return PttColorScheme(
                bgColor: AppColors.secondaryBg, borderColor: AppColors.blueBorder,
                ringColor: AppColors.secondaryBorder, textColor: AppColors.blueAccent,
                label: "ПРИЁМ", subLabel: "", subTextColor: .clear,
                waveColor: AppColors.blueAccent)
        case .voxIdle:
            return PttColorScheme(
                bgColor: AppColors.accent, borderColor: AppColors.accent,
                ringColor: AppColors.bgBorder, textColor: AppColors.onAccent,
                label: "VOX", subLabel: "авто", subTextColor: AppColors.onAccent,
                waveColor: .clear)
        case .voxTx:
            return PttColorScheme(
                bgColor: AppColors.redTxBg, borderColor: AppColors.redTxBorder,
                ringColor: AppColors.redBorder, textColor: AppColors.redTx,
                label: "VOX TX", subLabel: "", subTextColor: .clear,
                waveColor: AppColors.redTx)
        }
    }
}

private struct PttColorScheme {
    let bgColor: Color
    let borderColor: Color
    let ringColor: Color
    let textColor: Color
    let label: String
    let subLabel: String
    let subTextColor: Color
    let waveColor: Color
}

// MARK: - Canvas text drawing helper

private extension GraphicsContext {
    func drawText(_ text: String, at point: CGPoint, color: Color, font: Font) {
        let resolved = resolve(Text(text).font(font).foregroundColor(color))
        draw(resolved, at: point, anchor: .center)
    }
}

// MARK: - Светлее / темнее — для объёма кнопки

extension Color {
    func shade(_ k: Double) -> Color {
        var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
        UIColor(self).getRed(&r, green: &g, blue: &b, alpha: &a)
        func ch(_ v: CGFloat) -> Double { min(1, max(0, Double(v) * k)) }
        return Color(.sRGB, red: ch(r), green: ch(g), blue: ch(b), opacity: Double(a))
    }
}
