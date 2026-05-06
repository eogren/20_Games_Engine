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
}
