import Platform

@main
struct FlappyBirdApp {
    @MainActor
    static func main() {
        Host(game: MyGame()).run()
    }
}
