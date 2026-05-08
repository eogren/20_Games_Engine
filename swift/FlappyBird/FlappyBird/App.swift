import Platform

#if os(macOS)

@main
struct FlappyBirdApp {
    @MainActor
    static func main() {
        Host(game: MyGame(), title: "Flappy Bird", fpsCap: 60).run()
    }
}

#elseif os(iOS)

import SwiftUI
import UIKit

@main
struct FlappyBirdApp: App {
    var body: some Scene {
        WindowGroup {
            HostView()
                // Edge-to-edge — UIKit's safe area would otherwise letterbox
                // the metal layer, leaving black bars on iPhone notched
                // devices.
                .ignoresSafeArea()
                // Black behind the status bar in case any safe-area gap
                // sneaks in during rotation.
                .background(.black)
        }
    }
}

/// Bridges `Host.makeViewController()` into SwiftUI. The `Host` is
/// constructed once inside `makeUIViewController` and retained by the
/// returned controller.
struct HostView: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        Host(game: MyGame(), fpsCap: 60).makeViewController()
    }

    func updateUIViewController(_: UIViewController, context: Context) {}
}

#endif
