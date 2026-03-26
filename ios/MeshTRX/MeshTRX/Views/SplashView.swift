import SwiftUI

struct SplashView: View {
    @State private var isActive = false
    @State private var logoOpacity: Double = 0
    @State private var versionOpacity: Double = 0

    var body: some View {
        if isActive {
            MainTabView()
        } else {
            ZStack {
                Color.black.ignoresSafeArea()

                VStack(spacing: 20) {
                    Image(systemName: "antenna.radiowaves.left.and.right")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 100, height: 100)
                        .foregroundColor(.green)
                        .opacity(logoOpacity)

                    Text("MeshTRX")
                        .font(.system(size: 36, weight: .bold))
                        .foregroundColor(.white)
                        .opacity(logoOpacity)

                    Text("v1.0.0")
                        .font(.subheadline)
                        .foregroundColor(.gray)
                        .opacity(versionOpacity)
                }
            }
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
