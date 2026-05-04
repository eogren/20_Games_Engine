# Architecture

This is an Apple-only game engine. Two Swift packages: `Engine/` and `Platform/`.

## Scope: Metal-only, Apple-only

Targets macOS 14+ and iPadOS 17+, both Metal-backed. **Vulkan / DX12 / WebGPU are explicitly out of scope.** Don't design abstractions for backends that won't exist. Engine is free to use Metal vocabulary directly (`MTLDevice`, `MTLBuffer`, `MTLRenderPipelineState`, etc.) without protocol-wrapping ceremony.

## Layer split

**`Engine/`** — game logic, simulation state, math, input handling, **and rendering**. Imports system frameworks freely (Metal, GameController, QuartzCore, Foundation). The bulk of the engine lives here. Knows nothing about windows, app lifecycle, or platform-specific event loops.

**`Platform/`** — the OS-divergence layer. Owns:
- App lifecycle (`NSApplication.run()` on macOS, SwiftUI `App` / `UIApplicationMain` on iOS)
- Window / view hosting (`NSWindow` + `NSView` with `CAMetalLayer` vs `UIWindow` + `UIView`)
- Display-link plumbing (attached to a platform-specific view)
- Input sources that diverge between OSes (touch via `UIResponder`, mouse via `NSResponder`)

Platform depends on Engine. **Engine never imports Platform.** This direction is non-negotiable.

Cross-Apple input (keyboard via `GameController`, gamepad, GCMouse) lives in Engine because there's no OS divergence to handle.

## Renderer design

Lower-level *substrate* first; higher-level material/sprite/scene workflow extracted later, on top of the substrate, once 2–3 real games surface the repeated patterns. Don't build the workflow speculatively — verbosity in the first game is the feature, not the bug.

Shader sources (`.metal`) live in the **game/app target's bundle**, not in Engine or Platform. Compiled to `.metallib` at build time, loaded via `device.makeDefaultLibrary()`. Engine references shader functions by name.

## Testing isolation

Single Engine target keeps things simple, but discipline matters: **a file's imports should match its responsibility.** `Math.swift` doesn't import Metal; `Renderer.swift` does. Test files inherit the same discipline — math tests must not transitively pull in GPU types. The Engine module *links* Metal, but individual files only import what they need.

If render code starts leaking into game-logic or math tests, that's the signal to split Engine into `EngineCore` (no Metal) + `EngineRender` (Metal). Until then, single target is fine.

## Frame ordering

Per-frame work has a fixed order owned by `GameEngine.update(dt:)`:
1. Game logic / simulation tick (consumes input edges)
2. Render
3. `keyboard.endFrame()` — clear pressed/released edge sets

Edge sets must be cleared **after** game logic consumed them. The platform host calls `engine.update(dt:)` once per display refresh and nothing more.

## Game integration

Games are **a third package** on top of Platform → Engine. A game target implements Engine's `Game` protocol (`update(_ ctx: GameContext, dt: Float)`), constructs an instance, and hands it to `Host(game:)`. The engine ticks the game each frame and passes a `GameContext` — an explicit allowlist of engine services (currently `keyboard`; later `renderer`, `audio`, etc.) that the game may touch this tick. Games should not hold references to `GameEngine` itself.

Object/system registration (ECS-style) is **not** built in. When 2–3 games show repeated entity/system patterns in their top-level `update`, extract a registration layer on top of `Game` — same "substrate first, workflow later" pattern as rendering.
