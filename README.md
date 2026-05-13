# 20 Games Engine

A Windows-primary, Vulkan-backed game engine, built alongside the games
it runs. The engine grows from concrete game needs rather than
speculative abstractions (see [`CLAUDE.md`](CLAUDE.md)).

Primary target is Windows 10+ on x64. iPad support is a future option
via MoltenVK — not a parallel Metal backend. Direct3D 12, OpenGL, and
WebGPU are explicitly out of scope.

## Stack

- **C++23** + **CMake** + **Ninja**
- **Vulkan 1.3** (dynamic rendering, `synchronization2`, timeline semaphores)
- **Slang** → SPIR-V, compiled at build time via `slangc` from the Vulkan SDK
- **Win32** for the platform layer (no GLFW/SDL3)
- **Volk** for dynamic Vulkan loading, **VMA** for memory allocation

## Layout

```
engine/      Engine library — game logic, simulation, math, input, rendering.
             Talks Vulkan directly. Knows nothing about windows or message loops.
platform/    OS-divergence layer — Win32 window + message pump,
             vkCreateWin32SurfaceKHR, Raw Input / XInput plumbing.
             Depends on engine.
games/Pong/  First game — implements engine's Game interface and hands
             the instance to platform's host.
```

Engine never imports Platform. Portable input (keyboard codes, gamepad
button identity) lives in engine; the OS-specific event path lives in
platform.

## Build & run

Requires the **Vulkan SDK 1.3.296+** installed (`VULKAN_SDK` env var
set). `slangc` ships with the SDK.

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/games/Pong/Pong.exe
```

## Tests

```sh
ctest --test-dir build
```

Renderer pixel-readback tests skip on machines without a working
Vulkan ICD so the rest of the suite stays runnable.

## Status

Mid-pivot from an Apple-only Metal engine to the stack above. `engine/`,
`platform/`, and `games/Pong/` are pending in follow-up commits on this
branch. The `cpp/` directory holds the prior Metal/Objective-C++ Pong
work — kept as transitional reference for math + game-logic shapes that
port over, removed once nothing's left to mine.

## Documentation

- [`CLAUDE.md`](CLAUDE.md) — architecture, design influences, layer
  split, frame ordering, workflow conventions.
- [`TASKS.md`](TASKS.md) — engineering plan: substrate work driven by
  the current game's needs.
- [`INSPIRATION.md`](INSPIRATION.md) — art and music direction across
  games (Pong leans cyberpunk; Flappy's aesthetic is still being
  decided — the original synthwave-on-side-scroller pairing has
  alignment tension).

## License

Licensed under the [Apache License, Version 2.0](LICENSE).

Vendored third-party code (Volk, VMA, and the doctest test harness)
retains its original license — see each directory's `LICENSE.txt`. The
transitional `cpp/third_party/` retains `metal-cpp` (Apache-2.0) and
`doctest` (MIT) until `cpp/` is removed.
