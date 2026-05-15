// doctest's IMPLEMENT_WITH_MAIN lives in smoke_test.cpp; this TU just adds
// tests to the same executable.
#include "doctest.h"

#include "math/math.h"

#include <numbers>

using doctest::Approx;
using namespace engine;
using namespace engine::math_literals;

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
