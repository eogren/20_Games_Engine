/// The seam where game code plugs into the engine. A game target implements
/// `Game`, hands an instance to the platform host, and the engine calls
/// `update(_:dt:)` once per display refresh.
@MainActor
public protocol Game: AnyObject {
    /// One-time async setup: load meshes, parse JSON, etc. Runs once after
    /// the engine is constructed but before the first `update` tick — the
    /// platform host blocks the display link from starting until this
    /// returns, so the game is guaranteed to see populated assets in
    /// frame 0. Default impl does nothing for games with no async setup.
    func load(_ ctx: GameContext) async throws

    /// Must stay synchronous. `GameEngine.update` relies on this method
    /// running atomically on MainActor between input-event handling and
    /// `keyboard.endFrame()` — an `await` here would let a queued input
    /// Task land mid-tick, and its pressed-edge would be cleared before
    /// game code ever observed it.
    func update(_ ctx: GameContext, dt: Float)
}

extension Game {
    public func load(_ ctx: GameContext) async throws {}
}

/// Per-frame handle the engine passes to game code. Acts as an explicit
/// allowlist of what the game may touch this tick — easier to mock than
/// handing over the whole engine.
@MainActor
public struct GameContext {
    public let keyboard: Keyboard
    public let pointer: Pointer
    public let renderer: Renderer
    public let meshLoader: MeshLoader
}
