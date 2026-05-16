#include "input/input.h"

bool engine::KeyboardInput::justPressed(platform::KeyCode key) const
{
    return keys_just_pressed.test(static_cast<size_t>(key));
}

bool engine::KeyboardInput::pressed(platform::KeyCode key) const
{
    return keys_pressed.test(static_cast<size_t>(key));
}

bool engine::KeyboardInput::justReleased(platform::KeyCode key) const
{
    return keys_just_released.test(static_cast<size_t>(key));
}

void engine::KeyboardInput::update(std::bitset<kKeyCount> down_keys)
{
    // Order matters: edges read the *previous* keys_pressed, so assign edges
    // first and overwrite keys_pressed last.
    keys_just_pressed = down_keys & ~keys_pressed;
    keys_just_released = keys_pressed & ~down_keys;
    keys_pressed = down_keys;
}
