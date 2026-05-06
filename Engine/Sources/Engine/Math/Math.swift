import simd

public typealias Vec2 = SIMD2<Float>
public typealias Vec3 = SIMD3<Float>
public typealias Vec4 = SIMD4<Float>

extension simd_quatf {
    /// The zero-rotation quaternion `(0, 0, 0, 1)`. Distinct from
    /// `simd_quatf()`, which constructs the *zero* quaternion
    /// `(0, 0, 0, 0)` and produces the zero vector when used to rotate —
    /// silent garbage. Use `.identity` for "no rotation" defaults.
    public static let identity = simd_quatf(real: 1, imag: .zero)

    /// Rotation around the world X axis. Angle in radians.
    public static func aroundX(_ angle: Float) -> simd_quatf {
        simd_quatf(angle: angle, axis: [1, 0, 0])
    }

    /// Rotation around the world Y axis. Angle in radians.
    public static func aroundY(_ angle: Float) -> simd_quatf {
        simd_quatf(angle: angle, axis: [0, 1, 0])
    }

    /// Rotation around the world Z axis. Angle in radians.
    public static func aroundZ(_ angle: Float) -> simd_quatf {
        simd_quatf(angle: angle, axis: [0, 0, 1])
    }

    /// Rotation that aims the canonical object-local forward (`+Y`, the
    /// Blender right-handed Z-up convention: +X right, +Y forward, +Z up)
    /// along the given world-space `forward` direction, with `up` as the
    /// roll reference. Built from the orthonormal basis `[right, forward, up]`
    /// — column 0 / 1 / 2 are the world directions of local +X / +Y / +Z.
    ///
    /// Traps if `forward` is the zero vector or parallel/anti-parallel
    /// to `up` — both cases collapse the basis cross product to zero and
    /// would otherwise produce a NaN quaternion. The common gotcha is
    /// looking straight along world +Z or -Z with the default `+Z` up
    /// reference; pass a different `up` (e.g. `[0, 1, 0]`) when forward
    /// is near-vertical.
    public static func lookRotation(forward: Vec3, up: Vec3 = [0, 0, 1]) -> simd_quatf {
        precondition(length_squared(forward) > 1e-12,
            "simd_quatf.lookRotation: forward must be non-zero (got \(forward))")
        precondition(length_squared(cross(forward, up)) > 1e-12,
            "simd_quatf.lookRotation: forward must not be parallel to up (forward=\(forward), up=\(up))")
        let f = normalize(forward)
        let r = normalize(cross(f, up))
        let u = cross(r, f)
        return simd_quatf(simd_float3x3(columns: (r, f, u)))
    }
}
