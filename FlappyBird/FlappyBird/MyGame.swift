import Engine
import MetalKit

/// Layout must match `BackgroundUniforms` in `Shaders.metal`. Keep the
/// fields in the same order and use scalar `Float`s — Metal's `float`
/// matches Swift's `Float` (32-bit).
struct BackgroundUniforms {
    var time: Float
}

final class MyGame: Game {
    private var elapsed: Float = 0
    private var cube: MTKMesh?

    func load(_ ctx: GameContext) async throws {
        guard let url = Bundle.main.url(forResource: "cube", withExtension: "obj") else {
            fatalError("MyGame.load: cube.obj missing from app bundle")
        }
        cube = try await ctx.meshLoader.loadMesh(from: url)
    }

    func update(_ ctx: GameContext, dt: Float) {
        elapsed += dt
        ctx.renderer.drawFullscreenQuad(
            fragmentShader: "background",
            uniforms: BackgroundUniforms(time: elapsed)
        )
        // Cube draw lands when Renderer.drawMesh exists; load is wired
        // first so the asset path is exercised end-to-end before the
        // PSO/draw work surfaces.
    }
}
