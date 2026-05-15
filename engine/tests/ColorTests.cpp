// doctest's IMPLEMENT_WITH_MAIN lives in smoke_test.cpp; this TU just adds
// tests to the same executable.
#include "doctest.h"

#include "math/Color.h"

using doctest::Approx;
using engine::Color;

TEST_CASE("Color: rgb() exposes components verbatim")
{
    const auto c = Color::rgb(0.1f, 0.2f, 0.3f, 0.4f);
    CHECK(c.r() == Approx(0.1f));
    CHECK(c.g() == Approx(0.2f));
    CHECK(c.b() == Approx(0.3f));
    CHECK(c.a() == Approx(0.4f));
}

TEST_CASE("Color: rgb() defaults alpha to 1.0")
{
    CHECK(Color::rgb(0.5f, 0.5f, 0.5f).a() == Approx(1.0f));
}

TEST_CASE("Color: black() and white() constants")
{
    CHECK(Color::black().r() == 0.0f);
    CHECK(Color::black().g() == 0.0f);
    CHECK(Color::black().b() == 0.0f);
    CHECK(Color::black().a() == 1.0f);

    CHECK(Color::white().r() == 1.0f);
    CHECK(Color::white().g() == 1.0f);
    CHECK(Color::white().b() == 1.0f);
    CHECK(Color::white().a() == 1.0f);
}

TEST_CASE("Color: floats() returns RGBA in order")
{
    const auto rgba = Color::rgb(0.1f, 0.2f, 0.3f, 0.4f).floats();
    CHECK(rgba[0] == Approx(0.1f));
    CHECK(rgba[1] == Approx(0.2f));
    CHECK(rgba[2] == Approx(0.3f));
    CHECK(rgba[3] == Approx(0.4f));
}

TEST_CASE("Color: hsv() hits the six primary/secondary hues")
{
    // h=0      red, h=1/6  yellow, h=2/6  green
    // h=3/6 cyan, h=4/6  blue,   h=5/6 magenta
    const float oneSixth = 1.0f / 6.0f;
    auto eq = [](Color c, float r, float g, float b)
    {
        CHECK(c.r() == Approx(r));
        CHECK(c.g() == Approx(g));
        CHECK(c.b() == Approx(b));
    };
    eq(Color::hsv(0.0f * oneSixth, 1.0f, 1.0f), 1.0f, 0.0f, 0.0f);
    eq(Color::hsv(1.0f * oneSixth, 1.0f, 1.0f), 1.0f, 1.0f, 0.0f);
    eq(Color::hsv(2.0f * oneSixth, 1.0f, 1.0f), 0.0f, 1.0f, 0.0f);
    eq(Color::hsv(3.0f * oneSixth, 1.0f, 1.0f), 0.0f, 1.0f, 1.0f);
    eq(Color::hsv(4.0f * oneSixth, 1.0f, 1.0f), 0.0f, 0.0f, 1.0f);
    eq(Color::hsv(5.0f * oneSixth, 1.0f, 1.0f), 1.0f, 0.0f, 1.0f);
}

TEST_CASE("Color: hsv() at v=0 is black regardless of h, s")
{
    const auto c = Color::hsv(0.42f, 0.7f, 0.0f);
    CHECK(c.r() == Approx(0.0f));
    CHECK(c.g() == Approx(0.0f));
    CHECK(c.b() == Approx(0.0f));
}

TEST_CASE("Color: hsv() at s=0 is grey at intensity v")
{
    const auto c = Color::hsv(0.42f, 0.0f, 0.6f);
    CHECK(c.r() == Approx(0.6f));
    CHECK(c.g() == Approx(0.6f));
    CHECK(c.b() == Approx(0.6f));
}

TEST_CASE("Color: hue wraps modulo 1.0")
{
    const auto red0 = Color::hsv(0.0f, 1.0f, 1.0f);
    const auto red1 = Color::hsv(1.0f, 1.0f, 1.0f);
    const auto red2 = Color::hsv(2.0f, 1.0f, 1.0f);
    const auto redNeg = Color::hsv(-1.0f, 1.0f, 1.0f);
    CHECK(red1.r() == Approx(red0.r()));
    CHECK(red1.g() == Approx(red0.g()));
    CHECK(red1.b() == Approx(red0.b()));
    CHECK(red2.r() == Approx(red0.r()));
    CHECK(red2.g() == Approx(red0.g()));
    CHECK(red2.b() == Approx(red0.b()));
    CHECK(redNeg.r() == Approx(red0.r()));
    CHECK(redNeg.g() == Approx(red0.g()));
    CHECK(redNeg.b() == Approx(red0.b()));
}

TEST_CASE("Color: RGB→HSV recovers value()")
{
    CHECK(Color::rgb(1.0f, 0.0f, 0.0f).value() == Approx(1.0f));
    CHECK(Color::rgb(0.5f, 0.3f, 0.1f).value() == Approx(0.5f));
    CHECK(Color::rgb(0.0f, 0.0f, 0.0f).value() == Approx(0.0f));
}

TEST_CASE("Color: RGB→HSV recovers saturation()")
{
    CHECK(Color::rgb(1.0f, 0.0f, 0.0f).saturation() == Approx(1.0f));
    CHECK(Color::rgb(0.5f, 0.5f, 0.5f).saturation() == Approx(0.0f));
    // half-saturated red: max=1.0, min=0.5, sat = (max-min)/max = 0.5
    CHECK(Color::rgb(1.0f, 0.5f, 0.5f).saturation() == Approx(0.5f));
}

TEST_CASE("Color: grey collapses hue to 0")
{
    CHECK(Color::rgb(0.5f, 0.5f, 0.5f).hue() == Approx(0.0f));
    CHECK(Color::rgb(0.0f, 0.0f, 0.0f).hue() == Approx(0.0f));
    CHECK(Color::rgb(1.0f, 1.0f, 1.0f).hue() == Approx(0.0f));
}

TEST_CASE("Color: HSV roundtrip is exact at canonical points")
{
    auto roundtrip = [](float h, float s, float v)
    {
        const auto c = Color::hsv(h, s, v);
        CHECK(c.hue() == Approx(h));
        CHECK(c.saturation() == Approx(s));
        CHECK(c.value() == Approx(v));
    };
    roundtrip(0.0f, 1.0f, 1.0f);        // red
    roundtrip(1.0f / 3.0f, 1.0f, 1.0f); // green
    roundtrip(2.0f / 3.0f, 1.0f, 1.0f); // blue
    roundtrip(0.5f, 0.7f, 0.4f);
}

TEST_CASE("Color: with_value preserves hue and saturation")
{
    const auto original = Color::hsv(0.3f, 0.6f, 0.4f);
    const auto brighter = original.with_value(0.9f);
    CHECK(brighter.hue() == Approx(0.3f));
    CHECK(brighter.saturation() == Approx(0.6f));
    CHECK(brighter.value() == Approx(0.9f));
}

TEST_CASE("Color: with_saturation preserves hue and value")
{
    const auto original = Color::hsv(0.3f, 0.6f, 0.4f);
    const auto desaturated = original.with_saturation(0.1f);
    CHECK(desaturated.hue() == Approx(0.3f));
    CHECK(desaturated.saturation() == Approx(0.1f));
    CHECK(desaturated.value() == Approx(0.4f));
}

TEST_CASE("Color: with_hue preserves saturation and value")
{
    const auto original = Color::hsv(0.3f, 0.6f, 0.4f);
    const auto shifted = original.with_hue(0.7f);
    CHECK(shifted.hue() == Approx(0.7f));
    CHECK(shifted.saturation() == Approx(0.6f));
    CHECK(shifted.value() == Approx(0.4f));
}

TEST_CASE("Color: with_alpha leaves RGB intact")
{
    const auto c = Color::rgb(0.1f, 0.2f, 0.3f, 1.0f).with_alpha(0.5f);
    CHECK(c.r() == Approx(0.1f));
    CHECK(c.g() == Approx(0.2f));
    CHECK(c.b() == Approx(0.3f));
    CHECK(c.a() == Approx(0.5f));
}

TEST_CASE("Color: hsv() carries alpha through")
{
    CHECK(Color::hsv(0.3f, 0.5f, 0.7f, 0.42f).a() == Approx(0.42f));
}
