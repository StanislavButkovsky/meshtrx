import SwiftUI
import MapKit

struct MapTabView: View {
    @EnvironmentObject var controller: MeshTRXController
    var body: some View {
        MapTabContent(locationManager: controller.locationManager)
    }
}

struct MapTabContent: View {
    @EnvironmentObject var appState: AppState
    @ObservedObject var locationManager: LocationManager
    @State private var showRadar = false
    // Кнопки «Я» и «Все», как в Android: счётчики-сигналы для карты
    @State private var centerMeTick = 0
    @State private var showAllTick = 0

    var body: some View {
        ZStack {
            AppColors.bgPrimary.ignoresSafeArea()

            VStack(spacing: 0) {
                // Tab bar: Map / Radar
                tabBar

                // Content
                ZStack {
                    if showRadar {
                        RadarView(heading: locationManager.heading)
                            .environmentObject(appState)
                    } else {
                        MapContentView(appState: appState, locationManager: locationManager,
                                       centerMeTick: centerMeTick, showAllTick: showAllTick)
                        VStack(spacing: 12) {
                            Spacer()
                            HStack {
                                Spacer()
                                VStack(spacing: 12) {
                                    mapButton("Я") { centerMeTick += 1 }
                                    mapButton("ВСЕ") { showAllTick += 1 }
                                }
                                .padding(16)
                            }
                        }
                    }

                    // GPS status overlay
                    VStack {
                        HStack {
                            gpsStatusLabel
                                .padding(6)
                                .background(Color.black.opacity(0.8))
                                .cornerRadius(6)
                            Spacer()
                        }
                        .padding(8)
                        Spacer()
                    }
                }
            }
        }
    }

    private func mapButton(_ title: String, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 13, weight: .bold))
                .foregroundColor(.white)
                .frame(width: 56, height: 56)
                .background(AppColors.greenBg)
                .clipShape(Circle())
                .overlay(Circle().stroke(AppColors.greenBorder, lineWidth: 1))
        }
    }

    // MARK: - Tab bar

    private var tabBar: some View {
        HStack(spacing: 0) {
            tabButton("Карта", selected: !showRadar) { showRadar = false }
            tabButton("Радар", selected: showRadar) { showRadar = true }
        }
        .background(AppColors.navBg)
    }

    private func tabButton(_ title: String, selected: Bool, action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(title)
                .font(.system(size: 14, weight: .medium))
                .foregroundColor(selected ? AppColors.greenAccent : AppColors.textMuted)
                .frame(maxWidth: .infinity)
                .padding(.vertical, 8)
                .background(selected ? AppColors.greenBg : AppColors.bgSurface)
        }
    }

    private var gpsStatusLabel: some View {
        Group {
            if let lat = appState.myLat, let lon = appState.myLon {
                Text(String(format: "GPS: %.5f, %.5f", lat, lon))
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundColor(AppColors.greenAccent)
            } else {
                Text("GPS: ожидание...")
                    .font(.system(size: 11, design: .monospaced))
                    .foregroundColor(AppColors.amberAccent)
            }
        }
    }
}

// MARK: - Map Content (MKMapView wrapper)

struct MapContentView: UIViewRepresentable {
    let appState: AppState
    let locationManager: LocationManager
    var centerMeTick: Int = 0
    var showAllTick: Int = 0

    /// Делегат карты. Без него MKMapView не рисует линии: раньше они
    /// добавлялись, но видно их не было.
    final class Coordinator: NSObject, MKMapViewDelegate {
        var didCenter = false
        var lastCenterTick = 0
        var lastShowAllTick = 0
        var lineColors: [ObjectIdentifier: UIColor] = [:]

        func mapView(_ mapView: MKMapView, rendererFor overlay: MKOverlay) -> MKOverlayRenderer {
            guard let line = overlay as? MKPolyline else { return MKOverlayRenderer(overlay: overlay) }
            let r = MKPolylineRenderer(polyline: line)
            r.strokeColor = lineColors[ObjectIdentifier(line)] ?? .systemGreen
            r.lineWidth = 2
            r.lineDashPattern = [6, 6]
            return r
        }
    }

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeUIView(context: Context) -> MKMapView {
        let mapView = MKMapView()
        mapView.showsUserLocation = true
        mapView.mapType = .standard
        mapView.delegate = context.coordinator
        return mapView
    }

    func updateUIView(_ mapView: MKMapView, context: Context) {
        // Remove old annotations (except user location)
        let old = mapView.annotations.filter { !($0 is MKUserLocation) }
        mapView.removeAnnotations(old)
        mapView.removeOverlays(mapView.overlays)
        context.coordinator.lineColors.removeAll()

        let myLat = appState.myLat
        let myLon = appState.myLon
        let now = Date().timeIntervalSince1970 * 1000

        for peer in appState.peers {
            guard let pLat = peer.lat, let pLon = peer.lon else { continue }
            let ageSec = (Int64(now) - peer.lastSeenMs) / 1000

            // Annotation
            let annotation = MKPointAnnotation()
            annotation.coordinate = CLLocationCoordinate2D(latitude: pLat, longitude: pLon)

            let dist: String
            if let myLat = myLat, let myLon = myLon {
                let d = distanceKm(myLat, myLon, pLat, pLon)
                dist = d < 1 ? "\(Int(d * 1000))м" : String(format: "%.1fкм", d)
            } else {
                dist = "?"
            }
            annotation.title = peer.callSign
            annotation.subtitle = "\(peer.rssi)dBm · \(dist) · \(formatAge(ageSec))"
            mapView.addAnnotation(annotation)

            // Line to peer
            if let myLat = myLat, let myLon = myLon {
                let coords = [
                    CLLocationCoordinate2D(latitude: myLat, longitude: myLon),
                    CLLocationCoordinate2D(latitude: pLat, longitude: pLon)
                ]
                let polyline = MKPolyline(coordinates: coords, count: 2)
                // Цвет по давности: свежий — зелёный, до получаса — жёлтый, старше — серый
                context.coordinator.lineColors[ObjectIdentifier(polyline)] =
                    ageSec < 300 ? .systemGreen : ageSec < 1800 ? .systemOrange : .systemGray
                mapView.addOverlay(polyline)
            }
        }

        // На себе — один раз и по кнопке «Я». Раньше центрировалось при каждом
        // обновлении, и при одном абоненте карту было не сдвинуть.
        let c = context.coordinator
        if let lat = myLat, let lon = myLon, !c.didCenter || centerMeTick != c.lastCenterTick {
            c.didCenter = true
            c.lastCenterTick = centerMeTick
            mapView.setRegion(MKCoordinateRegion(
                center: CLLocationCoordinate2D(latitude: lat, longitude: lon),
                latitudinalMeters: 2000, longitudinalMeters: 2000), animated: true)
        }
        // «Все» — показать себя и всех абонентов с координатами
        if showAllTick != c.lastShowAllTick {
            c.lastShowAllTick = showAllTick
            mapView.showAnnotations(mapView.annotations, animated: true)
        }
    }

    private func formatAge(_ sec: Int64) -> String {
        if sec < 60 { return "\(sec)с" }
        if sec < 3600 { return "\(sec / 60)мин" }
        return "\(sec / 3600)ч"
    }
}
