import simd

/// Decomposed pose: translation, rotation, scale, plus a cached 4x4
/// `matrix` rebuilt automatically whenever a component is written.
///
/// Storage is quat-canonical (not Euler scalars), matching Bevy's
/// `Transform`. Hierarchy compose, slerp interpolation, and integrated
/// rotations (physics torque, look-at-velocity) all operate cleanly on
/// quaternions and stumble on Euler's gimbal-lock and round-trip
/// ambiguity. Games that prefer scalar-accumulator ergonomics (e.g. a
/// constant-rate spinning cube) own the scalar on their own state and
/// rebuild `transform.rotation` from it each tick:
///
///     yAngle += dt * spinRate
///     cube.transform.rotation = .aroundY(yAngle)
///
/// The cached `matrix` is for game-side use (transforming points
/// between object and world space, collision queries, attachment
/// points). The renderer's draw API still takes simulation-shaped
/// `(position, rotation)` inputs per the substrate decision and
/// composes its own model matrix internally; `Transform.matrix` is not
/// on that path.
public struct Transform: Sendable {
    public var translation: Vec3 = .zero {
        didSet { rebuildMatrix() }
    }
    public var rotation: simd_quatf = .identity {
        didSet { rebuildMatrix() }
    }
    public var scale: Vec3 = .one {
        didSet { rebuildMatrix() }
    }

    /// Cached 4x4 model matrix, rebuilt eagerly on every component
    /// write via `didSet`. Composed as `T * R * S` (scale, then rotate,
    /// then translate when applied to an object-space point), Metal's
    /// column-major convention, ready to upload as a vertex-shader
    /// model matrix.
    ///
    /// Read-only — callers update the matrix by writing the components.
    /// Multi-component changes pay one rebuild per write; if that ever
    /// shows up in a profile, switch to a dirty-flag pattern (rebuild
    /// lazily on read).
    public private(set) var matrix: simd_float4x4

    public init(
        translation: Vec3 = .zero,
        rotation: simd_quatf = .identity,
        scale: Vec3 = .one
    ) {
        self.translation = translation
        self.rotation = rotation
        self.scale = scale
        // `didSet` doesn't fire during init, so populate the cache
        // explicitly. Without this, a fresh Transform reads back the
        // stored property's default `simd_float4x4()` (zero matrix) —
        // garbage on first read.
        self.matrix = Self.compose(translation: translation, rotation: rotation, scale: scale)
    }

    public static let identity = Transform()

    /// Aim the object's forward (`-Z` in object space) at `target`,
    /// rotating in place around `translation`. `worldUp` stabilizes
    /// roll; defaults to `+Y`. Translation and scale are untouched.
    ///
    /// Traps if `target` equals `translation` (no direction to aim) or
    /// if the resulting forward is parallel to `worldUp` (no unique roll
    /// — see `simd_quatf.lookRotation`). Common parallel-case gotcha is
    /// looking straight up or down with default `+Y` up; pass a
    /// different `worldUp` when forward is near-vertical.
    public mutating func lookAt(_ target: Vec3, worldUp: Vec3 = [0, 1, 0]) {
        precondition(length_squared(target - translation) > 1e-12,
            "Transform.lookAt: target must differ from translation (both = \(target))")
        rotation = .lookRotation(forward: target - translation, up: worldUp)
    }

    private mutating func rebuildMatrix() {
        matrix = Self.compose(translation: translation, rotation: rotation, scale: scale)
    }

    private static func compose(
        translation: Vec3,
        rotation: simd_quatf,
        scale: Vec3
    ) -> simd_float4x4 {
        // T * R * S — applied to an object-space point: scale first,
        // then rotate, then translate to world position.
        let t = simd_float4x4(
            SIMD4<Float>(1, 0, 0, 0),
            SIMD4<Float>(0, 1, 0, 0),
            SIMD4<Float>(0, 0, 1, 0),
            SIMD4<Float>(translation.x, translation.y, translation.z, 1)
        )
        let r = simd_float4x4(rotation)
        let s = simd_float4x4(diagonal: SIMD4<Float>(scale.x, scale.y, scale.z, 1))
        return t * r * s
    }
}
