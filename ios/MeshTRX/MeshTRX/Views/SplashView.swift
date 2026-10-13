import SwiftUI

struct SplashView: View {
    @State private var isActive = false
    @State private var logoOpacity: Double = 0
    @State private var versionOpacity: Double = 0

    private var appVersion: String {
        Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String ?? ""
    }

    var body: some View {
        if isActive {
            MainTabView()
        } else {
            // Как в Android: камуфляжный фон, логотип по центру с отступами,
            // версия зелёным внизу. Раньше здесь была системная картинка антенны
            // и вписанная руками «v1.0.0».
            ZStack {
                Image("SplashLogo")
                    .resizable()
                    .scaledToFit()
                    .padding(24)
                    .opacity(logoOpacity)

                VStack {
                    Spacer()
                    Text("v\(appVersion)")
                        .font(.system(size: 14))
                        .foregroundColor(AppColors.greenAccent)
                        .opacity(versionOpacity)
                        .padding(.bottom, 48)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            // Фон — фоном, а не слоем: растянутая на весь экран картинка в слое
            // раздвигает раскладку, и логотип съезжает в сторону.
            .background(
                Image("CamoBg")
                    .resizable()
                    .scaledToFill()
                    .ignoresSafeArea()
            )
            .onAppear {
                withAnimation(.easeIn(duration: 0.8)) {
                    logoOpacity = 1
                }
                withAnimation(.easeIn(duration: 0.6).delay(0.4)) {
                    versionOpacity = 1
                }
                DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
                    withAnimation(.easeInOut(duration: 0.3)) {
                        isActive = true
                    }
                }
            }
        }
    }
}
