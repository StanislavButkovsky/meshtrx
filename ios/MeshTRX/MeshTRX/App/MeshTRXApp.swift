import SwiftUI

@main
struct MeshTRXApp: App {
    @StateObject private var appState = AppState()
    @StateObject private var controller: MeshTRXController

    init() {
        let state = AppState()
        _appState = StateObject(wrappedValue: state)
        _controller = StateObject(wrappedValue: MeshTRXController(appState: state))
    }

    var body: some Scene {
        WindowGroup {
            SplashView()
                .environmentObject(appState)
                .environmentObject(controller)
                .fullScreenCover(item: $appState.incomingCall) { call in
                    IncomingCallView(
                        call: call,
                        onAccept: { controller.acceptCall(call) },
                        onReject: { controller.rejectCall(call) }
                    )
                }
        }
    }
}
