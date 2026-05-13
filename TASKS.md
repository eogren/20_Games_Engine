# Tasks

**Status (2026-05-12):** Engine is mid-pivot from Apple-only Metal/Swift
to Windows-primary Vulkan/C++ (see [`CLAUDE.md`](CLAUDE.md)). Scaffold +
vendored deps + verified build are in; first real Vulkan/Win32 surface
is what's next.

## Immediate work

- [x] **Scaffold `engine/` + `platform/` + `games/Pong/`** — CMake
  graph, vendored deps (Volk, VMA, doctest), placeholder source files.
  `cmake -G Ninja` + MSVC verified on Windows; Pong.exe runs, ctest
  smoke test passes.

Next steps below. (2) and (3) are independent and can move in parallel;
(4) is where they meet. (1) has no Vulkan/Win32 surface so it's a safe
warm-up at any point.

- [ ] **Port `cpp/src/math/` into `engine/src/math/`.** First portable
  substrate that survives the pivot intact. Pure C++, no Vulkan or
  Win32 — fast warm-up, doctest tests carry over from
  `cpp/tests/MathTests.cpp`.
- [ ] **First Volk init + `VkInstance` creation** in engine. Proves
  the Vulkan SDK + Volk loader chain works end-to-end without needing
  a window. `vulkaninfo`-equivalent output is enough confirmation.
  Independent of Win32.
- [ ] **Win32 window in `platform/`.** Visible `HWND` with a pumping
  message loop, clean shutdown on `WM_CLOSE`. No Vulkan yet —
  developable in parallel with (2).
- [ ] **Surface + swapchain + first cleared frame.** Where (2) and (3)
  meet: `vkCreateWin32SurfaceKHR` from platform feeds engine's
  `VkSwapchainKHR`; engine clears the swapchain image to a solid color
  per frame and presents. First pixels on screen.
- [ ] **First triangle through Slang → SPIR-V → Vulkan.** Wires up the
  `slangc` custom-command pipeline in CMake, a vertex+fragment Slang
  shader under `engine/shaders/`, a graphics pipeline, and a single
  draw call.
- [ ] **Bring Pong gameplay up on the new renderer.** Paddles, ball,
  collision, score. The cpp/ Pong gameplay shape (see `cpp/TASKS.md`)
  ports over; the renderer underneath does not.

## Game #2 — pending

Flappy Bird was the original target before the pivot, planned as a
synthwave tunnel-runner to align the gameplay axis with the iconic
sunset/grid's vanishing-point axis. That plan is **parked** — needs
re-resolution of two open questions before driving engine work:

1. **Does the synthwave/Flappy pairing survive?** Side-scroller fights
   the aesthetic; tunnel-runner solves the alignment but costs a lot
   of substrate (3D, perspective camera, procedural ground shader,
   bloom post-pass) for game #2. Alternatives worth weighing:
   different aesthetic for Flappy (cyberpunk, pastel low-poly), or a
   different game #2 that's natively synthwave-axis (outrun runner,
   top-down racer, vertical shmup).

2. **What substrate does game #2 actually need?** Pong is 2D
   primitives — the answer to question 1 determines whether game #2
   adds perspective + 3D + post-processing or stays 2D.

Engine substrate work for game #2 is deliberately not specified here
until those questions settle.

## Reference

- [`cpp/TASKS.md`](cpp/TASKS.md) — the legacy Pong-on-Metal task list.
  Gameplay-side phases (paddles, ball, score, audio, CRT post) port
  over conceptually even though the substrate underneath them is being
  replaced. Useful as a "what was the shape of Pong work" reference
  while the new `engine/` comes up; deleted with `cpp/` once the new
  tree is far enough along.
