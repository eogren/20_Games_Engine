import Engine
import MetalKit
import simd

/// Layout must match `BackgroundUniforms` in `Shaders.metal`. Keep the
/// fields in the same order and use scalar `Float`s — Metal's `float`
/// matches Swift's `Float` (32-bit).
struct BackgroundUniforms {
    var time: Float
}

private enum RotationAxis { case x, y, z }

final class MyGame: Game {
    private static let rotationSpeed: Float = 1.0  // rad/sec

    private var elapsed: Float = 0
    private var cube: MTKMesh?
    private var rotationAxis: RotationAxis = .y
    // Accumulated cube orientation. Composed in world-space each frame
    // (newDelta * orientation) so the active axis stays world-aligned —
    // a tap visibly switches which world axis the cube spins about,
    // rather than smearing the spin into the cube's tilted local frame.
    private var orientation: simd_quatf = .identity

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

        if ctx.keyboard.wasPressed(.space) || ctx.pointer.tappedThisFrame {
            rotationAxis = switch rotationAxis {
            case .y: .x
            case .x: .z
            case .z: .y
            }
        }

        let delta: simd_quatf = switch rotationAxis {
        case .x: .aroundX(Self.rotationSpeed * dt)
        case .y: .aroundY(Self.rotationSpeed * dt)
        case .z: .aroundZ(Self.rotationSpeed * dt)
        }
        orientation = delta * orientation

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
        cubeTransform.rotation = orientation
        ctx.renderer.drawMesh(cube, fragmentShader: "cube_uv", meshTransform: cubeTransform)
    }
}
