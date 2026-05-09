#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"

#include "math/Math.h"

#include <cmath>
#include <numbers>

using doctest::Approx;
using namespace math_literals;

// ---------- Angle ------------------------------------------------------------

TEST_CASE("Angle: factory functions match expected radians")
{
    CHECK(Angle::from_radians(0.0f).radians() == 0.0f);
    CHECK(Angle::from_radians(1.5f).radians() == 1.5f);
    CHECK(Angle::from_degrees(0.0f).radians() == 0.0f);
    CHECK(Angle::from_degrees(180.0f).radians() == Approx(std::numbers::pi_v<float>));
    CHECK(Angle::from_degrees(90.0f).radians() == Approx(std::numbers::pi_v<float> / 2.0f));
    CHECK(Angle::zero().radians() == 0.0f);
}

TEST_CASE("Angle: round-trips through both unit accessors")
{
    const auto a = Angle::from_degrees(45.0f);
    CHECK(a.degrees() == Approx(45.0f));
    CHECK(a.radians() == Approx(std::numbers::pi_v<float> / 4.0f));

    const auto b = Angle::from_radians(std::numbers::pi_v<float>);
    CHECK(b.degrees() == Approx(180.0f));
}

TEST_CASE("Angle: arithmetic preserves units")
{
    CHECK((Angle::from_degrees(45.0f) + Angle::from_degrees(45.0f)).degrees() == Approx(90.0f));
    CHECK((Angle::from_degrees(90.0f) - Angle::from_degrees(30.0f)).degrees() == Approx(60.0f));
    CHECK((-Angle::from_degrees(45.0f)).degrees() == Approx(-45.0f));
    CHECK((Angle::from_degrees(45.0f) * 2.0f).degrees() == Approx(90.0f));
    CHECK((3.0f * Angle::from_degrees(30.0f)).degrees() == Approx(90.0f));
    CHECK((Angle::from_degrees(90.0f) / 2.0f).degrees() == Approx(45.0f));

    auto a = Angle::from_degrees(10.0f);
    a += Angle::from_degrees(5.0f);
    CHECK(a.degrees() == Approx(15.0f));
    a -= Angle::from_degrees(15.0f);
    CHECK(a.degrees() == Approx(0.0f));
    a = Angle::from_degrees(20.0f);
    a *= 2.0f;
    CHECK(a.degrees() == Approx(40.0f));
    a /= 4.0f;
    CHECK(a.degrees() == Approx(10.0f));
}

TEST_CASE("Angle: spaceship gives all six comparisons")
{
    const auto small = Angle::from_degrees(30.0f);
    const auto big = Angle::from_degrees(45.0f);
    CHECK(small < big);
    CHECK(big > small);
    CHECK(small <= small);
    CHECK(big >= big);
    CHECK(small == Angle::from_degrees(30.0f));
    CHECK(small != big);
}

TEST_CASE("Angle: literals construct correctly")
{
    CHECK((45.0_deg).degrees() == Approx(45.0f));
    CHECK((1.0_rad).radians() == Approx(1.0f));
    CHECK((90.0_deg + 90.0_deg).degrees() == Approx(180.0f));
}

TEST_CASE("Angle: usable in constexpr context")
{
    constexpr auto a = Angle::from_degrees(180.0f);
    constexpr float r = a.radians();
    static_assert(a.degrees() == 180.0f);
    CHECK(r == Approx(std::numbers::pi_v<float>));
}

// ---------- model_ts ---------------------------------------------------------

namespace
{
// Multiply a 4x4 by a 4-vector since we don't pull in any helpers besides simd.
simd::float4 apply(simd::float4x4 m, simd::float4 v)
{
    return m.columns[0] * v.x + m.columns[1] * v.y + m.columns[2] * v.z + m.columns[3] * v.w;
}
}  // namespace

TEST_CASE("model_ts: default scale yields identity-scale model")
{
    const auto m = model_ts(simd::float3{0.0f, 0.0f, 0.0f});
    const auto p = apply(m, simd::float4{2.0f, 3.0f, 4.0f, 1.0f});
    CHECK(p.x == Approx(2.0f));
    CHECK(p.y == Approx(3.0f));
    CHECK(p.z == Approx(4.0f));
    CHECK(p.w == Approx(1.0f));
}

TEST_CASE("model_ts: translation moves the origin")
{
    const auto m = model_ts(simd::float3{1.0f, 2.0f, 3.0f});
    const auto origin = apply(m, simd::float4{0.0f, 0.0f, 0.0f, 1.0f});
    CHECK(origin.x == Approx(1.0f));
    CHECK(origin.y == Approx(2.0f));
    CHECK(origin.z == Approx(3.0f));
}

TEST_CASE("model_ts: scale-then-translate composition order")
{
    // Scale should apply in local space, then translation.
    // Point (1,1,1) under scale (2,3,4) and translation (10,20,30) → (12,23,34).
    const auto m = model_ts(simd::float3{10.0f, 20.0f, 30.0f},
                            simd::float3{2.0f, 3.0f, 4.0f});
    const auto p = apply(m, simd::float4{1.0f, 1.0f, 1.0f, 1.0f});
    CHECK(p.x == Approx(12.0f));
    CHECK(p.y == Approx(23.0f));
    CHECK(p.z == Approx(34.0f));
    CHECK(p.w == Approx(1.0f));
}

TEST_CASE("model_ts: w of a direction vector is preserved as zero")
{
    const auto m = model_ts(simd::float3{5.0f, 5.0f, 5.0f}, simd::float3{2.0f, 2.0f, 2.0f});
    const auto dir = apply(m, simd::float4{1.0f, 0.0f, 0.0f, 0.0f});
    // Directions (w=0) should not pick up translation, only scale.
    CHECK(dir.x == Approx(2.0f));
    CHECK(dir.y == Approx(0.0f));
    CHECK(dir.z == Approx(0.0f));
    CHECK(dir.w == Approx(0.0f));
}

// ---------- ortho_rh ---------------------------------------------------------

TEST_CASE("ortho_rh: maps view-space corners to expected NDC")
{
    constexpr float L = -2.0f, R = 3.0f, B = -1.0f, T = 4.0f, N = 0.5f, F = 10.0f;
    const auto m = ortho_rh(L, R, B, T, N, F);

    // (left, bottom, -near) -> (-1, -1, 0)
    auto a = apply(m, simd::float4{L, B, -N, 1.0f});
    CHECK(a.x == Approx(-1.0f));
    CHECK(a.y == Approx(-1.0f));
    CHECK(a.z == Approx(0.0f));
    CHECK(a.w == Approx(1.0f));

    // (right, top, -far) -> (1, 1, 1)
    auto b = apply(m, simd::float4{R, T, -F, 1.0f});
    CHECK(b.x == Approx(1.0f));
    CHECK(b.y == Approx(1.0f));
    CHECK(b.z == Approx(1.0f));
    CHECK(b.w == Approx(1.0f));

    // Center of frustum -> (0, 0, 0.5)-ish.
    auto c = apply(m, simd::float4{(L + R) * 0.5f, (B + T) * 0.5f, -(N + F) * 0.5f, 1.0f});
    CHECK(c.x == Approx(0.0f));
    CHECK(c.y == Approx(0.0f));
    CHECK(c.z == Approx(0.5f));
}

TEST_CASE("ortho_rh: w stays 1 (no perspective divide)")
{
    const auto m = ortho_rh(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f);
    const auto p = apply(m, simd::float4{0.7f, -0.2f, -50.0f, 1.0f});
    CHECK(p.w == Approx(1.0f));
}
