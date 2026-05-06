import Metal
@preconcurrency import ModelIO
import QuartzCore

@MainActor
public final class GameEngine {
    public let keyboard: Keyboard
    public let renderer: Renderer
    public let meshLoader: MeshLoader
    private let game: any Game

    /// `gameLibrary` is optional during early development — a game with
    /// no `.metal` files yet has no default library. Draw calls that need
    /// a game-side fragment shader will trap if this is nil.
    public init(device: MTLDevice, gameLibrary: MTLLibrary?, game: any Game) {
        self.keyboard = Keyboard()
        self.renderer = Renderer(device: device, gameLibrary: gameLibrary)
        self.meshLoader = MeshLoader(device: device, vertexDescriptor: Self.meshVertexDescriptor())
        self.game = game
    }

    /// Run the game's one-time async setup. Platform host calls this once
    /// after construction and before starting the per-frame display link,
    /// so games are guaranteed to see fully-populated assets in their
    /// first `update` tick.
    public func load() async throws {
        let ctx = GameContext(keyboard: keyboard, renderer: renderer, meshLoader: meshLoader)
        try await game.load(ctx)
    }

    /// One simulation step. Platform host calls this once per display
    /// refresh with the frame's drawable + render-pass descriptor. The
    /// engine brackets `game.update` with the renderer's begin/endFrame
    /// so the game can issue draws against `ctx.renderer` immediately.
    /// `keyboard.endFrame()` runs last so the next tick sees clean edges.
    public func update(dt: Float, drawable: CAMetalDrawable, passDescriptor: MTL4RenderPassDescriptor) {
        renderer.beginFrame(passDescriptor: passDescriptor, drawable: drawable)
        let ctx = GameContext(keyboard: keyboard, renderer: renderer, meshLoader: meshLoader)
        game.update(ctx, dt: dt)
        renderer.endFrame()
        keyboard.endFrame()
    }

    /// Phase-1 mesh layout: position (float3) + UV (float2), interleaved
    /// in buffer 0. The renderer's mesh PSO will key off this same shape;
    /// kept as the engine's single owner of the convention until that
    /// PSO surfaces and pulls the descriptor next to itself.
    private static func meshVertexDescriptor() -> MDLVertexDescriptor {
        let d = MDLVertexDescriptor()
        d.attributes[0] = MDLVertexAttribute(
            name: MDLVertexAttributePosition,
            format: .float3,
            offset: 0,
            bufferIndex: 0)
        d.attributes[1] = MDLVertexAttribute(
            name: MDLVertexAttributeTextureCoordinate,
            format: .float2,
            offset: 12,
            bufferIndex: 0)
        d.layouts[0] = MDLVertexBufferLayout(stride: 20)
        return d
    }
}
