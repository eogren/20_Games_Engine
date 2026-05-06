@preconcurrency import MetalKit
@preconcurrency import ModelIO

public enum MeshError: Error, Equatable {
    case invalidModel
    /// The source asset's vertex layout didn't include one or more
    /// semantics the caller's descriptor requested (e.g., the descriptor
    /// asks for UVs but the file has none). ModelIO won't fabricate
    /// intrinsic attributes — it would silently zero-fill — so we throw
    /// at load time rather than letting the renderer sample garbage.
    case missingAttributes(Set<String>)
}

/// Loads a model file (`.obj`, `.usdz`, etc.) into an `MTKMesh`. Each call
/// runs on a detached background-priority task so a chunky parse can't
/// compete with the main-actor render loop and hitch frames.
///
/// The vertex layout is fixed at construction: ModelIO reshapes every
/// loaded asset to match the supplied `MDLVertexDescriptor`, so all meshes
/// from a given loader come out with the same buffer layout — which is
/// what the renderer's mesh PSOs depend on. ModelIO will reorder, repack,
/// and convert formats automatically, but it will *not* synthesize
/// missing attributes (e.g., normals from a position-only `.obj`); that
/// has to be done explicitly when the need surfaces.
///
/// `MTLDevice` is documented thread-safe (and `Sendable`), so it's cheap to
/// share. `MTKMeshBufferAllocator` is not documented thread-safe, so we
/// build a fresh one inside each task instead of caching it on `self`.
///
/// `@unchecked Sendable`: the class is effectively immutable after init
/// (both stored properties are `let`, and `vertexDescriptor` is a defensive
/// copy that we never mutate). Each `loadMesh` call also takes its own
/// per-call copy of the descriptor before handing work to a detached task,
/// so concurrent loads never share an MDLVertexDescriptor instance. The
/// `Sendable` conformance lets `@MainActor` callers (Game.load,
/// renderer tests) await `loadMesh` without a Sendable-self violation.
public final class MeshLoader: @unchecked Sendable {
    private let device: MTLDevice
    private let vertexDescriptor: MDLVertexDescriptor

    public init(device: MTLDevice, vertexDescriptor: MDLVertexDescriptor) {
        self.device = device
        // Defensive copy so a caller mutating their descriptor after
        // construction can't reach in and change ours.
        self.vertexDescriptor = MDLVertexDescriptor(vertexDescriptor: vertexDescriptor)
    }

    public func loadMesh(from url: URL) async throws -> MTKMesh {
        let device = self.device
        // Per-call copy. Two overlapping loads must not share an
        // MDLVertexDescriptor instance — Apple doesn't document
        // MDLAsset's read/write behavior on the descriptor, so we don't
        // risk concurrent access on a shared object. Same reasoning as
        // building the allocator inside the task.
        let descriptor = MDLVertexDescriptor(vertexDescriptor: self.vertexDescriptor)
        return try await Task.detached(priority: .background) {
            let allocator = MTKMeshBufferAllocator(device: device)
            // Load with nil descriptor first so we can inspect the source's
            // actual attributes — passing the descriptor up front lets
            // ModelIO normalize the asset before we get a chance to look,
            // erasing the information about what was missing.
            let asset = MDLAsset(url: url, vertexDescriptor: nil, bufferAllocator: allocator)
            let sources = (asset.childObjects(of: MDLMesh.self) as? [MDLMesh]) ?? []
            guard sources.count == 1, let source = sources.first else {
                throw MeshError.invalidModel
            }

            let requested = Set(descriptor.attributes
                .compactMap { ($0 as? MDLVertexAttribute)?.name }
                .filter { !$0.isEmpty })
            let present = Set(source.vertexDescriptor.attributes
                .compactMap { ($0 as? MDLVertexAttribute)?.name })
            let missing = requested.subtracting(present)
            if !missing.isEmpty {
                throw MeshError.missingAttributes(missing)
            }

            // All requested semantics are present — reshape the source to
            // the caller's layout and convert.
            source.vertexDescriptor = descriptor
            return try MTKMesh(mesh: source, device: device)
        }.value
    }
}
