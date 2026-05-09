#pragma once
// <math.h> (C header) before <simd/simd.h>: SDK 26.4's <simd/math.h> calls
// ::acosf etc. from the global namespace, which <cmath> alone doesn't expose.
// Most Apple framework headers pull <math.h> in transitively, so callers that
// already include Foundation/Metal don't notice — but a bare TU like a unit
// test does.
#include <math.h>
#include <simd/simd.h>

#include <numbers>

// Unit-tagged angle. Construct with Angle::from_radians / Angle::from_degrees
// (or the _rad / _deg literals) so the unit is explicit at every call site
// that takes an angle. Bare-float parameters silently assume radians, which
// is the classic source of "why is my FOV 57° instead of 1 radian?" bugs.
//
// Storage is radians-canonical; degrees are computed. Mirrors the Swift
// engine's Angle type.
class Angle
{
public:
    constexpr Angle() = default;

    static constexpr Angle from_radians(float v) { return Angle{v}; }
    static constexpr Angle from_degrees(float v)
    {
        return Angle{v * (std::numbers::pi_v<float> / 180.0f)};
    }
    static constexpr Angle zero() { return Angle{0.0f}; }

    constexpr float radians() const { return radians_; }
    constexpr float degrees() const { return radians_ * (180.0f / std::numbers::pi_v<float>); }

    constexpr auto operator<=>(const Angle&) const = default;

    constexpr Angle operator-() const { return Angle{-radians_}; }
    constexpr Angle operator+(Angle o) const { return Angle{radians_ + o.radians_}; }
    constexpr Angle operator-(Angle o) const { return Angle{radians_ - o.radians_}; }
    constexpr Angle operator*(float s) const { return Angle{radians_ * s}; }
    constexpr Angle operator/(float s) const { return Angle{radians_ / s}; }

    constexpr Angle& operator+=(Angle o) { radians_ += o.radians_; return *this; }
    constexpr Angle& operator-=(Angle o) { radians_ -= o.radians_; return *this; }
    constexpr Angle& operator*=(float s) { radians_ *= s; return *this; }
    constexpr Angle& operator/=(float s) { radians_ /= s; return *this; }

private:
    explicit constexpr Angle(float radians) : radians_{radians} {}
    float radians_ = 0.0f;
};

constexpr Angle operator*(float s, Angle a) { return a * s; }

// Opt-in literals: `using namespace math_literals;` then `45.0_deg` or `1.57_rad`.
namespace math_literals
{
    constexpr Angle operator""_rad(long double v)
    {
        return Angle::from_radians(static_cast<float>(v));
    }
    constexpr Angle operator""_deg(long double v)
    {
        return Angle::from_degrees(static_cast<float>(v));
    }
}

// Right-handed orthographic projection for Metal clip-space (Z in [0, 1]).
// Camera looks down -Z; near and far are positive distances along the view axis.
// Maps view-space [left, right] x [bottom, top] x [-far, -near] to clip
// [-1, 1] x [-1, 1] x [0, 1].
//
// Not constexpr: simd::float4x4's constructors aren't annotated in Apple's SDK
// headers, and simd_float4 is a clang ext_vector_type. inline + -O folds the
// arithmetic at known-arg sites anyway.
inline simd::float4x4 ortho_rh(float left, float right,
                               float bottom, float top,
                               float near, float far)
{
    const float rml = right - left;
    const float tmb = top - bottom;
    const float fmn = far - near;
    return simd::float4x4{
        simd::float4{ 2.0f / rml,            0.0f,                  0.0f,         0.0f },
        simd::float4{ 0.0f,                  2.0f / tmb,            0.0f,         0.0f },
        simd::float4{ 0.0f,                  0.0f,                 -1.0f / fmn,   0.0f },
        simd::float4{ -(right + left) / rml, -(top + bottom) / tmb, -near / fmn,  1.0f },
    };
}

// Model matrix from translation and per-axis scale, no rotation.
// Equivalent to T * S (scale first, then translate) — a point p in local space
// maps to translation + scale * p.
inline simd::float4x4 model_ts(simd::float3 translation,
                               simd::float3 scale = simd::float3{1.0f, 1.0f, 1.0f})
{
    return simd::float4x4{
        simd::float4{ scale.x, 0.0f,    0.0f,    0.0f },
        simd::float4{ 0.0f,    scale.y, 0.0f,    0.0f },
        simd::float4{ 0.0f,    0.0f,    scale.z, 0.0f },
        simd::float4{ translation.x, translation.y, translation.z, 1.0f },
    };
}
