import SwiftUI

struct RadarView: View {
    @EnvironmentObject var appState: AppState
    let heading: Double  // degrees from north

    @State private var rangeIndex: Int = 5
    private let ranges: [Double] = [0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0, 100.0]
    // Как в Android: масштаб сам подбирается по самому дальнему абоненту, пока
    // человек не нажал + или −; ☾/☀ — яркая палитра для солнца
    @State private var autoScale = true
    @State private var highContrast = false

    private var gridColor: Color { highContrast ? Color(hex: 0x2a6a2a) : AppColors.greenBg }
    private var gridTextColor: Color { highContrast ? Color(hex: 0x55cc55) : AppColors.greenDim }
    private var crossColor: Color { highContrast ? Color(hex: 0x1a3a1a) : Color(hex: 0x0d1f0d) }
    private var nameColor: Color { highContrast ? .white : AppColors.greenAccent }
    private var distColor: Color { highContrast ? Color(hex: 0xccffcc) : Color(hex: 0x88cc88) }

    /// Диапазон: при автомасштабе — ближайший, в который влезает самый дальний
    private var effectiveRangeIndex: Int {
        guard autoScale, let myLat = appState.myLat, let myLon = appState.myLon else { return rangeIndex }
        let far = appState.peers.compactMap { p -> Double? in
            guard let la = p.lat, let lo = p.lon else { return nil }
            return distanceKm(myLat, myLon, la, lo)
        }.max()
        guard let d = far else { return rangeIndex }
        return ranges.firstIndex { $0 >= d * 1.1 } ?? ranges.count - 1
    }

    var body: some View {
        ZStack {
            Color.black.ignoresSafeArea()

            TimelineView(.animation(minimumInterval: 1.0 / 20)) { timeline in
                Canvas { context, size in
                    drawRadar(context: context, size: size, date: timeline.date)
                }
            }

            // Zoom buttons
            VStack {
                Spacer()
                HStack {
                    zoomButton(highContrast ? "☀" : "☾") { highContrast.toggle() }
                        .padding(.leading, 12)
                        .padding(.bottom, 12)
                    Spacer()
                    VStack(spacing: 8) {
                        zoomButton("+") {
                            rangeIndex = effectiveRangeIndex; autoScale = false
                            if rangeIndex > 0 { rangeIndex -= 1 }
                        }
                        zoomButton("−") {
                            rangeIndex = effectiveRangeIndex; autoScale = false
                            if rangeIndex < ranges.count - 1 { rangeIndex += 1 }
                        }
                    }
                    .padding(.trailing, 12)
                    .padding(.bottom, 12)
                }
            }
        }
    }

    private func zoomButton(_ label: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(label)
                .font(.system(size: 20, weight: .bold))
                .foregroundColor(AppColors.textSecondary)
                .frame(width: 40, height: 40)
                .background(AppColors.bgElevated)
                .cornerRadius(8)
                .overlay(RoundedRectangle(cornerRadius: 8).stroke(AppColors.greenBg, lineWidth: 2))
        }
    }

    // MARK: - Drawing

    private func drawRadar(context: GraphicsContext, size: CGSize, date: Date) {
        let cx = size.width / 2
        let cy = size.height / 2
        let radius = min(cx, cy) * 0.85
        let rangeKm = ranges[effectiveRangeIndex]

        // Cross
        var crossH = Path(); crossH.move(to: CGPoint(x: cx - radius, y: cy)); crossH.addLine(to: CGPoint(x: cx + radius, y: cy))
        var crossV = Path(); crossV.move(to: CGPoint(x: cx, y: cy - radius)); crossV.addLine(to: CGPoint(x: cx, y: cy + radius))
        context.stroke(crossH, with: .color(crossColor), lineWidth: 1)
        context.stroke(crossV, with: .color(crossColor), lineWidth: 1)

        // Rotate for compass
        var rotated = context
        rotated.translateBy(x: cx, y: cy)
        rotated.rotate(by: .degrees(-heading))
        rotated.translateBy(x: -cx, y: -cy)

        // Range rings
        for i in 1...4 {
            let ringKm = rangeKm * Double(i) / 4.0
            let r = ringKm / rangeKm * radius
            var ring = Path()
            ring.addArc(center: CGPoint(x: cx, y: cy), radius: r,
                        startAngle: .zero, endAngle: .degrees(360), clockwise: false)
            rotated.stroke(ring, with: .color(gridColor), lineWidth: 1)

            // Distance label
            if i < 4 || true {
                let label = formatDist(ringKm)
                let resolved = rotated.resolve(Text(label).font(.system(size: 10)).foregroundColor(gridTextColor))
                rotated.draw(resolved, at: CGPoint(x: cx + 4, y: cy - r + 10))
            }
        }

        // Cardinal points
        let cardinalFont = Font.system(size: 12, weight: .bold)
        let nResolved = rotated.resolve(Text("С").font(cardinalFont).foregroundColor(AppColors.redAccent))
        rotated.draw(nResolved, at: CGPoint(x: cx, y: cy - radius - 10), anchor: .center)
        let sResolved = rotated.resolve(Text("Ю").font(cardinalFont).foregroundColor(gridTextColor))
        rotated.draw(sResolved, at: CGPoint(x: cx, y: cy + radius + 12), anchor: .center)
        let wResolved = rotated.resolve(Text("З").font(cardinalFont).foregroundColor(gridTextColor))
        rotated.draw(wResolved, at: CGPoint(x: cx - radius - 10, y: cy), anchor: .center)
        let eResolved = rotated.resolve(Text("В").font(cardinalFont).foregroundColor(gridTextColor))
        rotated.draw(eResolved, at: CGPoint(x: cx + radius + 10, y: cy), anchor: .center)

        // Peers
        if let myLat = appState.myLat, let myLon = appState.myLon {
            let now = Date()
            for peer in appState.peers {
                guard let pLat = peer.lat, let pLon = peer.lon else { continue }
                let dist = distanceKm(myLat, myLon, pLat, pLon)
                let bearing = bearingDeg(myLat, myLon, pLat, pLon)
                let ageSec = Int64(now.timeIntervalSince1970 * 1000 - Double(peer.lastSeenMs)) / 1000

                let r = min(dist / rangeKm * radius, radius)
                let angleRad = (bearing - 90) * .pi / 180
                let px = cx + r * cos(angleRad)
                let py = cy + r * sin(angleRad)

                // Color by age
                let dotColor: Color
                if ageSec < 300 { dotColor = AppColors.greenAccent }
                else if ageSec < 600 { dotColor = AppColors.amberAccent }
                else { dotColor = AppColors.redAccent }

                // Dot
                var dot = Path()
                dot.addArc(center: CGPoint(x: px, y: py), radius: 4,
                           startAngle: .zero, endAngle: .degrees(360), clockwise: false)
                // Яркость по уровню сигнала, как в Android: слабый — бледнее
                let alpha = min(max(Double(peer.rssi + 120), 10), 100) / 100
                rotated.fill(dot, with: .color(dotColor.opacity(max(alpha, 0.31))))

                // Callsign
                let nameResolved = rotated.resolve(
                    Text(peer.callSign).font(.system(size: 11, weight: .bold)).foregroundColor(nameColor))
                rotated.draw(nameResolved, at: CGPoint(x: px, y: py - 10), anchor: .center)

                // Distance + RSSI
                let distLabel = "\(formatDist(dist)) \(peer.rssi)dBm"
                let distResolved = rotated.resolve(
                    Text(distLabel).font(.system(size: 9)).foregroundColor(distColor))
                rotated.draw(distResolved, at: CGPoint(x: px, y: py + 12), anchor: .center)
            }
        }

        // Center dot (pulsating)
        let pulsePhase = date.timeIntervalSince1970.truncatingRemainder(dividingBy: 2) * .pi
        let pulseR = 4.0 + 3.0 * sin(pulsePhase)
        var centerDot = Path()
        centerDot.addArc(center: CGPoint(x: cx, y: cy), radius: pulseR,
                         startAngle: .zero, endAngle: .degrees(360), clockwise: false)
        context.fill(centerDot, with: .color(AppColors.greenAccent))

        var centerGlow = Path()
        centerGlow.addArc(center: CGPoint(x: cx, y: cy), radius: pulseR * 2.5,
                          startAngle: .zero, endAngle: .degrees(360), clockwise: false)
        context.fill(centerGlow, with: .color(AppColors.greenAccent.opacity(0.15)))

        // Range label
        let rangeTxt = context.resolve(
            Text(formatDist(rangeKm)).font(.system(size: 11, design: .monospaced)).foregroundColor(AppColors.textDim))
        context.draw(rangeTxt, at: CGPoint(x: cx, y: size.height - 16), anchor: .center)
    }

    // MARK: - Helpers

    private func formatDist(_ km: Double) -> String {
        km < 1 ? "\(Int(km * 1000))м" : String(format: "%.1fкм", km)
    }

    private func bearingDeg(_ lat1: Double, _ lon1: Double, _ lat2: Double, _ lon2: Double) -> Double {
        let dLon = (lon2 - lon1) * .pi / 180
        let la1 = lat1 * .pi / 180
        let la2 = lat2 * .pi / 180
        let y = sin(dLon) * cos(la2)
        let x = cos(la1) * sin(la2) - sin(la1) * cos(la2) * cos(dLon)
        return (atan2(y, x) * 180 / .pi + 360).truncatingRemainder(dividingBy: 360)
    }
}
