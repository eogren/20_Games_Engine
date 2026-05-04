#if os(macOS)
import AppKit
import Engine
import Metal
import QuartzCore

/// Top-level application entry. Owns AppKit setup, the window, and the
/// per-frame display-link tick that drives the engine. Call `run()` from
/// `@main` to boot.
@MainActor
public final class Host: NSObject {
    private let device: MTLDevice
    private let engine: GameEngine
    private var window: NSWindow?
    private var metalView: MetalView?
    private var displayLink: CADisplayLink?
    private var closeObserver: NSObjectProtocol?
    private var lastTimestamp: CFTimeInterval = 0

    public init(game: any Game) {
        guard let device = MTLCreateSystemDefaultDevice() else {
            fatalError("Host: no Metal device available on this system")
        }
        self.device = device
        // Bundle.main is the running app; the game ships its `.metal`
        // files there. A nil library is fine for early-dev games with no
        // shaders yet — Engine tolerates it.
        let gameLibrary = try? device.makeDefaultLibrary(bundle: .main)
        self.engine = GameEngine(device: device, gameLibrary: gameLibrary, game: game)
        super.init()
    }

    /// Boots AppKit, opens a window, starts the per-frame tick, and runs the
    /// event loop. Blocks until the user quits.
    public func run() {
        let app = NSApplication.shared
        app.setActivationPolicy(.regular)

        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 1280, height: 720),
            styleMask: [.titled, .closable, .resizable],
            backing: .buffered,
            defer: false
        )
        window.title = "Game"

        let view = MetalView(device: device)
        window.contentView = view
        self.metalView = view

        window.center()
        window.makeKeyAndOrderFront(nil)
        self.window = window

        // macOS keeps the app alive after the window closes so Dock-icon
        // re-opens it; wrong for a single-window game. Scoped to *this*
        // window (object: window) and routed through `terminate(_:)` so any
        // future delegate's `applicationShouldTerminate` still has a say.
        // Avoids claiming the app's single delegate slot.
        self.closeObserver = NotificationCenter.default.addObserver(
            forName: NSWindow.willCloseNotification,
            object: window,
            queue: .main
        ) { _ in
            MainActor.assumeIsolated {
                NSApplication.shared.terminate(nil)
            }
        }

        // AppKit only honors menu key equivalents when a main menu is
        // installed; without this, Cmd-Q is a no-op.
        app.mainMenu = Self.makeMainMenu()

        // CADisplayLink fires on the runloop it's added to, so attaching to
        // .main keeps the tick on the main thread (where @MainActor lives).
        let link = view.displayLink(target: self, selector: #selector(tick(_:)))
        link.add(to: .main, forMode: .common)
        self.displayLink = link

        app.activate()
        app.run()
    }

    @objc private func tick(_ link: CADisplayLink) {
        let now = link.timestamp
        let dt = lastTimestamp == 0 ? Float(0) : Float(now - lastTimestamp)
        lastTimestamp = now

        // nextDrawable can return nil under window-server pressure or when
        // the layer has zero size. Skip the frame — the display link will
        // fire again shortly.
        guard let view = metalView,
              let drawable = view.metalLayer.nextDrawable() else {
            return
        }

        let pass = MTLRenderPassDescriptor()
        let color = pass.colorAttachments[0]!
        color.texture = drawable.texture
        color.loadAction = .clear
        color.storeAction = .store
        color.clearColor = MTLClearColorMake(0, 0, 0, 1)

        engine.update(dt: dt, drawable: drawable, passDescriptor: pass)
    }

    /// Builds the macOS main menu. Currently exposes only a Quit item bound
    /// to Cmd-Q; AppKit dispatches the shortcut to `NSApplication.terminate`.
    /// Static and pure so tests can assert on the menu without booting AppKit.
    static func makeMainMenu() -> NSMenu {
        let mainMenu = NSMenu()
        let appMenuItem = NSMenuItem()
        mainMenu.addItem(appMenuItem)

        let appMenu = NSMenu()
        let quitItem = NSMenuItem(
            title: "Quit \(ProcessInfo.processInfo.processName)",
            action: #selector(NSApplication.terminate(_:)),
            keyEquivalent: "q"
        )
        quitItem.keyEquivalentModifierMask = .command
        appMenu.addItem(quitItem)
        appMenuItem.submenu = appMenu

        return mainMenu
    }
}
#else
// iOS/iPadOS host: AppKit doesn't apply. Typical pattern is a SwiftUI `App`
// (or UIApplicationDelegate) that owns a UIView containing a CAMetalLayer,
// with a CADisplayLink wired the same way as above. Defer until a renderer
// and a real iOS entry point are needed.
#endif
