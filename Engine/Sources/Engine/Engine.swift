import Metal
import QuartzCore

@MainActor
public final class GameEngine {
    public let keyboard: Keyboard
    public let pointer: Pointer
    public let renderer: Renderer
    public let meshLoader: MeshLoader
    private let game: any Game
    /// Game-time seconds — the running sum of `dt` across `update` calls.
    /// Pushed into the renderer at frame start so mesh shaders can drive
    /// animation off a single engine-owned clock instead of each game
    /// accumulating its own. Game-time, not wall-clock: a slow-motion or
    /// paused tick (smaller / zero `dt`) advances this proportionally,
    /// so shader animation tracks the simulation.
    private var elapsed: Float = 0

    /// `gameLibrary` is optional during early development — a game with
    /// no `.metal` files yet has no default library. Draw calls that need
    /// a game-side fragment shader will trap if this is nil.
    public init(device: MTLDevice, gameLibrary: MTLLibrary?, game: any Game) {
        self.keyboard = Keyboard()
        self.pointer = Pointer()
        self.renderer = Renderer(device: device, gameLibrary: gameLibrary)
        self.meshLoader = MeshLoader(device: device, vertexDescriptor: Renderer.meshVertexDescriptor())
        self.game = game
    }

    /// Run the game's one-time async setup. Platform host calls this once
    /// after construction and before starting the per-frame display link,
    /// so games are guaranteed to see fully-populated assets in their
    /// first `update` tick.
    public func load() async throws {
        let ctx = GameContext(keyboard: keyboard, pointer: pointer, renderer: renderer, meshLoader: meshLoader)
        try await game.load(ctx)
    }

    /// One simulation step. Platform host calls this once per display
    /// refresh with the frame's drawable + render-pass descriptor. The
    /// engine brackets `game.update` with the renderer's begin/endFrame
    /// so the game can issue draws against `ctx.renderer` immediately.
    /// Input edge clears run last so the next tick sees clean edges.
    public func update(dt: Float, drawable: CAMetalDrawable, passDescriptor: MTL4RenderPassDescriptor) {
        elapsed += dt
        renderer.beginFrame(passDescriptor: passDescriptor, drawable: drawable, time: elapsed)
        let ctx = GameContext(keyboard: keyboard, pointer: pointer, renderer: renderer, meshLoader: meshLoader)
        game.update(ctx, dt: dt)
        renderer.endFrame()
        keyboard.endFrame()
        pointer.endFrame()
    }
}
