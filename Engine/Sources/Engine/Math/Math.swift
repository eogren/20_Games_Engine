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

    /// Rotation that aims the canonical object-local forward (`-Z`, the
    /// right-handed Apple/OpenGL convention) along the given world-space
    /// `forward` direction, with `up` as the roll reference. Built from
    /// the orthonormal basis `[right, up, -forward]`.
    ///
    /// `forward` and `up` must be non-zero and not parallel — when they
    /// are, the cross product collapses to zero and normalization
    /// produces NaN. The common gotcha is looking straight up or straight
    /// down with the default `+Y` up reference.
    public static func lookRotation(forward: Vec3, up: Vec3 = [0, 1, 0]) -> simd_quatf {
        let f = normalize(forward)
        let r = normalize(cross(f, up))
        let u = cross(r, f)
        return simd_quatf(simd_float3x3(columns: (r, u, -f)))
    }
}
