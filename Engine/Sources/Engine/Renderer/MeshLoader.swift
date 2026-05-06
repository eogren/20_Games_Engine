@preconcurrency import MetalKit
@preconcurrency import ModelIO

public enum MeshError: Error {
    case invalidModel
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
public final class MeshLoader {
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
            let asset = MDLAsset(url: url, vertexDescriptor: descriptor, bufferAllocator: allocator)
            let (_, meshes) = try MTKMesh.newMeshes(asset: asset, device: device)
            guard meshes.count == 1, let mesh = meshes.first else {
                throw MeshError.invalidModel
            }
            return mesh
        }.value
    }
}
