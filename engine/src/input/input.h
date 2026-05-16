#pragma once

#include <bitset>

#include "platform/input.h"

namespace engine
{
    inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(platform::KeyCode::KeyCount);

    struct KeyboardInput
    {
        /**
         * Keys that transitioned to pressed this frame (edge).
         */
        std::bitset<kKeyCount> keys_just_pressed;

        /**
         * Keys currently held down (level).
         */
        std::bitset<kKeyCount> keys_pressed;

        /**
         * Keys that transitioned to released this frame (edge).
         */
        std::bitset<kKeyCount> keys_just_released;

        bool justPressed(platform::KeyCode key) const;
        bool pressed(platform::KeyCode key) const;
        bool justReleased(platform::KeyCode key) const;

        /**
         * Fold a fresh snapshot of currently-held keys (from the platform) into
         * the engine's keyboard state, recomputing the edge sets:
         *   just_pressed  = down_keys & ~prev_pressed
         *   just_released = prev_pressed & ~down_keys
         *   pressed       = down_keys
         * Call once per tick, before game logic reads the state.
         */
        void update(std::bitset<kKeyCount> down_keys);
    };

    /**
     * Tracks input state per-frame. Right now we only capture keyboard but
     * this will likely expand to mouse soon.
     */
    struct InputState
    {
        KeyboardInput keyboard;
    };
} // namespace engine
