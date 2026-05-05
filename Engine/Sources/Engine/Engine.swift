import Metal
import QuartzCore

@MainActor
public final class GameEngine {
    public let keyboard: Keyboard
    public let renderer: Renderer
    private let game: any Game

    /// `gameLibrary` is optional during early development — a game with
    /// no `.metal` files yet has no default library. Draw calls that need
    /// a game-side fragment shader will trap if this is nil.
    public init(device: MTLDevice, gameLibrary: MTLLibrary?, game: any Game) {
        self.keyboard = Keyboard()
        self.renderer = Renderer(device: device, gameLibrary: gameLibrary)
        self.game = game
    }

    /// One simulation step. Platform host calls this once per display
    /// refresh with the frame's drawable + render-pass descriptor. The
    /// engine brackets `game.update` with the renderer's begin/endFrame
    /// so the game can issue draws against `ctx.renderer` immediately.
    /// `keyboard.endFrame()` runs last so the next tick sees clean edges.
    public func update(dt: Float, drawable: CAMetalDrawable, passDescriptor: MTL4RenderPassDescriptor) {
        renderer.beginFrame(passDescriptor: passDescriptor, drawable: drawable)
        let ctx = GameContext(keyboard: keyboard, renderer: renderer)
        game.update(ctx, dt: dt)
        renderer.endFrame()
        keyboard.endFrame()
    }
}
