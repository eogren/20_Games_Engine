import Metal
import MetalKit
import simd
import Testing
@testable import Engine

/// Pixel-level smoke tests for the renderer. Render to an offscreen
/// `MTLTexture` (no window, no drawable), commit, wait, read back bytes,
/// assert. Lives here rather than in a screen-capture / window-driven
/// test because the offscreen path is deterministic, headless, and runs
/// on CI without a display server. Texture / pass / readback boilerplate
/// is in `PixelTestSupport.swift` so each test reads as the rendering
/// behavior it's pinning down.
///
/// Requires the engine `.metallib` to be available in `Bundle.module`,
/// which means these tests must run via `xcodebuild test` (the SwiftPM
/// CLI doesn't invoke the `metal` compiler).
@Suite struct RendererSmokeTests {
    /// True only when the engine `.metallib` is in `Bundle.module`. False
    /// under `swift test` (CLI doesn't invoke the Metal compiler) and
    /// true under `xcodebuild test` (which does). Tests that construct a
    /// `Renderer` are gated on this so `swift test` stays green.
    static let engineMetalLibraryAvailable: Bool =
        Bundle.module.url(forResource: "default", withExtension: "metallib") != nil

    /// True iff the system device supports Metal 4. Real Apple Silicon
    /// hardware passes; virtualized GPUs (CI VMs) don't. See `Renderer.init`
    /// for why we preflight instead of trusting the documented nil.
    static let metal4Supported: Bool = {
        guard let device = MTLCreateSystemDefaultDevice() else { return false }
        return device.supportsFamily(.metal4)
    }()

    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs engine metallib in Bundle.module (xcodebuild test) and a Metal-4-capable GPU (real hardware, not a CI VM)"))
    @MainActor func clearToBlackFillsTargetWithBlackPixels() throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon CI runner expected")
        let renderer = Renderer(device: device, gameLibrary: nil)

        let target = try OffscreenColorTarget(device: device, width: 64, height: 64)

        renderer.beginFrame(passDescriptor: target.clearPass(MTLClearColorMake(0, 0, 0, 1)))
        renderer.endFrame().waitUntilCompleted()

        let pixels = target.readback()
        for y in 0..<target.height {
            for x in 0..<target.width {
                #expect(pixels[x, y] == .opaqueBlack, "pixel at (\(x),\(y))")
            }
        }
    }

    /// Catches viewport / rasterizer-shaped bugs that the clear-only test
    /// can't see. A missing `setViewport` in `beginFrame` doesn't affect
    /// `loadAction = .clear` (clears bypass the rasterizer), but it does
    /// stop draws from landing on the target — exactly the regression
    /// that motivated this test. We render a fullscreen quad whose
    /// fragment emits the interpolated uv into red/green and assert the
    /// four corners disagree from each other in the expected directions:
    /// red increases left→right, green increases bottom→top. Any failure
    /// mode (no rasterization, wrong viewport, transposed axes, uniform
    /// color) breaks at least one of those inequalities.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs test metallib + engine metallib in their bundles (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func fullscreenDrawCoversTargetWithUVGradient() throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon hardware expected")

        // The test fragment lives in the test bundle's metallib. Loaded
        // and handed to the Renderer as the "game" library.
        let testLibrary = try #require(
            try? device.makeDefaultLibrary(bundle: .module),
            "couldn't load test metallib from Bundle.module — run via xcodebuild test"
        )
        let renderer = Renderer(device: device, gameLibrary: testLibrary)

        let target = try OffscreenColorTarget(device: device, width: 64, height: 64)
        // Magenta clear, distinguishable from anything the gradient emits.
        // If a corner reads back as clear-color the draw didn't cover it.
        let clearPass = target.clearPass(MTLClearColorMake(1, 0, 1, 1))

        struct UVGradientUniforms { var dummy: Float = 0 }

        renderer.beginFrame(passDescriptor: clearPass)
        renderer.drawFullscreenQuad(fragmentShader: "uv_gradient",
                                    uniforms: UVGradientUniforms())
        renderer.endFrame().waitUntilCompleted()

        let pixels = target.readback()
        // Sample one pixel in from each corner to dodge edge-rasterization
        // anomalies. Framebuffer row 0 is the top of the screen; the
        // fullscreen vertex shader emits uv=clip*0.5+0.5 with clip.y=+1
        // at top, so top-of-framebuffer corresponds to uv.y≈1 (green high).
        let topLeft     = pixels[1,                  1]
        let topRight    = pixels[target.width - 2,   1]
        let bottomLeft  = pixels[1,                  target.height - 2]
        let bottomRight = pixels[target.width - 2,   target.height - 2]

        // Sanity: the draw covered every corner (no clear-color leakage,
        // blue stays 0 from the shader, alpha stays 255).
        for (name, p) in [("top-left", topLeft), ("top-right", topRight),
                          ("bottom-left", bottomLeft), ("bottom-right", bottomRight)] {
            #expect(p.b == 0, "blue at \(name) (clear color leaked through?)")
            #expect(p.a == 255, "alpha at \(name)")
        }

        // Red increases left→right.
        #expect(topLeft.r < 32,      "top-left red should be near 0, got \(topLeft.r)")
        #expect(topRight.r > 223,    "top-right red should be near 255, got \(topRight.r)")
        #expect(bottomLeft.r < 32,   "bottom-left red should be near 0, got \(bottomLeft.r)")
        #expect(bottomRight.r > 223, "bottom-right red should be near 255, got \(bottomRight.r)")

        // Green increases bottom→top.
        #expect(topLeft.g > 223,    "top-left green should be near 255, got \(topLeft.g)")
        #expect(topRight.g > 223,   "top-right green should be near 255, got \(topRight.g)")
        #expect(bottomLeft.g < 32,  "bottom-left green should be near 0, got \(bottomLeft.g)")
        #expect(bottomRight.g < 32, "bottom-right green should be near 0, got \(bottomRight.g)")

        // The cross-pair inequality TASKS.md called out — top-right and
        // bottom-left should be maximally different (one is yellow, the
        // other is black). A bug that emits a uniform color anywhere
        // (e.g., shader ran but uv interpolation broke) trips this.
        #expect(topRight.r > bottomLeft.r + 192,
                "top-right.r (\(topRight.r)) should dominate bottom-left.r (\(bottomLeft.r))")
        #expect(topRight.g > bottomLeft.g + 192,
                "top-right.g (\(topRight.g)) should dominate bottom-left.g (\(bottomLeft.g))")
    }

    /// Pins down `Renderer.drawableSize` against a non-square render
    /// target so a future regression that swaps width/height (or
    /// drops one dimension to 0) trips immediately. Games rely on this
    /// to build aspect-correct projection matrices — getting it wrong
    /// silently distorts every 3D draw.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs engine metallib (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func drawableSizeReportsRenderTargetDimensions() throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon hardware expected")
        let renderer = Renderer(device: device, gameLibrary: nil)
        let target = try OffscreenColorTarget(device: device, width: 80, height: 40)

        renderer.beginFrame(passDescriptor: target.clearPass(MTLClearColorMake(0, 0, 0, 1)))
        let size = renderer.drawableSize
        renderer.endFrame().waitUntilCompleted()

        #expect(size == SIMD2<Float>(80, 40))
    }

    /// End-to-end pixel test for the mesh path. Loads the unit quad
    /// fixture, registers it for residency, sets a camera positioned so
    /// the quad covers the framebuffer center but not its corners, and
    /// asserts the readback matches that footprint. This is the first
    /// test that actually exercises `register` + `setCamera` + indexed
    /// `drawMesh` together — silent failures (GPU fault on non-resident
    /// allocation, indexed-draw issuing zero triangles, view-projection
    /// inverted) all surface as a wrong center pixel here.
    ///
    /// Geometry math: camera at (0, 0, 3) looking at the origin, fov 90°,
    /// aspect 1. Visible half-height at z=0 is 3*tan(45°)=3, so the
    /// ±1 quad covers ±1/3 of NDC — i.e., the center third of the
    /// framebuffer. Pixel (1, 1) is well outside that, pixel (32, 32)
    /// is well inside.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs test metallib + engine metallib in their bundles (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func drawMeshRendersQuadOverFramebufferCenter() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon hardware expected")
        let testLibrary = try #require(
            try? device.makeDefaultLibrary(bundle: .module),
            "couldn't load test metallib from Bundle.module — run via xcodebuild test"
        )
        let renderer = Renderer(device: device, gameLibrary: testLibrary)

        let quadURL = try #require(
            Bundle.module.url(forResource: "quad", withExtension: "obj", subdirectory: "Meshes"),
            "quad.obj missing from test bundle"
        )
        let loader = MeshLoader(device: device, vertexDescriptor: Renderer.meshVertexDescriptor())
        let mesh = try await loader.loadMesh(from: quadURL)
        renderer.register(mesh)

        let target = try OffscreenColorTarget(device: device, width: 64, height: 64)
        // Cyan clear so any leftover clear-color leakage in the center
        // is loud (cyan is maximally different from the shader's red).
        let clearPass = target.clearPass(MTLClearColorMake(0, 1, 1, 1))

        var camera = Transform.identity
        camera.translation = [0, 0, 3]
        camera.lookAt([0, 0, 0])
        let proj = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: 0.1, far: 100)
        let vp = float4x4.viewPerspectiveMatrix(cameraTransform: camera, cameraPerspective: proj)

        renderer.beginFrame(passDescriptor: clearPass)
        renderer.setCamera(viewProjection: vp)
        renderer.drawMesh(mesh, fragmentShader: "mesh_solid_red", meshTransform: .identity)
        renderer.endFrame().waitUntilCompleted()

        let pixels = target.readback()

        // Center: quad covers it → red.
        let center = pixels[target.width / 2, target.height / 2]
        #expect(center.r > 223, "center red should be near 255, got \(center.r)")
        #expect(center.g < 32,  "center green should be near 0 (no clear leakage), got \(center.g)")
        #expect(center.b < 32,  "center blue should be near 0 (no clear leakage), got \(center.b)")

        // Corner: quad doesn't cover it → cyan clear color survives.
        let corner = pixels[1, 1]
        #expect(corner.r < 32,  "corner red should be near 0 (cyan clear), got \(corner.r)")
        #expect(corner.g > 223, "corner green should be near 255, got \(corner.g)")
        #expect(corner.b > 223, "corner blue should be near 255, got \(corner.b)")
    }
}
