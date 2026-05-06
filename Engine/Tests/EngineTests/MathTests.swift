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

    @Test func lookRotationAlongPlusYIsIdentity() {
        // Forward = +Y, up = +Z matches the canonical Blender Z-up
        // object basis, so the resulting rotation must be a no-op on
        // every axis.
        let q = simd_quatf.lookRotation(forward: [0, 1, 0])
        #expect(approx(q.act([0, 1, 0]), [0, 1, 0]))
        #expect(approx(q.act([1, 0, 0]), [1, 0, 0]))
        #expect(approx(q.act([0, 0, 1]), [0, 0, 1]))
    }

    @Test func lookRotationAimsObjectForwardAlongInputForward() {
        let target: Vec3 = [3, 0, 0]
        let q = simd_quatf.lookRotation(forward: target)
        // Object's local forward is +Y; after lookRotation it should
        // align with the normalized input forward.
        #expect(approx(q.act([0, 1, 0]), normalize(target)))
    }

    @Test func lookRotationNormalizesInput() {
        // Same direction, different magnitude — must produce the same
        // rotation (within tolerance).
        let q1 = simd_quatf.lookRotation(forward: [1, 0, 0])
        let q2 = simd_quatf.lookRotation(forward: [100, 0, 0])
        #expect(approx(q1.act([0, 1, 0]), q2.act([0, 1, 0])))
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
        // Object's local forward (+Y, Blender Z-up), rotated by the new
        // orientation, should point along +X.
        #expect(approx(t.rotation.act([0, 1, 0]), [1, 0, 0]))
    }

    @Test func lookAtAimsAtTargetFromArbitraryPosition() {
        // Pick an axis perpendicular to default worldUp (+Z) so the
        // direction isn't parallel to up. Looking from +5X to -5X means
        // the world-space forward direction is -X.
        var t = Transform(translation: [5, 0, 0])
        t.lookAt([-5, 0, 0])
        let forwardWorld = t.rotation.act([0, 1, 0])
        #expect(approx(forwardWorld, [-1, 0, 0]))
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

    @Test func identityMatrixIsIdentity4x4() {
        // Origin maps to origin, every basis vector unchanged.
        let m = Transform.identity.matrix
        #expect(approx(applyMatrix(m, to: [0, 0, 0]), [0, 0, 0]))
        #expect(approx(applyMatrix(m, to: [1, 0, 0]), [1, 0, 0]))
        #expect(approx(applyMatrix(m, to: [0, 1, 0]), [0, 1, 0]))
        #expect(approx(applyMatrix(m, to: [0, 0, 1]), [0, 0, 1]))
    }

    @Test func translationOnlyMovesOrigin() {
        let t = Transform(translation: [5, -2, 3])
        #expect(approx(applyMatrix(t.matrix, to: [0, 0, 0]), [5, -2, 3]))
        #expect(approx(applyMatrix(t.matrix, to: [1, 1, 1]), [6, -1, 4]))
    }

    @Test func rotationOnlyRotatesAroundOrigin() {
        // aroundY(π/2) takes +X to -Z (validated in QuaternionExtensionTests).
        let t = Transform(rotation: .aroundY(.pi / 2))
        #expect(approx(applyMatrix(t.matrix, to: [1, 0, 0]), [0, 0, -1]))
        #expect(approx(applyMatrix(t.matrix, to: [0, 1, 0]), [0, 1, 0]))
    }

    @Test func scaleOnlyScalesAroundOrigin() {
        let t = Transform(scale: [2, 1, 0.5])
        #expect(approx(applyMatrix(t.matrix, to: [1, 1, 1]), [2, 1, 0.5]))
        #expect(approx(applyMatrix(t.matrix, to: [0, 0, 0]), [0, 0, 0]))
    }

    @Test func combinedTRSAppliesInScaleRotateTranslateOrder() {
        // Object-space (1, 0, 0):
        //   scale [2, 1, 1] → (2, 0, 0)
        //   rotate aroundY(π/2) → (0, 0, -2)
        //   translate [10, 0, 0] → (10, 0, -2)
        let t = Transform(
            translation: [10, 0, 0],
            rotation: .aroundY(.pi / 2),
            scale: [2, 1, 1]
        )
        #expect(approx(applyMatrix(t.matrix, to: [1, 0, 0]), [10, 0, -2]))
    }

    @Test func writingTranslationRebuildsMatrix() {
        var t = Transform.identity
        t.translation = [3, 4, 5]
        // Matrix should immediately reflect the new translation.
        #expect(approx(applyMatrix(t.matrix, to: [0, 0, 0]), [3, 4, 5]))
    }

    @Test func writingRotationRebuildsMatrix() {
        var t = Transform.identity
        t.rotation = .aroundZ(.pi / 2)
        // aroundZ(π/2) takes +X to +Y.
        #expect(approx(applyMatrix(t.matrix, to: [1, 0, 0]), [0, 1, 0]))
    }

    @Test func writingScaleRebuildsMatrix() {
        var t = Transform.identity
        t.scale = [3, 3, 3]
        #expect(approx(applyMatrix(t.matrix, to: [1, 1, 1]), [3, 3, 3]))
    }

    @Test func lookAtUpdatesMatrix() {
        var t = Transform(translation: [0, 0, 0])
        t.lookAt([10, 0, 0])
        // After lookAt, object-local forward (+Y, Blender Z-up) should
        // map to +X direction in world space (translation is origin, so
        // the matrix's rotation portion alone determines the result).
        #expect(approx(applyMatrix(t.matrix, to: [0, 1, 0]), [1, 0, 0]))
    }
}

@Suite struct PerspectiveProjectionTests {
    // Right-handed view space: camera looks down -Z, so points in front
    // have negative view-space z. Metal NDC z range is [0, 1] (not the
    // [-1, 1] GL convention) — these tests pin that boundary on both
    // sides of the frustum.

    @Test func pointOnNearPlaneMapsToNdcZZero() {
        let p = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: 1, far: 100)
        let ndc = projectThenDivide(p, view: [0, 0, -1])
        #expect(abs(ndc.z - 0) < tolerance)
    }

    @Test func pointOnFarPlaneMapsToNdcZOne() {
        let p = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: 1, far: 100)
        let ndc = projectThenDivide(p, view: [0, 0, -100])
        #expect(abs(ndc.z - 1) < tolerance)
    }

    @Test func midDepthLandsBetweenZeroAndOne() {
        // A point between near and far must produce 0 < ndc.z < 1.
        // Catches sign flips that would put it outside the unit range
        // even when the endpoints happen to land correctly.
        let p = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: 1, far: 100)
        let ndc = projectThenDivide(p, view: [0, 0, -10])
        #expect(ndc.z > 0 && ndc.z < 1)
    }

    @Test func depthIsMonotonicallyIncreasingWithDistance() {
        // Closer to the camera → smaller ndc.z. A sign error on the z
        // row would invert this and make the depth test reject the wrong
        // surfaces.
        let p = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: 1, far: 100)
        let near = projectThenDivide(p, view: [0, 0, -2]).z
        let far  = projectThenDivide(p, view: [0, 0, -50]).z
        #expect(near < far)
    }

    @Test func ninetyDegreeFovMapsNearPlaneEdgeToNdcOne() {
        // With fovY = 90° and aspect = 1, the near plane is a 2x2 square
        // at z = -near. A point at (near, 0, -near) sits on the right
        // edge and should project to ndc.x = +1.
        let near: Float = 1
        let p = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: near, far: 100)
        let ndc = projectThenDivide(p, view: [near, 0, -near])
        #expect(abs(ndc.x - 1) < tolerance)
        #expect(abs(ndc.y - 0) < tolerance)
    }

    @Test func aspectRatioCompressesXAxis() {
        // Aspect = 2 (wider than tall) means horizontal FoV is wider, so
        // the same world-space x lands at a smaller |ndc.x| than with
        // aspect = 1. A point that hits ndc.x = 1 at aspect 1 should be
        // at ndc.x = 0.5 at aspect 2.
        let near: Float = 1
        let pSquare = float4x4.perspective(fovY: .pi / 2, aspect: 1, near: near, far: 100)
        let pWide   = float4x4.perspective(fovY: .pi / 2, aspect: 2, near: near, far: 100)
        let edge: SIMD4<Float> = [near, 0, -near, 1]
        let ndcSquare = perspectiveDivide(pSquare * edge)
        let ndcWide   = perspectiveDivide(pWide * edge)
        #expect(abs(ndcSquare.x - 1) < tolerance)
        #expect(abs(ndcWide.x - 0.5) < tolerance)
    }

    @Test func clipWEqualsNegativeViewZ() {
        // Pre-divide w must equal -view.z so the perspective divide by w
        // is a divide by positive depth-from-camera. If w is built wrong
        // the perspective divide flips signs and the rasterizer culls
        // everything.
        let p = float4x4.perspective(fovY: .pi / 3, aspect: 1.5, near: 0.1, far: 50)
        let clip = p * SIMD4<Float>(0, 0, -7, 1)
        #expect(abs(clip.w - 7) < tolerance)
    }
}

private func projectThenDivide(_ m: float4x4, view: Vec3) -> Vec3 {
    let clip = m * SIMD4<Float>(view.x, view.y, view.z, 1)
    return perspectiveDivide(clip)
}

private func perspectiveDivide(_ clip: SIMD4<Float>) -> Vec3 {
    Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w)
}

private func applyMatrix(_ m: simd_float4x4, to p: Vec3) -> Vec3 {
    let r = m * SIMD4<Float>(p.x, p.y, p.z, 1)
    return Vec3(r.x, r.y, r.z)
}

private let tolerance: Float = 1e-5

private func approx(_ a: Vec3, _ b: Vec3) -> Bool {
    abs(a.x - b.x) < tolerance &&
    abs(a.y - b.y) < tolerance &&
    abs(a.z - b.z) < tolerance
}
