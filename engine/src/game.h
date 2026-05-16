#pragma once

#include "input/input.h"
#include "renderer/renderer.h"

namespace engine
{
    /**
     * Per-tick allowlist of engine services a game may touch this frame.
     *
     * Constructed by Engine immediately before each Game::update call and
     * passed by reference. The game must not retain pointers or references
     * into this object past the call — the next tick gets a fresh ctx whose
     * embedded references may point at different storage.
     *
     * Starts narrow (keyboard + renderer); audio, pointer, time, etc. will
     * land here as those subsystems come online. See CLAUDE.md > "Game
     * integration".
     */
    struct GameContext
    {
        const KeyboardInput& keyboard;
        renderer::Renderer& renderer;
    };

    /**
     * Abstract base for a game's per-frame logic. The host (games/<Name>/
     * main.cpp) constructs a concrete subclass on the stack and hands it to
     * Engine::run(); the engine ticks update() once per frame, between
     * renderer beginFrame and endFrame.
     *
     * `dt` is seconds since the last update; 0.0 on the first call.
     */
    class Game
    {
    public:
        virtual ~Game() = default;

        Game() = default;
        Game(const Game&) = delete;
        Game& operator=(const Game&) = delete;
        Game(Game&&) = delete;
        Game& operator=(Game&&) = delete;

        virtual void update(GameContext& ctx, float dt) = 0;
    };
} // namespace engine
