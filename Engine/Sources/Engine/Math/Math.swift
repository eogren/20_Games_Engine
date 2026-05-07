import simd

public typealias Vec2 = SIMD2<Float>
public typealias Vec3 = SIMD3<Float>
public typealias Vec4 = SIMD4<Float>

/// Sugar for `simd_float4x4`. Column-major (Metal's convention), so the
/// CPU-built matrix uploads to a vertex shader without transposing.
public typealias float4x4 = simd_float4x4

extension float4x4 {
    /// Right-handed perspective projection matching Metal's clip-space
    /// conventions: `x, y` in `[-1, 1]`, `z` in `[0, 1]` (not `[-1, 1]` —
    /// that's the GL convention). Camera looks down `-Z` in view space;
    /// view-space `z = -near` maps to clip `z = 0`, view-space `z = -far`
    /// maps to clip `z = 1`.
    ///
    /// Getting the `z` range wrong produces a depth buffer that's either
    /// degenerate (everything at one depth) or inverted relative to the
    /// depth-test direction, and the symptom is silent — geometry just
    /// disappears or z-fights, with no validation error.
    ///
    /// Traps if any input is non-finite, `fovY` or `aspect` is non-positive,
    /// or `far <= near`. All four constraints are required for a usable
    /// projection; violating them yields a singular or NaN-filled matrix.
    public static func perspective(
        fovY: Angle, aspect: Float, near: Float, far: Float
    ) -> float4x4 {
        let fovYRadians = fovY.radians
        precondition(fovYRadians.isFinite && aspect.isFinite && near.isFinite && far.isFinite,
            "float4x4.perspective: all inputs must be finite (fovY=\(fovYRadians)rad, aspect=\(aspect), near=\(near), far=\(far))")
        precondition(fovYRadians > 0, "float4x4.perspective: fovY must be > 0 (got \(fovYRadians)rad)")
        precondition(aspect > 0, "float4x4.perspective: aspect must be > 0 (got \(aspect))")
        precondition(near > 0, "float4x4.perspective: near must be > 0 (got \(near))")
        precondition(far > near, "float4x4.perspective: far must be > near (near=\(near), far=\(far))")

        let f = 1 / tan(fovYRadians / 2)
        let zScale = far / (near - far)        // negative; maps view -near→0, -far→1 after divide
        let zBias  = far * near / (near - far) // negative
        return float4x4(
            SIMD4<Float>(f / aspect, 0,  0,      0),
            SIMD4<Float>(0,          f,  0,      0),
            SIMD4<Float>(0,          0,  zScale, -1),
            SIMD4<Float>(0,          0,  zBias,  0)
        )
    }

    /// Combined view-projection matrix for a camera placed at
    /// `cameraTransform` looking through `cameraPerspective`. The world-
    /// to-view matrix is the inverse of the camera's transform — i.e.,
    /// "where the world goes when the camera moves to the origin." With
    /// the engine's Y-up world / -Z forward object convention,
    /// `cameraTransform.matrix.inverse` matches view space directly, so
    /// no basis swap is needed before the projection.
    public static func viewPerspectiveMatrix(
        cameraTransform: Transform, cameraPerspective: float4x4
    ) -> float4x4 {
        cameraPerspective * cameraTransform.matrix.inverse
    }
}

extension simd_quatf {
    /// The zero-rotation quaternion `(0, 0, 0, 1)`. Distinct from
    /// `simd_quatf()`, which constructs the *zero* quaternion
    /// `(0, 0, 0, 0)` and produces the zero vector when used to rotate —
    /// silent garbage. Use `.identity` for "no rotation" defaults.
    public static let identity = simd_quatf(real: 1, imag: .zero)

    /// Rotation around the world X axis.
    public static func aroundX(_ angle: Angle) -> simd_quatf {
        simd_quatf(angle: angle.radians, axis: [1, 0, 0])
    }

    /// Rotation around the world Y axis.
    public static func aroundY(_ angle: Angle) -> simd_quatf {
        simd_quatf(angle: angle.radians, axis: [0, 1, 0])
    }

    /// Rotation around the world Z axis.
    public static func aroundZ(_ angle: Angle) -> simd_quatf {
        simd_quatf(angle: angle.radians, axis: [0, 0, 1])
    }

    /// Rotation that aims the canonical object-local forward (`-Z`, the
    /// right-handed Apple/OpenGL convention) along the given world-space
    /// `forward` direction, with `up` as the roll reference. Built from
    /// the orthonormal basis `[right, up, -forward]`.
    ///
    /// Traps if `forward` is the zero vector or parallel/anti-parallel
    /// to `up` — both cases collapse the basis cross product to zero and
    /// would otherwise produce a NaN quaternion. The common gotcha is
    /// looking straight up or straight down with the default `+Y` up
    /// reference; pass a different `up` (e.g. `[0, 0, -1]`) when forward
    /// is near-vertical.
    public static func lookRotation(forward: Vec3, up: Vec3 = [0, 1, 0]) -> simd_quatf {
        precondition(length_squared(forward) > 1e-12,
            "simd_quatf.lookRotation: forward must be non-zero (got \(forward))")
        precondition(length_squared(cross(forward, up)) > 1e-12,
            "simd_quatf.lookRotation: forward must not be parallel to up (forward=\(forward), up=\(up))")
        let f = normalize(forward)
        let r = normalize(cross(f, up))
        let u = cross(r, f)
        return simd_quatf(simd_float3x3(columns: (r, u, -f)))
    }
}
