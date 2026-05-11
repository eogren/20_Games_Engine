#include "doctest/doctest.h"

#include "Primitive2D.h"

// pack_rgba's bit layout is the contract with Metal's unpack_unorm4x8_to_float:
// low byte = R, then G, B, A in ascending bytes. Wrong order would silently
// render plausible-but-wrong colors, so the cases below pin each channel to
// its byte position.

TEST_CASE("pack_rgba: R lives in the low byte")
{
    CHECK(pack_rgba(1.0f, 0.0f, 0.0f, 0.0f) == 0x000000FFu);
}

TEST_CASE("pack_rgba: G lives in the second byte")
{
    CHECK(pack_rgba(0.0f, 1.0f, 0.0f, 0.0f) == 0x0000FF00u);
}

TEST_CASE("pack_rgba: B lives in the third byte")
{
    CHECK(pack_rgba(0.0f, 0.0f, 1.0f, 0.0f) == 0x00FF0000u);
}

TEST_CASE("pack_rgba: A lives in the high byte")
{
    CHECK(pack_rgba(0.0f, 0.0f, 0.0f, 1.0f) == 0xFF000000u);
}

TEST_CASE("pack_rgba: all-zero RGBA packs to 0")
{
    CHECK(pack_rgba(0.0f, 0.0f, 0.0f, 0.0f) == 0x00000000u);
}

TEST_CASE("pack_rgba: all-one RGBA packs to 0xFFFFFFFF (1.0 must map to 255, not wrap)")
{
    CHECK(pack_rgba(1.0f, 1.0f, 1.0f, 1.0f) == 0xFFFFFFFFu);
}

TEST_CASE("pack_rgba: out-of-range channels clamp to [0, 1] per channel independently")
{
    // R = -1 → 0, G = 2 → 255, B = -100 → 0, A = 100 → 255.
    CHECK(pack_rgba(-1.0f, 2.0f, -100.0f, 100.0f) == 0xFF00FF00u);
}

TEST_CASE("pack_rgba: 0.5 rounds to 128 (round-half-away-from-zero, not truncation)")
{
    // 0.5 * 255 = 127.5; std::lround → 128. The naïve "+0.5 then cast" trick
    // would give 127 here — that's the bug this test pins down.
    CHECK((pack_rgba(0.5f, 0.0f, 0.0f, 0.0f) & 0xFFu) == 128u);
}

TEST_CASE("pack_rgba: values just below 1.0 round up to 255")
{
    // 0.999 * 255 = 254.745 → 255.
    CHECK((pack_rgba(0.999f, 0.0f, 0.0f, 0.0f) & 0xFFu) == 255u);
}

TEST_CASE("pack_rgba: 4-arg overload defaults alpha to 1.0")
{
    CHECK(pack_rgba(1.0f, 0.0f, 0.0f) == 0xFF0000FFu);
}

TEST_CASE("pack_rgba: simd::float4 and 4-arg overloads agree")
{
    CHECK(pack_rgba(simd::float4{0.25f, 0.5f, 0.75f, 1.0f}) == pack_rgba(0.25f, 0.5f, 0.75f, 1.0f));
}
