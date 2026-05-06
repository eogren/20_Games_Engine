import Testing
import simd
@testable import Engine

@Suite struct QuaternionExtensionTests {
    @Test func identityHasComponentsZeroZeroZeroOne() {
        let q = simd_quatf.identity
        #expect(q.imag == .zero)
        #expect(q.real == 1)
    }

    @Test func identityActsAsNoOpOnVector() {
        let v: Vec3 = [3, -2, 5]
        #expect(approx(simd_quatf.identity.act(v), v))
    }

    // Handedness checks: simd's quat follows the right-hand rule, so a
    // positive angle around an axis rotates counter-clockwise when looking
    // *down* the axis (axis pointing toward you). These three tests pin
    // that convention so a sign flip in `aroundX/Y/Z` is caught.

    @Test func aroundYByQuarterTurnTakesPlusXToMinusZ() {
        #expect(approx(simd_quatf.aroundY(.pi / 2).act([1, 0, 0]), [0, 0, -1]))
    }

    @Test func aroundXByQuarterTurnTakesPlusYToPlusZ() {
        #expect(approx(simd_quatf.aroundX(.pi / 2).act([0, 1, 0]), [0, 0, 1]))
    }

    @Test func aroundZByQuarterTurnTakesPlusXToPlusY() {
        #expect(approx(simd_quatf.aroundZ(.pi / 2).act([1, 0, 0]), [0, 1, 0]))
    }

    @Test func zeroAngleProducesIdentityRotation() {
        let v: Vec3 = [1, 2, 3]
        #expect(approx(simd_quatf.aroundX(0).act(v), v))
        #expect(approx(simd_quatf.aroundY(0).act(v), v))
        #expect(approx(simd_quatf.aroundZ(0).act(v), v))
    }

    @Test func lookRotationAlongMinusZIsIdentity() {
        // Forward = -Z, up = +Y matches the canonical object basis, so
        // the resulting rotation must be a no-op on every axis.
        let q = simd_quatf.lookRotation(forward: [0, 0, -1])
        #expect(approx(q.act([0, 0, -1]), [0, 0, -1]))
        #expect(approx(q.act([1, 0, 0]),  [1, 0, 0]))
        #expect(approx(q.act([0, 1, 0]),  [0, 1, 0]))
    }

    @Test func lookRotationAimsObjectForwardAlongInputForward() {
        let target: Vec3 = [3, 0, 0]
        let q = simd_quatf.lookRotation(forward: target)
        // Object's local forward is -Z; after lookRotation it should
        // align with the normalized input forward.
        #expect(approx(q.act([0, 0, -1]), normalize(target)))
    }

    @Test func lookRotationNormalizesInput() {
        // Same direction, different magnitude — must produce the same
        // rotation (within tolerance).
        let q1 = simd_quatf.lookRotation(forward: [1, 0, 0])
        let q2 = simd_quatf.lookRotation(forward: [100, 0, 0])
        #expect(approx(q1.act([0, 0, -1]), q2.act([0, 0, -1])))
    }
}

@Suite struct TransformTests {
    @Test func identityIsAtOriginNoRotationUnitScale() {
        let t = Transform.identity
        #expect(t.translation == .zero)
        #expect(t.scale == .one)
        #expect(t.rotation.imag == .zero)
        #expect(t.rotation.real == 1)
    }

    @Test func defaultInitMatchesIdentity() {
        let t = Transform()
        let id = Transform.identity
        #expect(t.translation == id.translation)
        #expect(t.scale == id.scale)
        #expect(t.rotation.real == id.rotation.real)
        #expect(t.rotation.imag == id.rotation.imag)
    }

    @Test func customInitPopulatesAllFields() {
        let t = Transform(
            translation: [1, 2, 3],
            rotation: .aroundY(.pi / 2),
            scale: [0.5, 0.5, 0.5]
        )
        #expect(t.translation == [1, 2, 3])
        #expect(t.scale == [0.5, 0.5, 0.5])
        #expect(approx(t.rotation.act([1, 0, 0]), [0, 0, -1]))
    }

    @Test func lookAtFromOriginToPlusXAimsForwardAtPlusX() {
        var t = Transform.identity
        t.lookAt([10, 0, 0])
        // Object's local forward (-Z), rotated by the new orientation,
        // should point along +X.
        #expect(approx(t.rotation.act([0, 0, -1]), [1, 0, 0]))
    }

    @Test func lookAtAimsAtTargetFromArbitraryPosition() {
        var t = Transform(translation: [0, 0, 5])
        t.lookAt([0, 0, -5])
        let forwardWorld = t.rotation.act([0, 0, -1])
        // Looking from +5Z toward -5Z means the world-space forward
        // direction is -Z.
        #expect(approx(forwardWorld, [0, 0, -1]))
    }

    @Test func lookAtPreservesTranslationAndScale() {
        var t = Transform(
            translation: [1, 2, 3],
            scale: [0.5, 2, 0.5]
        )
        t.lookAt([10, 10, 10])
        #expect(t.translation == [1, 2, 3])
        #expect(t.scale == [0.5, 2, 0.5])
    }
}

private let tolerance: Float = 1e-5

private func approx(_ a: Vec3, _ b: Vec3) -> Bool {
    abs(a.x - b.x) < tolerance &&
    abs(a.y - b.y) < tolerance &&
    abs(a.z - b.z) < tolerance
}
