import Metal

/// CPU-writable bump allocator over a shared-memory `MTLBuffer`. The
/// renderer copies small uniform structs in via `allocate`, gets back a
/// GPU address suitable for `MTL4ArgumentTable.setAddress`, and resets
/// the bump pointer with `clear()` once per frame after the GPU has
/// consumed the previous frame's contents. Single-in-flight only — if
/// multiple frames need to overlap on the GPU, instantiate one
/// `UploadBuffer` per slot.
///
/// `T`'s Swift layout must match the MSL `constant T&` binding's layout.
/// Since uniform structs in this engine use `simd_*` types — which share
/// alignment with their MSL counterparts — `MemoryLayout<T>.alignment`
/// is what the GPU expects, and `allocate` uses it directly.
final class UploadBuffer {
    /// Underlying buffer; exposed so the renderer can register it with a
    /// residency set. Don't write through it directly — go through
    /// `allocate` so the bump pointer stays in sync.
    let buffer: MTLBuffer
    private let capacity: Int
    private var used: Int

    init(device: MTLDevice, length: Int, label: String? = nil) {
        guard let buffer = device.makeBuffer(length: length, options: .storageModeShared) else {
            fatalError("UploadBuffer: makeBuffer(length: \(length)) returned nil")
        }
        buffer.label = label
        self.buffer = buffer
        self.capacity = length
        self.used = 0
    }

    func clear() {
        used = 0
    }

    /// Bump-allocate `MemoryLayout<T>.stride` bytes (rounded up to
    /// `T`'s alignment), copy `value` in, and return the GPU address of
    /// the copy — ready to hand to `argumentTable.setAddress(_:index:)`.
    func allocate<T: BitwiseCopyable>(_ value: T) -> UInt64 {
        let stride = MemoryLayout<T>.stride
        let alignment = MemoryLayout<T>.alignment
        let aligned = (used + alignment - 1) & ~(alignment - 1)
        guard aligned + stride <= capacity else {
            fatalError("UploadBuffer: overflow (\(stride) bytes won't fit at offset \(aligned) of \(capacity))")
        }
        var copy = value
        let dst = buffer.contents().advanced(by: aligned)
        withUnsafePointer(to: &copy) { src in
            dst.copyMemory(from: UnsafeRawPointer(src), byteCount: stride)
        }
        used = aligned + stride
        return buffer.gpuAddress + UInt64(aligned)
    }
}
