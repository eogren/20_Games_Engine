#pragma once

#include "input/input.h"
#include "renderer/renderer.h"

namespace engine
{
    /**
     * Services available during a fixed simulation step. Deliberately omits
     * the renderer — draw calls are not valid outside beginFrame/endFrame.
     */
    struct FixedUpdateContext
    {
        const KeyboardInput& keyboard;
    };

    /**
     * Per-frame allowlist of engine services a game may touch during the
     * render pass. Constructed by Engine immediately before each Game::update
     * call and passed by reference. The game must not retain pointers or
     * references into this object past the call.
     *
     * Starts narrow (keyboard + renderer); audio, pointer, etc. will land
     * here as those subsystems come online. See CLAUDE.md > "Game integration".
     *
     * `alpha` is accumulator / fixedDt in [0, 1) — the fraction of a fixed
     * step elapsed since the last simulation tick. Lerp between the previous
     * and current simulation state by this factor when positioning objects.
     */
    struct GameContext
    {
        const KeyboardInput& keyboard;
        renderer::Renderer& renderer;
        float alpha;
    };

    /**
     * Abstract base for a game's logic. The host (games/<Name>/main.cpp)
     * constructs a concrete subclass and hands it to Engine::run().
     *
     * fixedUpdate runs 0–N times per frame at a fixed dt (60 Hz) before the
     * render pass — use it for simulation, collision, and integration.
     * update runs once per frame inside beginFrame/endFrame — use it for
     * draw calls and renderer configuration.
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

        virtual void fixedUpdate(FixedUpdateContext&, float) {}
        virtual void update(GameContext& ctx, float dt) = 0;
    };
} // namespace engine
