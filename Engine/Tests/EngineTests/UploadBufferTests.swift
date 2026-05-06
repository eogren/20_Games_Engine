import Metal
import simd
import Testing
@testable import Engine

@Suite struct UploadBufferTests {
    /// Any Mac with a usable Metal device can run these — no Metal 4
    /// dependency, no shader compilation, no metallib required. Skip
    /// only if `MTLCreateSystemDefaultDevice` returns nil (a CI runner
    /// without GPU passthrough).
    static let device: MTLDevice? = MTLCreateSystemDefaultDevice()
    static let deviceAvailable: Bool = device != nil

    @Test(.enabled(if: deviceAvailable, "no Metal device available"))
    func allocateCopiesValueIntoBuffer() throws {
        let device = try #require(Self.device)
        let upload = UploadBuffer(device: device, length: 256)

        let value = simd_float4(1, 2, 3, 4)
        let address = upload.allocate(value)

        let offset = Int(address - upload.buffer.gpuAddress)
        let read = upload.buffer.contents()
            .advanced(by: offset)
            .load(as: simd_float4.self)
        #expect(read == value)
    }

    @Test(.enabled(if: deviceAvailable, "no Metal device available"))
    func allocatedAddressFallsWithinBuffer() throws {
        let device = try #require(Self.device)
        let length = 256
        let upload = UploadBuffer(device: device, length: length)

        let address = upload.allocate(simd_float4(1, 2, 3, 4))

        let base = upload.buffer.gpuAddress
        #expect(address >= base)
        #expect(address + UInt64(MemoryLayout<simd_float4>.stride) <= base + UInt64(length))
    }

    @Test(.enabled(if: deviceAvailable, "no Metal device available"))
    func successiveAllocationsDoNotOverlap() throws {
        let device = try #require(Self.device)
        let upload = UploadBuffer(device: device, length: 256)

        let a = simd_float4(1, 2, 3, 4)
        let b = simd_float4(5, 6, 7, 8)
        let aAddress = upload.allocate(a)
        let bAddress = upload.allocate(b)

        // b must start at or after a's end. Equality is fine because
        // simd_float4 is exactly stride-sized and 16-aligned.
        #expect(bAddress >= aAddress + UInt64(MemoryLayout<simd_float4>.stride))

        // Both reads are stable: the second allocation didn't stomp the first.
        let aOffset = Int(aAddress - upload.buffer.gpuAddress)
        let bOffset = Int(bAddress - upload.buffer.gpuAddress)
        let aRead = upload.buffer.contents().advanced(by: aOffset).load(as: simd_float4.self)
        let bRead = upload.buffer.contents().advanced(by: bOffset).load(as: simd_float4.self)
        #expect(aRead == a)
        #expect(bRead == b)
    }

    /// Allocate a 4-byte-aligned value first to misalign the bump pointer,
    /// then a 16-byte-aligned value. The second offset must be 16-aligned —
    /// this is the case stride alone wouldn't handle.
    @Test(.enabled(if: deviceAvailable, "no Metal device available"))
    func allocateAlignsToTypeAlignment() throws {
        let device = try #require(Self.device)
        let upload = UploadBuffer(device: device, length: 256)

        _ = upload.allocate(Float(1))
        let address = upload.allocate(simd_float4(1, 2, 3, 4))

        let offset = Int(address - upload.buffer.gpuAddress)
        #expect(offset.isMultiple(of: MemoryLayout<simd_float4>.alignment),
                "offset \(offset) should be \(MemoryLayout<simd_float4>.alignment)-byte aligned")
    }

    @Test(.enabled(if: deviceAvailable, "no Metal device available"))
    func clearResetsOffset() throws {
        let device = try #require(Self.device)
        let upload = UploadBuffer(device: device, length: 256)

        let firstAddress = upload.allocate(simd_float4(1, 2, 3, 4))
        upload.clear()
        let secondAddress = upload.allocate(simd_float4(5, 6, 7, 8))

        #expect(firstAddress == secondAddress)
    }
}
