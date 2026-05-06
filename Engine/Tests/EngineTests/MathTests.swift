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
}

private let tolerance: Float = 1e-5

private func approx(_ a: Vec3, _ b: Vec3) -> Bool {
    abs(a.x - b.x) < tolerance &&
    abs(a.y - b.y) < tolerance &&
    abs(a.z - b.z) < tolerance
}
