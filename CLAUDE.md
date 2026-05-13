# Architecture

This is a Windows-primary, Vulkan-backed game engine. Three C++ trees: `engine/`, `platform/`, and per-game targets under `games/`. iPad support stays a future option via MoltenVK rather than a parallel Metal backend.

## Scope: Vulkan-only, Windows-primary

Primary target is Windows 10+ on x64. **Direct3D 12, OpenGL, and WebGPU are explicitly out of scope.** Don't design abstractions for backends that won't exist. Engine is free to use Vulkan vocabulary directly (`VkDevice`, `VkBuffer`, `VkPipeline`, etc.) without protocol-wrapping ceremony.

iPad support is on the table eventually via **MoltenVK** — a Vulkan-on-Metal runtime translator, not a separate Metal backend. Concretely: write straight Vulkan; if and when iPad happens, the same SPIR-V and the same VkDevice setup runs through MoltenVK. The cost of keeping this door open is mild: no Windows-only Vulkan extensions in the renderer's hot path, no D3D fallback paths, no `HWND` types in cross-cutting headers. Don't pre-abstract for it — MoltenVK is a runtime, not a second backend.

## Tech baseline

- **C++23.** Same as the legacy `cpp/` tree. The math substrate and game-logic shapes there port over largely intact.
- **Vulkan 1.3** as the device/instance API version. The 1.0→1.3 jump is what shapes the engine (dynamic rendering, `synchronization2`, timeline semaphores). The 1.3→1.4 jump is quality-of-life — push descriptors and vertex input dynamic state moved into core. Promote to 1.4-baseline later when older-GPU exclusion is a zero-cost decision and MoltenVK 1.4 conformance is fully proven; specific 1.4 features can be enabled today on a 1.3 device via their KHR/EXT extensions when present.
- **Slang** for shaders, compiled to SPIR-V at build time via `slangc` (ships in Vulkan SDK 1.3.296+). One `.slang` file per shader module; multiple entry points per file are fine. SPIR-V output sits next to the executable and loads at runtime — embed in binaries later if deployment cleanliness ever matters more than debug iteration.
- **Win32** for the platform layer. No GLFW/SDL3 — a direct `CreateWindowExW` + message pump is small (low hundreds of lines) and matches the substrate-first ethos. The intermediary frameworks save typing but hide enough that "what does the OS actually deliver here" gets fuzzy, which is the opposite of what we want for a learning-and-controlling-the-stack project.
- **Volk** for Vulkan function loading. Loads ICD entry points dynamically instead of linking against `vulkan-1.lib`'s stub layer — both faster (skips the loader trampoline on every call) and easier (no manual `vkGetInstanceProcAddr` dance for extension entry points). Vendored under `engine/third_party/`.
- **VMA** (Vulkan Memory Allocator from AMD GPUOpen) for buffer and image allocation. Hand-rolling memory-type selection and suballocation is hundreds of lines of pure boilerplate without architectural interest. Vendored under `engine/third_party/`.
- **CMake** as the build system. Vulkan-find, MSVC integration, and per-file custom commands (for `slangc` invocation) are mature and don't need replacement.

## Design influences

**Bevy is the primary design reference.** When facing engine-level design decisions (Transform shape, component split, scheduling, hierarchy, asset loading), Bevy's solution is the default starting point, not Unity's or Unreal's. Reason: Bevy's value-type / decomposed-struct ergonomics translate cleanly to C++23's struct + concept model, and its substrate-first philosophy aligns with this codebase's own. Concretely: `Transform` is Bevy-shape (translation + quaternion + scale, quat-canonical), rotation APIs favor multiplicative composition the way `Transform.rotate_y` does, and the renderer's draw API takes simulation-shaped inputs the way Bevy systems read components.

Unity, Unreal, and Godot are useful **cross-references** for catching cases where Bevy's choice doesn't translate — either because Vulkan/Windows patterns argue for something different, or because Bevy's choice is Rust-language-specific and would be awkward in C++. Reference order when in doubt: Bevy first, then the others for sanity checks.

This is a *design heuristic*, not a hard rule. If Bevy and substrate-first ever conflict on a specific decision, substrate-first wins — wait for 2–3 games before extracting workflow on top of substrate, regardless of how Bevy structures its workflow layer.

## Layer split

**`engine/`** — game logic, simulation state, math, input handling, **and rendering**. Includes `<vulkan/vulkan.h>` and VMA freely. The bulk of the engine lives here. Knows nothing about windows, app lifecycle, or platform-specific event loops.

**`platform/`** — the OS-divergence layer. Owns:
- App lifecycle and message pump (`CreateWindowExW`, `PeekMessageW`/`DispatchMessageW`)
- Window creation (`HWND` + `WNDCLASSEXW`) and resize/close plumbing
- Vulkan surface creation (`vkCreateWin32SurfaceKHR`) — the only Vulkan call platform makes; everything else flows through engine
- Input sources that have no portable shape (Raw Input / XInput)

Platform depends on Engine. **Engine never imports Platform.** This direction is non-negotiable. Vulkan code itself stays in engine — `platform/` exists for what Windows does that's *not* Vulkan. When iPad lands, a sibling `platform/ios/` (or similar) sits at the same level, owns `CAMetalLayer` surface creation, and feeds the same Vulkan-talking engine.

Portable input (keyboard codes, gamepad button identity) lives in engine — the OS-specific path that produces those events lives in platform.

## Renderer design

Lower-level *substrate* first; higher-level material/sprite/scene workflow extracted later, on top of the substrate, once 2–3 real games surface the repeated patterns. Don't build the workflow speculatively — verbosity in the first game is the feature, not the bug.

Specific internals (frame sync mechanism, descriptor strategy, pipeline cache shape, command buffer recording model) are intentionally left unspecified here. They get decided in the renderer code as concrete needs surface; pinning them in CLAUDE.md before the first game runs risks freezing the wrong shape.

**Shader source ownership.** Game-authored shaders (per-game fragment shaders, custom material logic) live in the **game target's source tree** (e.g. `games/Pong/shaders/`), compiled to `.spv` at build time via `slangc` custom commands. The engine ships its own *substrate* shaders (fullscreen vertex, 2D primitive vertex) under `engine/shaders/`. The renderer loads both at startup; pipelines mix-and-match a vertex from one with a fragment from the other.

## Testing isolation

Single `engine/` target keeps things simple, but discipline matters: **a file's includes should match its responsibility.** `Math.h` doesn't `#include <vulkan/vulkan.h>`; `Renderer.cpp` does. Test files inherit the same discipline — math tests must not transitively pull in Vulkan types. The engine target *links* Vulkan, but individual files only include what they need.

If render code starts leaking into game-logic or math tests, that's the signal to split engine into `engine_core` (no Vulkan) + `engine_render` (Vulkan). Until then, single target is fine.

Renderer pixel-readback tests need a working Vulkan ICD and compiled shaders; they gate on device availability and skip gracefully on machines without a working driver so the rest of the suite stays runnable in CI and on under-equipped boxes.

## Frame ordering

Per-frame work has a fixed order owned by the engine's per-frame entry point:
1. `renderer.beginFrame(...)` — acquires a swapchain image, begins command-buffer recording, sets up the render target.
2. Game logic / simulation tick (consumes input edges, issues draws against the renderer immediately — encoding is interleaved with simulation, not a separate phase).
3. `renderer.endFrame()` — closes recording, submits, presents.
4. `input.endFrame()` — clears pressed/released edge sets.

Edge sets must be cleared **after** game logic consumed them. Encoding happens *during* `game.update`, not after — the engine just brackets it. The platform host pumps the Win32 message loop and calls `engine.update(...)` per swapchain frame.

## Game integration

Games are **a third tier** on top of platform → engine. A game target implements engine's `Game` interface (`update(GameContext& ctx, float dt)`), constructs an instance, and hands it to platform's host. The engine ticks the game each frame and passes a `GameContext` — an explicit allowlist of engine services (initially `keyboard` and `renderer`; later `audio`, `pointer`, etc.) the game may touch this tick. Games should not hold references to the engine itself.

Object/system registration (ECS-style) is **not** built in. When 2–3 games show repeated entity/system patterns in their top-level `update`, extract a registration layer on top of `Game` — same "substrate first, workflow later" pattern as rendering.

The first game on the new engine is **Pong**, mirroring the legacy `cpp/` Pong effort but on top of Vulkan. Flappy Bird (the original tunnel-runner target before the pivot) is deferred until Pong proves the substrate — its perspective camera, procedural floor shader, and synthwave post-processing surface a different and larger set of substrate needs that shouldn't drive engine-shape decisions until the basics are settled.

## Workflow

**Branches and PRs.** Non-trivial changes go on a feature branch in a git worktree and land via a GitHub PR — never direct to `main`. Worktrees keep `main` clean while work is in flight and let multiple branches build in parallel without stomping each other's CMake build dir. The pattern:

```sh
git worktree add ../20_Games_Engine_worktrees/<branch> -b <branch> main
cd ../20_Games_Engine_worktrees/<branch>
# ... work, commit ...
git push -u origin <branch>
gh pr create
```

Trivial doc edits (a `TASKS.md` checkbox flip, a typo fix) can go direct to `main`. Anything that touches code, the build graph, or the test suite goes through a PR.

**Running builds and tests.** CMake + Ninja on Windows; `cmake --build build && ctest --test-dir build` is the canonical entry. Per-package `test.sh` wrappers land alongside the directories they belong to as the test suites grow. Specifics get filled in with the scaffolding PR.

## Current state (2026-05-12)

`engine/`, `platform/`, and `games/Pong/` don't exist on this branch yet — they're being scaffolded in follow-up commits. The legacy `cpp/` tree (Metal + Objective-C++) remains as transitional reference for the math substrate and game-logic shapes that port over to the new tree; it gets removed once nothing's left to mine from it. `cpp/` itself is not being revived against Vulkan; the new code lives in `engine/`.
