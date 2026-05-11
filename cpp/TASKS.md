# Tasks — Pong (C++ port)

Companion to the Swift Engine + Flappy Bird `TASKS.md` at the repo root.
This list scopes the parallel C++/MTL4 Pong effort.

**Status (2026-05-08):** Skeleton landed (PR #37). 320×240 offscreen
target, single-triangle fullscreen blit to drawable, MTL4 frame loop
with allocator semaphore + capture scope. Window currently paints
solid blue (the offscreen clear color sampled through the blit) —
nothing is drawn yet.

Scope rule: each item should end with something visibly different on
screen (or audibly, for audio) — same rule as the Swift TASKS.md.

---

## Bringup

Suggested ordering — each step exercises substrate the next step
depends on. Flexible, not a hard sequence.

- [x] **Field + center line.** Static rectangles in the offscreen target.
  Exercises a "draw rectangles" pipeline (vertex layout, position-only
  PSO, plus the world-to-NDC transform for source-resolution coordinates)
  with no game-state coupling. Walls are optional — classic Pong has
  open top/bottom that the ball bounces off, but a thin wall sprite
  reads more clearly on a low-res target.

- [ ] **Paddles + keyboard input.** Land together; input has nothing
  to validate without paddles. Two paddles, controlled by W/S and ↑/↓.
  Engine-side: a polling `Keyboard` class with `isPressed(key)` and
  edge detection (`wasPressedThisFrame`); macOS-side: an `NSResponder`
  hook that pushes events into it. (See the Swift `Engine/Input/`
  package for the shape — `KeyboardState` value type with edge sets,
  cleared each frame after game logic consumes them.)

- [ ] **Ball.** SDF circle in fragment shader (`if dot(d,d) > r*r
  discard;`) on a quad — discussed in conversation. One quad, one
  fragment shader, no triangle tessellation. With `nearest` upscale
  the circle will be retro-pixelated and that's the look.

- [ ] **AI opponent vs. hot-seat 2P decision.** Decide before wiring
  the second paddle's source. AI is the more interesting engine-shape
  question (does it live in game logic? a separate "controller"
  abstraction?); 2P is simpler but means committing to a 2-keyboard
  control scheme. Single-player + AI is the default.

- [ ] **Game logic.** Ball physics (velocity, wall bounce, paddle
  bounce with angle-from-impact-point), serve direction, scoring,
  win condition. The classic Pong serve trick — ball direction varies
  by where it hit the paddle — turns the paddle into a steering wheel
  and is what makes the game feel good.

- [ ] **Game state machine.** Title → serve → play → score → serve →
  game-over → restart. Not strictly required (autoplay-on-launch is a
  valid v1) but the moment you want a "press space to start" you need
  it.

## Polish

- [ ] **Scoreboard.** Biggest sub-task. Two paths:
  (a) bitmap font baked into the bundle as a tiny PNG, sample via
      offsets — fits the retro aesthetic, no SDF math.
  (b) SDF font atlas — overkill for digits 0–9 but reusable for
      game-over text later.
  Recommend (a) for v1; revisit if a third text consumer surfaces.

- [ ] **Aspect-ratio handling on upscale.** Currently the blit covers
  the entire drawable, stretching to whatever window size. Real fix:
  letterbox or pillarbox to preserve 320:240 (4:3) — compute a
  centered viewport with the largest 4:3 rect that fits in the
  drawable, clear the rest to black. Goes in the blit pass's
  `setViewport` call, not the offscreen pass.

- [ ] **Audio.** Paddle hit, wall bounce, score, win. New service —
  AVAudioEngine wrapped behind a tiny `Audio` class, exposed on a
  `GameContext`-equivalent. First non-render subsystem in the cpp port.

## Substrate gaps to fix when they bite

Not standalone phases — fold into whichever feature first hits them.

- [ ] **Sim/render boundary.** Today `Pong` mixes Pong's lifecycle/
  timing, the renderer substrate (compiler/library/PSO/sampler/blit
  pass), and the per-frame command-buffer dance in one class. Once
  paddles + ball + scoreboard all draw, the natural extraction is a
  `Renderer` class that owns substrate state, leaving `Pong` as the
  game. Resist pre-factoring — wait for two games' worth of repeat
  patterns, per CLAUDE.md.

- [ ] **iOS port.** CLAUDE.md targets macOS + iPadOS. The cpp skeleton
  is macOS-only today (`PongApplication.mm` is AppKit, `main.mm`
  has no `UIApplicationMain`). Mirror the Swift project's iOS bringup
  pattern when this becomes a priority. CI gates iOS off until
  there's something to build.

## Explicitly NOT in scope

- ECS / entity registration. Pong is game #1; CLAUDE.md says wait
  for 2–3 games before extracting a registration layer.
- Asset loading from disk beyond what the bundle gives us.
- Particle effects, screen shake, juice. Pong is supposed to look
  austere; revisit only if it feels too dead.
