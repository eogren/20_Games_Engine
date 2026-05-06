import Metal
import MetalKit
import ModelIO
import Testing
@testable import Engine

/// Tests for `MeshLoader`. These don't need a `.metallib`, just a real
/// `MTLDevice` — so they run under both `swift test` and `xcodebuild test`.
@Suite struct MeshLoaderTests {
    static let metalAvailable: Bool = MTLCreateSystemDefaultDevice() != nil

    /// Phase-1 layout: position (float3) + UV (float2), interleaved in
    /// buffer 0. Mirrors what a substrate-first mesh PSO will expect.
    static func phase1Descriptor() -> MDLVertexDescriptor {
        let d = MDLVertexDescriptor()
        d.attributes[0] = MDLVertexAttribute(
            name: MDLVertexAttributePosition,
            format: .float3,
            offset: 0,
            bufferIndex: 0)
        d.attributes[1] = MDLVertexAttribute(
            name: MDLVertexAttributeTextureCoordinate,
            format: .float2,
            offset: 12,
            bufferIndex: 0)
        d.layouts[0] = MDLVertexBufferLayout(stride: 20)
        return d
    }

    static func fixture(named name: String, ext: String) throws -> URL {
        try #require(
            Bundle.module.url(forResource: name, withExtension: ext, subdirectory: "Meshes"),
            "fixture \(name).\(ext) missing from test bundle — check Package.swift resources"
        )
    }

    @Test(.enabled(if: metalAvailable, "skipped: no Metal device"))
    func loadsSingleMeshQuad() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let loader = MeshLoader(device: device, vertexDescriptor: Self.phase1Descriptor())
        let url = try Self.fixture(named: "quad", ext: "obj")

        let mesh = try await loader.loadMesh(from: url)

        #expect(mesh.vertexCount == 4, "quad has 4 unique vertices")
        #expect(mesh.submeshes.count == 1, "one face group → one submesh")
        #expect(mesh.vertexBuffers.count == 1, "interleaved layout → single buffer")
    }

    @Test(.enabled(if: metalAvailable, "skipped: no Metal device"))
    func reshapesToRequestedLayout() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let loader = MeshLoader(device: device, vertexDescriptor: Self.phase1Descriptor())
        let url = try Self.fixture(named: "quad", ext: "obj")

        let mesh = try await loader.loadMesh(from: url)

        // The loader promised to reshape the asset to the descriptor we
        // passed in — verify the resulting mesh actually has that layout.
        let layout = try #require(mesh.vertexDescriptor.layouts[0] as? MDLVertexBufferLayout)
        #expect(layout.stride == 20, "stride = float3 (12) + float2 (8)")

        let attr0 = try #require(mesh.vertexDescriptor.attributes[0] as? MDLVertexAttribute)
        #expect(attr0.name == MDLVertexAttributePosition)
        #expect(attr0.format == .float3)
        #expect(attr0.offset == 0)

        let attr1 = try #require(mesh.vertexDescriptor.attributes[1] as? MDLVertexAttribute)
        #expect(attr1.name == MDLVertexAttributeTextureCoordinate)
        #expect(attr1.format == .float2)
        #expect(attr1.offset == 12)
    }

    // Note: there's no multi-mesh `.obj` fixture because `.obj`'s `o`
    // directive doesn't actually split into multiple MDLMeshes — ModelIO
    // collapses them into one mesh with multiple submeshes (the file has
    // a single global vertex pool, so `o` is just a named subset). The
    // `count != 1` invariant's `> 1` branch isn't exercised by these
    // tests; if/when a real .usdz with multiple meshes shows up that we
    // need to reject, fixture it then.

    @Test(.enabled(if: metalAvailable, "skipped: no Metal device"))
    func rejectsAssetMissingRequestedSemantics() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let loader = MeshLoader(device: device, vertexDescriptor: Self.phase1Descriptor())
        let url = try Self.fixture(named: "quad_no_uv", ext: "obj")

        // The descriptor asks for position + UV; the file has only
        // position. Loud throw beats a silent zero-filled UV channel.
        await #expect(throws: MeshError.missingAttributes([MDLVertexAttributeTextureCoordinate])) {
            _ = try await loader.loadMesh(from: url)
        }
    }

    @Test(.enabled(if: metalAvailable, "skipped: no Metal device"))
    func rejectsMissingFile() async throws {
        let device = try #require(MTLCreateSystemDefaultDevice())
        let loader = MeshLoader(device: device, vertexDescriptor: Self.phase1Descriptor())
        let url = URL(fileURLWithPath: "/tmp/definitely-does-not-exist-\(UUID().uuidString).obj")

        // MDLAsset(url:) doesn't throw on a missing file — it just produces
        // an empty asset. The loader's `count == 1` invariant catches it.
        await #expect(throws: MeshError.invalidModel) {
            _ = try await loader.loadMesh(from: url)
        }
    }
}
