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
            // Как в Android 4.4.31: фон темы, новый знак, «Mesh» + жёлтое «TRX»,
            // подпись и версия внизу; в «Хаки» — оливковый фон и знак в охре.
            let khaki = AppTheme.current == .khaki
            ZStack {
                VStack(spacing: 0) {
                    Image(khaki ? "SplashMarkKhaki" : "SplashMark")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 200, height: 200)
                    (Text("Mesh").foregroundColor(AppColors.text) +
                     Text("TRX").foregroundColor(AppColors.accent))
                        .font(.system(size: 38, weight: .bold))
                        .padding(.top, 4)
                    Text("Рация без интернета и сотовой связи")
                        .font(.system(size: 15))
                        .foregroundColor(AppColors.textMuted)
                        .padding(.top, 6)
                }
                .opacity(logoOpacity)
                .scaleEffect(0.92 + 0.08 * logoOpacity)

                VStack {
                    Spacer()
                    Text("v\(appVersion)")
                        .font(.system(size: 13))
                        .foregroundColor(AppColors.textFaint)
                        .opacity(versionOpacity)
                        .padding(.bottom, 48)
                }
            }
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            // Фон совпадает с фоном картинки знака
            .background(Color(hex: khaki ? 0x24281C : 0x0C162C).ignoresSafeArea())
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
