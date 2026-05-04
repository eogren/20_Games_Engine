/// The seam where game code plugs into the engine. A game target implements
/// `Game`, hands an instance to the platform host, and the engine calls
/// `update(_:dt:)` once per display refresh.
@MainActor
public protocol Game: AnyObject {
    /// Must stay synchronous. `GameEngine.update` relies on this method
    /// running atomically on MainActor between input-event handling and
    /// `keyboard.endFrame()` — an `await` here would let a queued input
    /// Task land mid-tick, and its pressed-edge would be cleared before
    /// game code ever observed it.
    func update(_ ctx: GameContext, dt: Float)
}

/// Per-frame handle the engine passes to game code. Acts as an explicit
/// allowlist of what the game may touch this tick — easier to mock than
/// handing over the whole engine.
@MainActor
public struct GameContext {
    public let keyboard: Keyboard
    public let renderer: Renderer
}
