import Engine

final class MyGame: Game {
    private var frame = 0

    func update(_ ctx: GameContext, dt: Float) {
        frame += 1
        if frame.isMultiple(of: 60) {
            print("Hello, world!")
        }
    }
}
