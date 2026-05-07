import Foundation

/// A unit-tagged angle. Construct with `.radians(_:)` or `.degrees(_:)` so
/// the unit is explicit at every call site that takes an angle (`fovY`,
/// quaternion `aroundX/Y/Z`, etc.) — bare `Float` parameters silently
/// assumed radians, which is the common source of "why is my FOV
/// 57° instead of 1 radian?" bugs.
///
/// Storage is radians-canonical; `degrees` is computed. SwiftUI's `Angle`
/// is the shape reference, narrowed to `Float` to match the rest of the
/// engine's GPU-facing math.
public struct Angle: Sendable, Equatable, Hashable, Comparable {
    public var radians: Float

    public var degrees: Float { radians * 180 / .pi }

    public init(radians: Float) { self.radians = radians }

    public static func radians(_ value: Float) -> Angle { .init(radians: value) }
    public static func degrees(_ value: Float) -> Angle { .init(radians: value * .pi / 180) }

    public static let zero = Angle(radians: 0)

    public static func < (a: Angle, b: Angle) -> Bool { a.radians < b.radians }

    public static func + (a: Angle, b: Angle) -> Angle { .init(radians: a.radians + b.radians) }
    public static func - (a: Angle, b: Angle) -> Angle { .init(radians: a.radians - b.radians) }
    public static prefix func - (a: Angle) -> Angle { .init(radians: -a.radians) }

    public static func * (a: Angle, s: Float) -> Angle { .init(radians: a.radians * s) }
    public static func * (s: Float, a: Angle) -> Angle { .init(radians: a.radians * s) }
    public static func / (a: Angle, s: Float) -> Angle { .init(radians: a.radians / s) }

    public static func += (a: inout Angle, b: Angle) { a.radians += b.radians }
    public static func -= (a: inout Angle, b: Angle) { a.radians -= b.radians }
    public static func *= (a: inout Angle, s: Float) { a.radians *= s }
    public static func /= (a: inout Angle, s: Float) { a.radians /= s }
}
