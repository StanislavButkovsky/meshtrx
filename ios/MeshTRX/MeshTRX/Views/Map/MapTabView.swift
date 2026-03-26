import SwiftUI
import MapKit

struct MapTabView: View {
    @EnvironmentObject var appState: AppState
    @StateObject private var locationManager = LocationManager()
    @State private var showRadar = false

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
                        MapContentView(appState: appState, locationManager: locationManager)
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
        .onAppear {
            locationManager.requestPermission()
        }
        .onChange(of: locationManager.latitude) { lat in
            appState.myLat = lat
        }
        .onChange(of: locationManager.longitude) { lon in
            appState.myLon = lon
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

    func makeUIView(context: Context) -> MKMapView {
        let mapView = MKMapView()
        mapView.showsUserLocation = true
        mapView.mapType = .standard
        return mapView
    }

    func updateUIView(_ mapView: MKMapView, context: Context) {
        // Remove old annotations (except user location)
        let old = mapView.annotations.filter { !($0 is MKUserLocation) }
        mapView.removeAnnotations(old)
        mapView.removeOverlays(mapView.overlays)

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
                mapView.addOverlay(polyline)
            }
        }

        // Center on first update
        if let lat = myLat, let lon = myLon, mapView.annotations.count <= 2 {
            let region = MKCoordinateRegion(
                center: CLLocationCoordinate2D(latitude: lat, longitude: lon),
                latitudinalMeters: 2000, longitudinalMeters: 2000
            )
            mapView.setRegion(region, animated: false)
        }
    }

    private func formatAge(_ sec: Int64) -> String {
        if sec < 60 { return "\(sec)с" }
        if sec < 3600 { return "\(sec / 60)мин" }
        return "\(sec / 3600)ч"
    }
}
