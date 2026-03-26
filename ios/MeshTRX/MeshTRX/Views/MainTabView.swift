import SwiftUI

struct MainTabView: View {
    @EnvironmentObject var appState: AppState
    @State private var selectedTab = 0

    var body: some View {
        TabView(selection: $selectedTab) {
            VoiceView()
                .tabItem {
                    Image(systemName: "mic.fill")
                    Text("Voice")
                }
                .tag(0)

            MessagesView()
                .tabItem {
                    Image(systemName: "message.fill")
                    Text("Messages")
                }
                .badge(appState.unreadMessages)
                .tag(1)

            FilesView()
                .tabItem {
                    Image(systemName: "doc.fill")
                    Text("Files")
                }
                .tag(2)

            MapTabView()
                .tabItem {
                    Image(systemName: "map.fill")
                    Text("Map")
                }
                .tag(3)

            SettingsView()
                .tabItem {
                    Image(systemName: "gear")
                    Text("Settings")
                }
                .tag(4)
        }
        .accentColor(.green)
    }
}
