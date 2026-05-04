import Engine

/// Layout must match `BackgroundUniforms` in `Shaders.metal`. Keep the
/// fields in the same order and use scalar `Float`s — Metal's `float`
/// matches Swift's `Float` (32-bit).
struct BackgroundUniforms {
    var time: Float
}

final class MyGame: Game {
    private var elapsed: Float = 0

    func update(_ ctx: GameContext, dt: Float) {
        elapsed += dt
        ctx.renderer.drawFullscreenQuad(
            fragmentShader: "background",
            uniforms: BackgroundUniforms(time: elapsed)
        )
    }
}
