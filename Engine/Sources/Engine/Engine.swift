@MainActor
public final class GameEngine {
    public let keyboard: Keyboard
    private let game: any Game

    public init(game: any Game) {
        self.keyboard = Keyboard()
        self.game = game
    }

    /// One simulation step. Platform host calls this once per display refresh.
    /// `dt` is seconds since the previous tick. Frame ordering is fixed:
    /// game logic runs first (consumes input edges), then `endFrame()` clears
    /// edges so the next tick sees a clean slate.
    public func update(dt: Float) {
        let ctx = GameContext(keyboard: keyboard)
        game.update(ctx, dt: dt)
        keyboard.endFrame()
    }
}
