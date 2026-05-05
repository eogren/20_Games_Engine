import Platform

@main
struct FlappyBirdApp {
    @MainActor
    static func main() {
        Host(game: MyGame(), title: "Flappy Bird", fpsCap: 60).run()
    }
}
