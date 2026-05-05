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

**Shader source ownership.** Game-authored shaders (per-game fragment shaders, custom material logic) live in the **game/app target's bundle**, compiled to `.metallib` at build time and loaded via `device.makeDefaultLibrary(bundle: .main)`. The Engine package may ship its own *substrate* shaders (e.g., the fullscreen vertex shader used by `drawFullscreenQuad`) as `.metal` files under `Sources/Engine/Shaders/`, declared as a `.process(...)` resource in `Package.swift` — these compile into `Engine_Engine.bundle/default.metallib` and load via `device.makeDefaultLibrary(bundle: .module)`. The renderer holds both libraries; PSOs link a vertex function from one with a fragment function from the other.

**Build-system note.** The SwiftPM CLI (`swift build` / `swift test`) does NOT invoke the Metal compiler. Any code path that loads `Bundle.module`'s metallib only works under Xcode-driven builds (the FlappyBird app, or `xcodebuild test` against the Engine package). Renderer tests gate on metallib presence so `swift test` stays green for non-renderer code.

## Testing isolation

Single Engine target keeps things simple, but discipline matters: **a file's imports should match its responsibility.** `Math.swift` doesn't import Metal; `Renderer.swift` does. Test files inherit the same discipline — math tests must not transitively pull in GPU types. The Engine module *links* Metal, but individual files only import what they need.

If render code starts leaking into game-logic or math tests, that's the signal to split Engine into `EngineCore` (no Metal) + `EngineRender` (Metal). Until then, single target is fine.

## Frame ordering

Per-frame work has a fixed order owned by `GameEngine.update(dt:drawable:passDescriptor:)`:
1. `renderer.beginFrame(...)` — opens a render encoder.
2. Game logic / simulation tick (consumes input edges, issues draws against `ctx.renderer` immediately — encoding is interleaved with simulation, not a separate phase).
3. `renderer.endFrame()` — closes the encoder, presents (if a drawable was bound), commits the command buffer.
4. `keyboard.endFrame()` — clears pressed/released edge sets.

Edge sets must be cleared **after** game logic consumed them. Encoding happens *during* `game.update`, not after — the engine just brackets it. The platform host fetches a drawable + builds a clear-color pass descriptor each tick and calls `engine.update(...)` once per display refresh.

## Game integration

Games are **a third package** on top of Platform → Engine. A game target implements Engine's `Game` protocol (`update(_ ctx: GameContext, dt: Float)`), constructs an instance, and hands it to `Host(game:)`. The engine ticks the game each frame and passes a `GameContext` — an explicit allowlist of engine services (currently `keyboard` and `renderer`; later `audio`, etc.) that the game may touch this tick. Games should not hold references to `GameEngine` itself.

Object/system registration (ECS-style) is **not** built in. When 2–3 games show repeated entity/system patterns in their top-level `update`, extract a registration layer on top of `Game` — same "substrate first, workflow later" pattern as rendering.

## Workflow

**Branches and PRs.** Non-trivial changes go on a feature branch in a git worktree and land via a GitHub PR — never direct to `main`. Worktrees keep `main` clean while work is in flight and let multiple branches build in parallel without stomping each other's `DerivedData`. The pattern:

```sh
git worktree add ../20_Games_Engine_worktrees/<branch> -b <branch> main
cd ../20_Games_Engine_worktrees/<branch>
# ... work, commit ...
git push -u origin <branch>
gh pr create
```

Trivial doc edits (a `TASKS.md` checkbox flip, a typo fix) can go direct to `main`. Anything that touches code, the build graph, or the test suite goes through a PR.

**Running tests.** `./test.sh` at repo root runs the Engine package's test suite via `xcodebuild test` (the SwiftPM CLI doesn't invoke the Metal compiler, so renderer pixel-readback tests skip under `swift test` but run under `xcodebuild`). Pass-through args go to xcodebuild — e.g., `./test.sh -only-testing:EngineTests/RendererSmokeTests`.

**Test-only shaders** live in `Engine/Tests/EngineTests/Shaders/` and are wired into the test bundle via `resources: [.process("Shaders")]` on the test target. Tests load them with `device.makeDefaultLibrary(bundle: .module)` (the test bundle) and hand the library to `Renderer(device:gameLibrary:)`.
