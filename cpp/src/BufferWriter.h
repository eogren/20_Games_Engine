#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <type_traits>

// Bump-pointer writer for packing typed payloads into a contiguous byte
// region. Each `write` aligns the cursor to (at least) alignof(T) before
// copying, so the returned offset is a valid binding offset for that
// type — pair it with `buffer->gpuAddress() + offset` when calling
// MTL4::ArgumentTable::setAddress.
//
// Intentionally has no Metal dependency: it operates on a raw
// (base, capacity) pair so the header stays compatible with the
// no-Metal test target and the alignment logic is unit-testable
// against a std::array.
//
// Per-frame use against an MTLBuffer:
//   BufferWriter w(buf->contents(), buf->length());
//   auto u_off = w.write(uniforms);
//   auto p_off = w.write(std::span<const Primitive2D>(prims));
//   table->setAddress(buf->gpuAddress() + u_off, UNIFORMS_SLOT);
//   table->setAddress(buf->gpuAddress() + p_off, PRIMITIVES_SLOT);
//
// Requires CPU-visible storage when paired with an MTLBuffer
// (StorageModeShared on Apple Silicon). `contents()` returns null for
// private-storage buffers; the constructor asserts on a null base.
class BufferWriter
{
public:
    BufferWriter(void* base, size_t capacity) : base_(static_cast<std::byte*>(base)), capacity_(capacity), offset_(0)
    {
        assert(base_ && "BufferWriter requires a non-null base pointer");
    }

    // Aligns the cursor to max(alignof(T), min_alignment), memcpys `value`
    // there, advances past it, and returns the offset the value landed at.
    // `min_alignment` covers the rare case where the binding context needs
    // more than T's natural alignment (e.g. an argument buffer with its
    // own offset rule); leave it at the default otherwise.
    template <class T> size_t write(const T& value, size_t min_alignment = alignof(T))
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "BufferWriter payload must be trivially copyable — it's memcpy'd into a GPU buffer");
        size_t off = reserve_bytes(sizeof(T), std::max(min_alignment, alignof(T)));
        std::memcpy(base_ + off, &value, sizeof(T));
        return off;
    }

    // Same shape for a contiguous range. The returned offset addresses
    // the first element; element i sits at `offset + i * sizeof(T)`,
    // which matches the stride a shader sees on `device T*`.
    template <class T> size_t write(std::span<const T> values, size_t min_alignment = alignof(T))
    {
        static_assert(std::is_trivially_copyable_v<T>,
                      "BufferWriter payload must be trivially copyable — it's memcpy'd into a GPU buffer");
        size_t off = reserve_bytes(values.size_bytes(), std::max(min_alignment, alignof(T)));
        std::memcpy(base_ + off, values.data(), values.size_bytes());
        return off;
    }

    // Reserve uninitialized storage for `count` Ts; the caller fills the
    // returned pointer in place. `*out_offset` receives the binding offset.
    // Use when payloads are produced streaming-style instead of from a
    // pre-built span.
    template <class T> T* reserve(size_t count, size_t* out_offset, size_t min_alignment = alignof(T))
    {
        static_assert(std::is_trivially_copyable_v<T>, "BufferWriter payload must be trivially copyable");
        size_t off = reserve_bytes(sizeof(T) * count, std::max(min_alignment, alignof(T)));
        if (out_offset)
        {
            *out_offset = off;
        }
        return reinterpret_cast<T*>(base_ + off);
    }

    // Pad the cursor up to a multiple of `alignment` without writing
    // anything. Routine writes auto-align; use this only when an explicit
    // gap is wanted (e.g. emitting a fixed-layout region with reserved
    // tail-padding before the next block).
    void pad_to(size_t alignment)
    {
        offset_ = align_up(offset_, alignment);
    }

    // Rewind the cursor so the same buffer can be refilled next frame.
    // The buffer contents are not zeroed — fresh writes overwrite in place.
    void reset()
    {
        offset_ = 0;
    }

    size_t bytes_used() const
    {
        return offset_;
    }
    size_t capacity() const
    {
        return capacity_;
    }

private:
    size_t reserve_bytes(size_t bytes, size_t alignment)
    {
        size_t off = align_up(offset_, alignment);
        assert(off + bytes <= capacity_ && "BufferWriter would overrun the buffer");
        offset_ = off + bytes;
        return off;
    }

    static size_t align_up(size_t value, size_t alignment)
    {
        assert(std::has_single_bit(alignment) && "alignment must be a power of two");
        return (value + alignment - 1) & ~(alignment - 1);
    }

    std::byte* base_;
    size_t capacity_;
    size_t offset_;
};
