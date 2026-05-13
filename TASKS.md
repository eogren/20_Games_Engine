# Tasks

**Status (2026-05-12):** Engine is mid-pivot from Apple-only Metal/Swift
to Windows-primary Vulkan/C++ (see [`CLAUDE.md`](CLAUDE.md)). This
top-level plan is being rebuilt from scratch alongside the
`engine/` + `platform/` scaffolding work. Until that lands, the
substrate task list lives in the scaffolding PR rather than here.

## Immediate work

- [ ] Scaffold `engine/`, `platform/`, `games/Pong/` (CMake skeleton,
  Win32 window, Vulkan instance + device + swapchain, first cleared
  frame on screen).
- [ ] Port `cpp/src/math/` into `engine/src/math/` — first portable
  substrate that survives the pivot intact.
- [ ] First triangle through the Slang → SPIR-V → Vulkan pipeline.
- [ ] Bring Pong gameplay up on the new renderer — paddles, ball,
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
