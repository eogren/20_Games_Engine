import Engine
import Metal
import QuartzCore

#if os(macOS)
import AppKit
#elseif os(iOS)
import UIKit
#endif

/// Top-level host for a game. Constructs the engine, owns the per-frame
/// display-link tick, and bridges into whichever UI framework is current.
/// macOS opens an `NSWindow` and runs the AppKit event loop via `run()`;
/// iOS hands a `UIViewController` back to a SwiftUI `App` (or UIKit
/// equivalent) via `makeViewController()`.
///
/// The frame-tick body lives in a single shared `runFrame(view:link:)` so
/// that as the per-frame contract evolves (e.g. picking up audio, sim
/// scheduling, multi-pass setup) only one place changes; the macOS and
/// iOS @objc shims exist only to source the active `MetalView`.
@MainActor
public final class Host: NSObject {
    private let device: MTLDevice
    private let engine: GameEngine
    private let title: String
    private let fpsCap: Int?
    private var lastTimestamp: CFTimeInterval = 0

    #if os(macOS)
    private var window: NSWindow?
    private var metalView: MetalView?
    private var displayLink: CADisplayLink?
    private var closeObserver: NSObjectProtocol?
    #endif

    public init(game: any Game, title: String = "Game", fpsCap: Int? = nil) {
        guard let device = MTLCreateSystemDefaultDevice() else {
            fatalError("Host: no Metal device available on this system")
        }
        self.device = device
        // `title` is the macOS window title. Accepted on iOS too so the
        // constructor signature is portable, but ignored — UIKit has no
        // window-title surface to apply it to.
        self.title = title
        self.fpsCap = fpsCap
        // Bundle.main is the running app; the game ships its `.metal`
        // files there. A nil library is fine for early-dev games with no
        // shaders yet — Engine tolerates it.
        let gameLibrary = try? device.makeDefaultLibrary(bundle: .main)
        self.engine = GameEngine(device: device, gameLibrary: gameLibrary, game: game)
        super.init()
    }

    /// Single per-frame entry point shared across platforms. Both the
    /// macOS Host's @objc tick and the iOS HostViewController's @objc
    /// tick funnel into this method — keep frame-loop logic here so it
    /// stays in lockstep across both targets.
    fileprivate func runFrame(view: MetalView, link: CADisplayLink) {
        let now = link.timestamp
        let dt = lastTimestamp == 0 ? Float(0) : Float(now - lastTimestamp)
        lastTimestamp = now

        // nextDrawable can return nil under window-server pressure or when
        // the layer has zero size. The MSAA color and depth attachments
        // are nil until the first layout pass sizes them. Skip the frame
        // in any of these cases — the display link will fire again shortly.
        guard let drawable = view.metalLayer.nextDrawable(),
              let depthTexture = view.depthTexture,
              let msaaColor = view.msaaColorTexture else {
            return
        }

        // 4× MSAA: rasterize into the multisample color, resolve into the
        // drawable on-tile. `.multisampleResolve` (not `.storeAnd…`) —
        // we don't want MSAA samples kept around, which lets the MSAA
        // texture stay memoryless on Apple Silicon.
        let pass = MTL4RenderPassDescriptor()
        let color = pass.colorAttachments[0]!
        color.texture = msaaColor
        color.resolveTexture = drawable.texture
        color.loadAction = .clear
        color.storeAction = .multisampleResolve
        color.clearColor = MTLClearColorMake(0, 0, 0, 1)

        let depth = pass.depthAttachment!
        depth.texture = depthTexture
        depth.loadAction = .clear
        depth.storeAction = .dontCare
        depth.clearDepth = 1.0

        engine.update(dt: dt, drawable: drawable, passDescriptor: pass)
    }

    fileprivate var fpsCapForFrameRateRange: Float? {
        fpsCap.map(Float.init)
    }

    fileprivate var enginePointer: Pointer { engine.pointer }
    fileprivate var metalDevice: MTLDevice { device }
    fileprivate func runEngineLoad() async throws { try await engine.load() }

    // MARK: - macOS entry

    #if os(macOS)
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
        window.title = title

        let view = MetalView(device: device, pointer: engine.pointer)
        window.contentView = view
        self.metalView = view

        window.center()
        window.makeKeyAndOrderFront(nil)
        // Without an explicit first responder, NSWindow itself sits at the
        // top of the chain — keyDown bubbles up to it and beeps. Hand the
        // role to MetalView so its keyDown override swallows the event.
        window.makeFirstResponder(view)
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

        // Defer the display link until the game's async load completes —
        // ticking against an unloaded game would produce frames where mesh
        // assets aren't bound yet. The Task suspends inside `engine.load()`
        // (MeshLoader hops to a detached background task for file IO), so
        // the main runloop started by `app.run()` below is free to dispatch
        // its continuation back here when load finishes.
        Task { @MainActor [weak self] in
            guard let self else { return }
            do {
                try await self.runEngineLoad()
            } catch {
                fatalError("Host: engine.load() failed: \(error)")
            }
            self.startDisplayLink()
        }

        app.activate()
        app.run()
    }

    /// Attaches a CADisplayLink to the main runloop so `tick(_:)` fires
    /// once per display refresh. Pulled out of `run()` so it can be
    /// deferred until after `engine.load()` completes.
    private func startDisplayLink() {
        guard let view = metalView else {
            fatalError("Host.startDisplayLink: metalView not set — run() should have created it")
        }
        // CADisplayLink fires on the runloop it's added to, so attaching to
        // .main keeps the tick on the main thread (where @MainActor lives).
        let link = view.displayLink(target: self, selector: #selector(tick(_:)))
        if let rate = fpsCapForFrameRateRange {
            link.preferredFrameRateRange = CAFrameRateRange(minimum: rate, maximum: rate, preferred: rate)
        }
        link.add(to: .main, forMode: .common)
        self.displayLink = link
    }

    @objc private func tick(_ link: CADisplayLink) {
        guard let view = metalView else { return }
        runFrame(view: view, link: link)
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
    #endif

    // MARK: - iOS entry

    #if os(iOS)
    /// Builds a UIViewController whose view is the engine's `MetalView`
    /// and which owns the per-frame display-link tick. Intended consumer
    /// is a `UIViewControllerRepresentable` inside a SwiftUI `App`. The
    /// controller retains `self`, so callers can drop their reference
    /// after handing the VC off.
    public func makeViewController() -> UIViewController {
        HostViewController(host: self)
    }
    #endif
}

#if os(iOS)
/// View controller that owns the `MetalView` and the display link. Kept
/// private because consumers should construct it via `Host.makeViewController()`.
@MainActor
private final class HostViewController: UIViewController {
    private let host: Host
    private var displayLink: CADisplayLink?
    private var didStartLoad = false

    init(host: Host) {
        self.host = host
        super.init(nibName: nil, bundle: nil)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("HostViewController is code-only") }

    override func loadView() {
        view = MetalView(device: host.metalDevice, pointer: host.enginePointer)
    }

    override func viewDidAppear(_ animated: Bool) {
        super.viewDidAppear(animated)
        // Defer the display link until the game's async load completes —
        // mirrors the macOS `Host.run()` flow. viewDidAppear may fire
        // multiple times across appearance transitions; gate with a flag
        // so load() runs exactly once. Subsequent appearances re-attach
        // a fresh display link if the previous one was torn down.
        if !didStartLoad {
            didStartLoad = true
            Task { @MainActor [weak self] in
                guard let self else { return }
                do {
                    try await self.host.runEngineLoad()
                } catch {
                    fatalError("Host: engine.load() failed: \(error)")
                }
                self.attachDisplayLink()
            }
        } else if displayLink == nil {
            attachDisplayLink()
        }
    }

    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        // Tear down rather than pause: v1 has no game state to preserve
        // across suspend/foreground transitions. Revisit when phase 2's
        // sim could break on backgrounding.
        displayLink?.invalidate()
        displayLink = nil
    }

    private func attachDisplayLink() {
        // CADisplayLink fires on the runloop it's added to, so attaching to
        // .main keeps the tick on the main thread (where @MainActor lives).
        let link = CADisplayLink(target: self, selector: #selector(tick(_:)))
        if let rate = host.fpsCapForFrameRateRange {
            link.preferredFrameRateRange = CAFrameRateRange(minimum: rate, maximum: rate, preferred: rate)
        }
        link.add(to: .main, forMode: .common)
        displayLink = link
    }

    @objc private func tick(_ link: CADisplayLink) {
        guard let metalView = view as? MetalView else { return }
        host.runFrame(view: metalView, link: link)
    }
}
#endif
