// doctest's IMPLEMENT_WITH_MAIN lives in smoke_test.cpp; this TU just adds
// tests to the same executable.
#include "doctest.h"

#include "input/input.h"

#include <bitset>
#include <cstddef>

using engine::InputState;
using engine::KeyboardInput;
using engine::kKeyCount;
using platform::KeyCode;

namespace
{
    std::bitset<kKeyCount> only(KeyCode key)
    {
        std::bitset<kKeyCount> b;
        b.set(static_cast<std::size_t>(key));
        return b;
    }

    std::bitset<kKeyCount> both(KeyCode a, KeyCode b)
    {
        std::bitset<kKeyCount> r;
        r.set(static_cast<std::size_t>(a));
        r.set(static_cast<std::size_t>(b));
        return r;
    }
} // namespace

TEST_CASE("KeyboardInput: default state reports no input on any key")
{
    const KeyboardInput k;
    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK_FALSE(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyW));
    CHECK_FALSE(k.justPressed(KeyCode::KeyS));
    CHECK_FALSE(k.pressed(KeyCode::KeyS));
    CHECK_FALSE(k.justReleased(KeyCode::KeyS));
}

TEST_CASE("KeyboardInput: each query method reads its own backing bitset")
{
    KeyboardInput k;
    k.keys_pressed.set(static_cast<std::size_t>(KeyCode::KeyW));
    k.keys_just_pressed.set(static_cast<std::size_t>(KeyCode::KeyS));
    k.keys_just_released.set(static_cast<std::size_t>(KeyCode::KeyW));

    CHECK(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.pressed(KeyCode::KeyS));

    CHECK(k.justPressed(KeyCode::KeyS));
    CHECK_FALSE(k.justPressed(KeyCode::KeyW));

    CHECK(k.justReleased(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyS));
}

TEST_CASE("InputState: default-constructs an empty keyboard")
{
    const InputState state;
    CHECK_FALSE(state.keyboard.pressed(KeyCode::KeyW));
    CHECK_FALSE(state.keyboard.justPressed(KeyCode::KeyW));
    CHECK_FALSE(state.keyboard.justReleased(KeyCode::KeyW));
}

TEST_CASE("KeyboardInput::update: first press flags just_pressed and pressed")
{
    KeyboardInput k;
    k.update(only(KeyCode::KeyW));

    CHECK(k.justPressed(KeyCode::KeyW));
    CHECK(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyW));
}

TEST_CASE("KeyboardInput::update: continuously held key drops just_pressed on next tick")
{
    KeyboardInput k;
    k.update(only(KeyCode::KeyW));
    k.update(only(KeyCode::KeyW));

    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyW));
}

TEST_CASE("KeyboardInput::update: release flags just_released and clears pressed")
{
    KeyboardInput k;
    k.update(only(KeyCode::KeyW));
    k.update({});

    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK_FALSE(k.pressed(KeyCode::KeyW));
    CHECK(k.justReleased(KeyCode::KeyW));
}

TEST_CASE("KeyboardInput::update: just_released lives exactly one tick")
{
    KeyboardInput k;
    k.update(only(KeyCode::KeyW));
    k.update({});
    k.update({});

    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK_FALSE(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyW));
}

TEST_CASE("KeyboardInput::update: edge sets isolate per-key transitions in one tick")
{
    KeyboardInput k;
    // Prior frame: KeyW held.
    k.update(only(KeyCode::KeyW));
    REQUIRE(k.pressed(KeyCode::KeyW));

    // This tick: KeyW continues held, KeyS becomes pressed.
    k.update(both(KeyCode::KeyW, KeyCode::KeyS));

    CHECK(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK_FALSE(k.justReleased(KeyCode::KeyW));

    CHECK(k.pressed(KeyCode::KeyS));
    CHECK(k.justPressed(KeyCode::KeyS));
    CHECK_FALSE(k.justReleased(KeyCode::KeyS));

    // Next tick: KeyW released, KeyS still held.
    k.update(only(KeyCode::KeyS));

    CHECK_FALSE(k.pressed(KeyCode::KeyW));
    CHECK_FALSE(k.justPressed(KeyCode::KeyW));
    CHECK(k.justReleased(KeyCode::KeyW));

    CHECK(k.pressed(KeyCode::KeyS));
    CHECK_FALSE(k.justPressed(KeyCode::KeyS));
    CHECK_FALSE(k.justReleased(KeyCode::KeyS));
}

TEST_CASE("KeyboardInput::update: simultaneous press and release in a single tick")
{
    KeyboardInput k;
    k.update(only(KeyCode::KeyW));

    // KeyW released, KeyS pressed — both edges on the same tick.
    k.update(only(KeyCode::KeyS));

    CHECK_FALSE(k.pressed(KeyCode::KeyW));
    CHECK(k.justReleased(KeyCode::KeyW));
    CHECK_FALSE(k.justPressed(KeyCode::KeyW));

    CHECK(k.pressed(KeyCode::KeyS));
    CHECK(k.justPressed(KeyCode::KeyS));
    CHECK_FALSE(k.justReleased(KeyCode::KeyS));
}
