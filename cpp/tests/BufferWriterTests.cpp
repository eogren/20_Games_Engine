#include "doctest/doctest.h"

#include "BufferWriter.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace
{
    // Synthetic POD types with known alignments. Local to the test so
    // BufferWriter's alignment logic is exercised independently of any
    // shader-shared struct (which already has its own layout asserts).
    struct alignas(16) Align16
    {
        uint32_t x, y, z, w;
    };

    struct alignas(8) Align8
    {
        uint64_t v;
    };

    // 256-byte backing region, over-aligned to 64 so even the
    // min_alignment-override case has room to align up.
    using Backing = std::array<std::byte, 256>;
} // namespace

TEST_CASE("BufferWriter: first write returns offset 0 and advances the cursor")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    auto off = w.write(uint32_t{0xDEADBEEF});
    CHECK(off == 0);
    CHECK(w.bytes_used() == sizeof(uint32_t));

    uint32_t readback = 0;
    std::memcpy(&readback, backing.data() + off, sizeof(readback));
    CHECK(readback == 0xDEADBEEF);
}

TEST_CASE("BufferWriter: sequential writes pad up to alignof(T) of the next type")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    // 4-byte write leaves the cursor at offset 4. The next write demands
    // 16-byte alignment, so it should land at offset 16.
    auto a_off = w.write(uint32_t{1});
    CHECK(a_off == 0);

    auto b_off = w.write(Align16{1, 2, 3, 4});
    CHECK(b_off == 16);
    CHECK(b_off % alignof(Align16) == 0);
    CHECK(w.bytes_used() == 16 + sizeof(Align16));
}

TEST_CASE("BufferWriter: a smaller-alignment write after a larger one packs in immediately")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    auto a_off = w.write(Align16{1, 2, 3, 4});
    CHECK(a_off == 0);

    // Right after the 16-byte block, a uint32 sits at offset 16 — no
    // artificial padding for "binding offset".
    auto b_off = w.write(uint32_t{5});
    CHECK(b_off == sizeof(Align16));
}

TEST_CASE("BufferWriter: pad_to aligns the cursor without writing")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    w.write(uint32_t{1});
    CHECK(w.bytes_used() == 4);

    w.pad_to(32);
    CHECK(w.bytes_used() == 32);

    auto off = w.write(uint32_t{2});
    CHECK(off == 32);
}

TEST_CASE("BufferWriter: reset rewinds the cursor and lets writes overwrite in place")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    w.write(uint32_t{0xAAAAAAAA});
    CHECK(w.bytes_used() == 4);

    w.reset();
    CHECK(w.bytes_used() == 0);

    auto off = w.write(uint32_t{0xBBBBBBBB});
    CHECK(off == 0);

    uint32_t readback = 0;
    std::memcpy(&readback, backing.data(), sizeof(readback));
    CHECK(readback == 0xBBBBBBBB);
}

TEST_CASE("BufferWriter: span write copies the full range and offsets the first element")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    std::array<uint32_t, 4> values{10, 20, 30, 40};
    auto off = w.write(std::span<const uint32_t>(values));
    CHECK(off == 0);
    CHECK(w.bytes_used() == sizeof(values));

    for (size_t i = 0; i < values.size(); ++i)
    {
        uint32_t v = 0;
        std::memcpy(&v, backing.data() + off + i * sizeof(uint32_t), sizeof(v));
        CHECK(v == values[i]);
    }
}

TEST_CASE("BufferWriter: reserve returns an aligned pointer and sets out_offset")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    w.write(uint32_t{1});

    size_t off = 0;
    auto* ptr = w.reserve<Align16>(2, &off);
    CHECK(off == 16);
    CHECK(reinterpret_cast<uintptr_t>(ptr) % alignof(Align16) == 0);
    CHECK(w.bytes_used() == 16 + 2 * sizeof(Align16));

    // In-place writes through the returned pointer must be visible in the
    // backing region at the reported offset.
    ptr[0] = {1, 2, 3, 4};
    ptr[1] = {5, 6, 7, 8};

    Align16 readback{};
    std::memcpy(&readback, backing.data() + off, sizeof(readback));
    CHECK(readback.x == 1);
    CHECK(readback.w == 4);

    std::memcpy(&readback, backing.data() + off + sizeof(Align16), sizeof(readback));
    CHECK(readback.x == 5);
    CHECK(readback.w == 8);
}

TEST_CASE("BufferWriter: min_alignment override pads past alignof(T)")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    w.write(uint32_t{1});

    // alignof(uint32_t) == 4, but we demand a 32-byte-aligned offset
    // (e.g. argument-buffer binding rule).
    auto off = w.write(uint32_t{2}, /*min_alignment=*/32);
    CHECK(off == 32);
}

TEST_CASE("BufferWriter: min_alignment smaller than alignof(T) is clamped up")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    w.write(uint32_t{1});

    // The explicit 4 is overridden by alignof(Align16) == 16.
    auto off = w.write(Align16{}, /*min_alignment=*/4);
    CHECK(off == 16);
}

TEST_CASE("BufferWriter: mixed alignment progression matches expected offsets")
{
    alignas(64) Backing backing{};
    BufferWriter w(backing.data(), backing.size());

    // Realistic per-frame layout: a 16-aligned uniforms block followed
    // by a packed array of 8-aligned per-instance records.
    auto u_off = w.write(Align16{1, 2, 3, 4});
    CHECK(u_off == 0);

    std::array<Align8, 3> instances{Align8{100}, Align8{200}, Align8{300}};
    auto p_off = w.write(std::span<const Align8>(instances));
    CHECK(p_off == sizeof(Align16));
    CHECK(p_off % alignof(Align8) == 0);
    CHECK(w.bytes_used() == sizeof(Align16) + sizeof(instances));
}
