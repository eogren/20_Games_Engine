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
        let proj = float4x4.perspective(fovY: .degrees(90), aspect: 1, near: 0.1, far: 100)
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

    /// Pins down the world-space-position varying that `mesh_vertex`
    /// computes (`modelMatrix * objectPos`) and passes through
    /// `MeshVertexOut.worldPosition`. The test shader encodes the
    /// interpolated world position into RGB as `worldPos * 0.5 + 0.5`,
    /// so a known framebuffer pixel decodes back to a known world
    /// coordinate.
    ///
    /// Setup: unit quad translated to (0.5, 0, 0), camera at (0.5, 0, 3)
    /// looking at (0.5, 0, 0), fov 90°, aspect 1. The framebuffer-center
    /// ray hits the quad surface near world (0.5, 0, 0) — but pixel
    /// [32, 32] in a 64×64 target is offset half a pixel from true
    /// center: NDC ≈ (+1/64, -1/64) (row 0 is top of screen, so row 32
    /// is just *below* center). At z=0 with half-width=3, that maps to
    /// world ≈ (0.547, -0.047, 0). Expected encoding: r = 0.547*0.5+0.5
    /// ≈ 0.77 → ~197, g = -0.047*0.5+0.5 ≈ 0.48 → ~122, b = 0.5 → 128.
    ///
    /// Two ways this can go wrong that the test catches:
    ///
    /// 1. **Model matrix dropped.** If `mesh_vertex` emitted object-space
    ///    position instead of world-space, the same surface point would
    ///    decode to world ≈ (0.04, 0.02, 0), giving r ≈ 133 — well
    ///    outside the asserted [180, 220] window.
    ///
    /// 2. **Varying not interpolated.** A pixel offset 4 columns to the
    ///    right of center hits world x ≈ 0.92, encoding r ≈ 245. If the
    ///    varying came through but interpolation broke (uniform color),
    ///    the right pixel would equal the center pixel; the >+30 delta
    ///    on the second assertion catches that. The translation is held
    ///    at 0.5 instead of 1 so neither sample saturates the red
    ///    channel — saturation would make the delta untestable.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs test metallib + engine metallib in their bundles (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func meshVertexExposesWorldPositionToFragment() async throws {
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
        // Black clear so any leakage into the readback is loud (the
        // shader's encoding never produces all-zero RGB for this geometry).
        let clearPass = target.clearPass(MTLClearColorMake(0, 0, 0, 1))

        var camera = Transform.identity
        camera.translation = [0.5, 0, 3]
        camera.lookAt([0.5, 0, 0])
        let proj = float4x4.perspective(fovY: .degrees(90), aspect: 1, near: 0.1, far: 100)
        let vp = float4x4.viewPerspectiveMatrix(cameraTransform: camera, cameraPerspective: proj)

        var meshTransform = Transform.identity
        meshTransform.translation = [0.5, 0, 0]

        renderer.beginFrame(passDescriptor: clearPass)
        renderer.setCamera(viewProjection: vp)
        renderer.drawMesh(mesh, fragmentShader: "mesh_world_position", meshTransform: meshTransform)
        renderer.endFrame().waitUntilCompleted()

        let pixels = target.readback()

        // Center pixel: world ≈ (0.547, -0.047, 0), encoded ≈ (197, 122, 128).
        // The [180, 220] window on r excludes the no-modelMatrix case
        // (would be ~133) on one side and a uniform-saturation bug on
        // the other.
        let center = pixels[target.width / 2, target.height / 2]
        #expect(center.r > 180 && center.r < 220,
                "center red should be ~197 (worldPos.x = 0.547 → 0.547*0.5+0.5), got \(center.r)")
        #expect(abs(Int(center.g) - 122) < 4,
                "center green should be ~122 (worldPos.y ≈ -0.047), got \(center.g)")
        #expect(abs(Int(center.b) - 128) < 4,
                "center blue should be ~128 (worldPos.z = 0), got \(center.b)")

        // Right-of-center pixel: world x ≈ 0.92, encoded r ≈ 245.
        // Confirms the new varying is interpolated across the triangle
        // (perspective-correct via [[position]]'s w), not just emitted
        // as a per-vertex constant or uniform.
        let rightOfCenter = pixels[target.width / 2 + 4, target.height / 2]
        #expect(rightOfCenter.r > center.r + 30,
                "pixel +4px right of center should encode larger worldPos.x, got r=\(rightOfCenter.r) vs center r=\(center.r)")
    }

    /// Pins down the depth substrate: with a depth attachment on the pass
    /// (the platform host's normal shape), the closer mesh wins the
    /// center pixel regardless of which mesh was issued first. Without
    /// depth, this test would fail in whichever order draws the far
    /// mesh second — that's the regression this guards against.
    ///
    /// Geometry: camera at world (0,0,3) looking at origin, fov 90°,
    /// aspect 1. Near quad sits at z=+1 (camera-distance 2), far quad
    /// at z=-1 (camera-distance 4). Both cover the framebuffer center;
    /// the lessEqual depth test must pick whichever was drawn at
    /// smaller NDC z.
    @Test(.enabled(if: engineMetalLibraryAvailable && metal4Supported,
                   "skipped: needs test metallib + engine metallib in their bundles (xcodebuild test) and a Metal-4-capable GPU"))
    @MainActor func depthOcclusionPicksClosestRegardlessOfDrawOrder() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice(),
                                  "no Metal device — Apple Silicon hardware expected")
        let testLibrary = try #require(
            try? device.makeDefaultLibrary(bundle: .module),
            "couldn't load test metallib from Bundle.module — run via xcodebuild test"
        )

        let quadURL = try #require(
            Bundle.module.url(forResource: "quad", withExtension: "obj", subdirectory: "Meshes"),
            "quad.obj missing from test bundle"
        )
        let loader = MeshLoader(device: device, vertexDescriptor: Renderer.meshVertexDescriptor())
        let mesh = try await loader.loadMesh(from: quadURL)

        var camera = Transform.identity
        camera.translation = [0, 0, 3]
        camera.lookAt([0, 0, 0])
        let proj = float4x4.perspective(fovY: .degrees(90), aspect: 1, near: 0.1, far: 100)
        let vp = float4x4.viewPerspectiveMatrix(cameraTransform: camera, cameraPerspective: proj)

        var nearTransform = Transform.identity
        nearTransform.translation = [0, 0, 1]   // closer to camera at z=3
        var farTransform = Transform.identity
        farTransform.translation = [0, 0, -1]   // farther from camera

        // Same submission run twice, swapping draw order. A correct depth
        // implementation gives the same answer both times. A broken one
        // gives last-drawn-wins, so one of the two runs would fail.
        for nearFirst in [true, false] {
            // Fresh renderer per run so the inflight semaphore doesn't
            // need a `waitUntilCompleted` round-trip between runs to be
            // safe. Cheap to construct.
            let renderer = Renderer(device: device, gameLibrary: testLibrary)
            renderer.register(mesh)

            let color = try OffscreenColorTarget(device: device, width: 64, height: 64)
            let depth = try OffscreenDepthTarget(device: device, width: 64, height: 64)
            // Cyan clear: maximally different from both red (near) and
            // blue (far) so any leakage is loud in the readback.
            let pass = color.clearPass(MTLClearColorMake(0, 1, 1, 1), depth: depth)

            renderer.beginFrame(passDescriptor: pass)
            renderer.setCamera(viewProjection: vp)
            if nearFirst {
                renderer.drawMesh(mesh, fragmentShader: "mesh_solid_red",  meshTransform: nearTransform)
                renderer.drawMesh(mesh, fragmentShader: "mesh_solid_blue", meshTransform: farTransform)
            } else {
                renderer.drawMesh(mesh, fragmentShader: "mesh_solid_blue", meshTransform: farTransform)
                renderer.drawMesh(mesh, fragmentShader: "mesh_solid_red",  meshTransform: nearTransform)
            }
            renderer.endFrame().waitUntilCompleted()

            let pixels = color.readback()
            let center = pixels[color.width / 2, color.height / 2]
            let order = nearFirst ? "near-then-far" : "far-then-near"
            #expect(center.r > 223, "\(order): center should be red (near won), got r=\(center.r)")
            #expect(center.b < 32,  "\(order): center blue should be near 0 (far lost), got b=\(center.b)")
        }
    }
}
