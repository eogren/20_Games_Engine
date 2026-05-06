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
        let mesh = try await ctx.meshLoader.loadMesh(from: url)
        ctx.renderer.register(mesh)
        cube = mesh
    }

    func update(_ ctx: GameContext, dt: Float) {
        elapsed += dt
        ctx.renderer.drawFullscreenQuad(
            fragmentShader: "background",
            uniforms: BackgroundUniforms(time: elapsed)
        )
        guard let cube else { return }

        // Camera at (3, 2, 4) looking at origin shows three faces of the
        // cube at a comfortable 3D angle.
        var camera = Transform.identity
        camera.translation = [3, 2, 4]
        camera.lookAt([0, 0, 0])
        let size = ctx.renderer.drawableSize
        let proj = float4x4.perspective(fovY: .pi / 3, aspect: size.x / size.y, near: 0.1, far: 100)
        let vp = float4x4.viewPerspectiveMatrix(cameraTransform: camera, cameraPerspective: proj)
        ctx.renderer.setCamera(viewProjection: vp)

        var cubeTransform = Transform.identity
        cubeTransform.rotation = .aroundY(elapsed)
        ctx.renderer.drawMesh(cube, fragmentShader: "cube_uv", meshTransform: cubeTransform)
    }
}
