import simd

/// Decomposed pose: translation, rotation, scale. The renderer composes a
/// 4x4 model matrix from these at draw time; game code never holds a
/// matrix.
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
public struct Transform: Sendable {
    public var translation: Vec3 = .zero
    public var rotation: simd_quatf = .identity
    public var scale: Vec3 = .one

    public init(
        translation: Vec3 = .zero,
        rotation: simd_quatf = .identity,
        scale: Vec3 = .one
    ) {
        self.translation = translation
        self.rotation = rotation
        self.scale = scale
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
}
