#if os(macOS)
import AppKit
import Engine
import QuartzCore

/// Top-level application entry. Owns AppKit setup, the window, and the
/// per-frame display-link tick that drives the engine. Call `run()` from
/// `@main` to boot.
@MainActor
public final class Host: NSObject {
    private let engine: GameEngine
    private var window: NSWindow?
    private var displayLink: CADisplayLink?
    private var lastTimestamp: CFTimeInterval = 0

    public init(game: any Game) {
        self.engine = GameEngine(game: game)
        super.init()
    }

    /// Boots AppKit, opens a window, starts the per-frame tick, and runs the
    /// event loop. Blocks until the user quits.
    public func run() {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)

        // Placeholder window. Swap the contentView for a CAMetalLayer-backed
        // NSView once the renderer exists.
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1280, height: 720),
            styleMask: [.titled, .closable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Game"
        window.center()
        window.makeKeyAndOrderFront(nil)
        self.window = window

        // CADisplayLink fires on the runloop it's added to, so attaching to
        // .main keeps the tick on the main thread (where @MainActor lives).
        let link = window.contentView!.displayLink(target: self, selector: #selector(tick(_:)))
        link.add(to: .main, forMode: .common)
        self.displayLink = link

        app.activate()
        app.run()
    }

    @objc private func tick(_ link: CADisplayLink) {
        let now = link.timestamp
        let dt = lastTimestamp == 0 ? Float(0) : Float(now - lastTimestamp)
        lastTimestamp = now

        engine.update(dt: dt)
    }
}
#else
// iOS/iPadOS host: AppKit doesn't apply. Typical pattern is a SwiftUI `App`
// (or UIApplicationDelegate) that owns a UIView containing a CAMetalLayer,
// with a CADisplayLink wired the same way as above. Defer until a renderer
// and a real iOS entry point are needed.
#endif
