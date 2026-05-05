# Tasks — toward 3D Flappy Bird

Target: tunnel-runner Flappy Bird with a perspective camera. Bird fixed at
origin with Y-only motion (tap-to-flap + gravity); gates approach on a
treadmill in -Z; synthwave-style procedural neon floor grid for the
visual aesthetic. See INSPIRATION.md for the art/music direction. This
list drives engine substrate work from concrete game needs (per CLAUDE.md:
substrate first, extract workflow later).

Why this shape vs. a side-scroller: a side-scrolling Flappy Bird with a
synthwave grid background is aesthetically incoherent — the grid's whole
identity is "rushing toward a vanishing point" and side-scrolling motion
goes the wrong direction. Tunnel-runner aligns the gameplay axis with the
grid's vanishing-point axis. Gameplay logic stays equivalent or simpler
(Y-only input, single moment-of-crossing collision check vs. continuous
AABB sweep); the cost is paid in the renderer, which now needs perspective
+ depth + a procedural floor shader.

Scope rule: each phase should end with something visibly different on screen.
If a phase doesn't, it's probably over-engineered — collapse it.

---

## Independent quick wins (do anytime)

Small fixes that don't depend on or block phase 1.

- [x] **Pass game title into `Host`.** Done — `Host.init(game:title:fpsCap:)`
  takes a `title: String = "Game"`; FlappyBird passes `"Flappy Bird"`.

- [x] **Frame-rate cap as a `Host` option.** Done — `Host.init` takes
  `fpsCap: Int? = nil`, applied via `CAFrameRateRange` on the display link
  when non-nil. FlappyBird passes `60`.

- [ ] **Renderer regression test that catches viewport-shaped bugs.** The
  current `RendererSmokeTests.clearToBlackFillsTargetWithBlackPixels`
  only exercises the clear path; it didn't catch the missing
  `setViewport` in `beginFrame` because that bug only manifests when a
  draw runs (clears go through `loadAction` and bypass the rasterizer).
  Add an offscreen draw-and-readback test: render `drawFullscreenQuad`
  with a fragment that emits, e.g., `float4(uv.x, uv.y, 0, 1)`, read
  back the texture, and assert that the four corner pixels differ from
  the clear color in the expected directions (top-right ≠ bottom-left,
  etc.). Blocker: the test needs a `.metal` fragment shader to load as
  the "game library" — easiest path is shipping a tiny test-only
  `.metal` in `Engine/Tests/EngineTests/Shaders/` and wiring it through
  Package.swift's test-target resources. Natural to slot in alongside
  phase 1's standard mesh shader since both need a "tests have a real
  fragment shader available" pattern.

---

## Phase 1 — 3D substrate (engine + platform)

Goal: keep the fullscreen quad as a background and draw a lit, tilted,
depth-tested cube *on top* of it. Two distinct render paths coexisting in
one frame is the real test — single-cube would let bugs in PSO/depth/format
caching hide. No game logic yet.

- [ ] **Depth attachment plumbing.**
  - `Platform/MetalView` allocates a depth texture matching the drawable size,
    rebuilds it on resize, and sets `depthAttachment` on the pass descriptor
    each tick (load=clear, store=dontCare, clearDepth=1.0).
  - `Renderer` learns the depth pixel format at `beginFrame` and includes it
    in `PipelineKey` so PSOs are cached per (fragment, color, depth) tuple.
  - Decide: is depth always-on, or opt-in per draw? Default always-on is
    simpler; revisit if a 2D-only path appears.

- [ ] **Math: 4x4 matrices + camera helpers.**
  - `float4x4` (typealias to `simd_float4x4` is fine) with `*` for compose.
    `simd_float4x4` is column-major (Metal's convention) — no transpose
    needed crossing the CPU/GPU boundary. Operators compile to NEON
    instructions on Apple Silicon (parallel FMAs, single-digit ns per
    matmul); no need to reach for Accelerate/AMX at 4x4 scale.
  - `float4x4.perspective(fovY:aspect:near:far:)` — Metal NDC z range is
    [0,1], not [-1,1]; get this right or depth-test silently fails.
  - `float4x4.lookAt(eye:target:up:)` — produces the world-to-camera
    transform directly. Avoids constructing a camera transform and
    inverting it; ergonomic for both static poses and per-frame rebuilds
    when the camera follows the bird.
  - `float4x4.translation(_:)`, `.rotationX(_:)`, `.rotationY(_:)`,
    `.rotationZ(_:)`, `.scale(_:)`.
  - Orthographic projection helper deferred to Phase 3 — first user is
    HUD/text rendering. YAGNI until then.

- [ ] **Mesh type + loader (ModelIO/MetalKit-backed).**
  - `Mesh` wraps `MTKMesh` (which already owns vertex/index buffers + submesh
    metadata). Engine doesn't author geometry — Blender does.
  - Canonical vertex layout v1: `position: float3`, `normal: float3`,
    `texcoord: float2`. Define once as an `MDLVertexDescriptor`; ModelIO
    repacks any loaded asset to match, so the standard shader's vertex input
    is fixed regardless of how the artist exported.
  - `Mesh.load(named:in:device:)` — looks up `<name>.obj` in the given bundle
    (defaults to `.main`, i.e. the game's bundle), loads via `MDLAsset` with
    the canonical descriptor + `MTKMeshBufferAllocator`, returns the first
    `MTKMesh`. Trap with a clear message if the file is missing or has no
    meshes.
  - Format: **OBJ** for v1 (native Blender export, ASCII debuggable, no
    texture dependency). Revisit USDZ in phase 3 when textures land.
  - No asset cache yet — game holds `Mesh` instances as fields. Add a cache
    when hot-reload or streaming forces it.

- [ ] **Standard mesh shader (engine substrate).**
  - `Sources/Engine/Shaders/StandardMesh.metal` adds `standard_mesh_vertex`
    that takes a vertex struct + per-frame `ViewUniforms { viewProj, lightDir,
    lightColor, ambient }` at buffer 1, and per-draw `ModelUniforms { model,
    normalMatrix }` at buffer 2.
  - Game ships a `lit` (or named) fragment that consumes interpolated
    world-space normal + a base color uniform at fragment buffer 0.

- [ ] **Shared shader-types header (Engine→Game→Tests).**
  - `Sources/Engine/Shaders/EngineShaderTypes.h` declares `ViewUniforms`
    and `ModelUniforms` (and any future shared structs) using `simd` types
    (`simd_float4x4`, `simd_float3`, etc.) so the Swift, Metal, and C
    compilers all see compatible memory layouts. Single source of truth.
  - Engine `.metal` files `#include "EngineShaderTypes.h"` for the buffer
    parameter types in `standard_mesh_vertex`. Game `.metal` files do the
    same for any fragment that consumes view/model uniforms. Tests too,
    once a renderer test grows uniforms beyond the current dummy.
  - Swift side: a small C-header sibling target (or a header exposed via
    SwiftPM `headerSearchPaths`) re-exports the same types into Swift,
    so `GameEngine.update` can populate `ViewUniforms` without
    hand-mirroring the layout.
  - Wiring caveats: Metal compiler needs `MTL_HEADER_SEARCH_PATHS` (Xcode)
    or `headerSearchPaths` (SwiftPM) pointing at the engine's header dir
    from each target that consumes it. Use `simd_*` typedefs exclusively
    in shared structs — bare `float3` is 12 bytes in C / 16 in Metal, a
    silent layout-mismatch trap. Stick to numeric types; bools/enums
    don't share cleanly.
  - Why now and not earlier: with one production uniforms struct
    (`BackgroundUniforms { time }`) the duplication is one line per side
    and the wiring overhead would dwarf the savings. `ViewUniforms` +
    `ModelUniforms` is where hand-mirroring becomes a real bug source —
    add a field on one side and forget on the other and you get garbage
    on the GPU with no validation error. Land the header alongside the
    standard mesh shader, before the second consumer copies the layout.

- [ ] **Renderer.drawMesh API.**
  - `drawMesh(_ mesh: Mesh, fragmentShader: String, uniforms: U, modelMatrix: float4x4)`
    — pairs `standard_mesh_vertex` with the named game fragment, uploads model
    + normal matrix to vertex buffer 2, fragment uniforms to fragment buffer 0,
    issues `drawIndexedPrimitives`.
  - View uniforms (viewProj, light) live somewhere set once per frame — see
    next item.

- [ ] **Camera in GameContext + per-frame view binding.**
  - Add `Camera` struct (eye, target, up, fovY, near, far) with a
    `viewProjection(aspect:)` method built from `lookAt` + `perspective`.
    V and P are per-frame constants; build VP once, ship it via the per-
    frame uniform buffer rather than recomposing per object.
  - `GameContext` exposes `camera: inout Camera` (game mutates it; engine
    reads after `update` returns? or game sets at top of update? — pick one).
  - `GameEngine.update` uploads `ViewUniforms` to vertex buffer 1 between
    `beginFrame` and game `update`, so every `drawMesh` call inherits it.

- [ ] **MyGame: fullscreen-quad background + spinning lit cube on top.**
  - Keep the existing `drawFullscreenQuad("background", ...)` call. Add
    `Mesh.load(named: "cube")` and a `drawMesh(cube, ...)` issued *after*
    the quad in the same frame. Two PSOs, two draws, one render pass.
  - Author a unit cube in Blender, export `cube.obj` into the FlappyBird
    app target's resources (Xcode-bundled, ends up in `Bundle.main`).
  - Perspective camera at `eye=(0, 1, 5)`, `target=(0, 0, -10)`,
    `up=(0, 1, 0)` — same pose Phase 2 will use. Cube placed around
    `z=-3` orbits a small circle in the XY plane while spinning on Y —
    the orbit exercises `translation`, the spin exercises `rotationY`,
    and the composed model matrix proves the multiply order is right.
    Perspective foreshortening should be visible as the cube moves
    forward/backward in z (closer = bigger), which also validates the
    projection itself. Catching sign/handedness/projection bugs here is
    much cheaper than debugging them in phase 2 when six things are using
    transforms at once.
  - `Shaders.metal` gains a `lit` fragment (Lambertian, hardcoded base
    color or color uniform).
  - **Quad-vs-cube depth interaction.** The fullscreen quad's vertex
    shader synthesizes NDC positions directly, so its z is whatever it
    emits. With depth always-on, the cube must win the depth test against
    the quad. Two clean options: (a) emit `position.z = 1.0` from the
    fullscreen vertex shader so the quad sits at the far plane, or (b)
    give the quad PSO a depth state of `compare = .always, write = false`
    so it ignores depth entirely. Pick (a) if "background sits at far,
    everything else draws over it" is a useful invariant; pick (b) if
    you want the quad to be philosophically a 2D overlay decoupled from
    the 3D world. (a) is probably the right answer.
  - **Blocker note:** this milestone needs a real `.obj` to exist. If you
    don't have one handy when implementing, Blender's default cube
    (File → New → General, then File → Export → Wavefront OBJ) is exactly
    right.

**Open questions for phase 1:**
- Push constants (`setVertexBytes`) vs. a per-draw uniform buffer ring? Inline
  bytes are fine for ≤4KB and we're well under; revisit if draw count gets
  high enough that command-encoder overhead matters.
- Where does `Mesh` live in the package — `Engine/Renderer/Mesh.swift`?
  Probably yes; it's render substrate, not game state.

---

## Phase 2 — Tunnel-runner gameplay + procedural floor grid

Goal: playable tunnel-runner Flappy Bird with primitive-mesh stand-ins.
Bird = cube, gates = paired beams, floor = single quad with procedural
neon-grid fragment shader. Ugly-but-distinctive and playable.

- [ ] **Bird entity.** Position fixed at `(0, 0, 0)` — only Y-velocity
  moves. Gravity each tick; space (already wired through `Keyboard`) sets
  Y-velocity to a fixed flap impulse on `pressed` edge. Visual rotation
  tied to velocity for that Flappy "nose-dive" feel.

- [ ] **Gate entity.** Two horizontal beams forming a vertical opening:
  top beam from `(-W, gapTop, z)` to `(+W, ceiling, z)`, bottom beam from
  `(-W, floor, z)` to `(+W, gapBottom, z)`. Single z, both beams move
  together. Built from the same cube mesh as the bird, scaled.

- [ ] **Gate spawner.** Spawn at fixed time interval at `z = -50`,
  randomize `gapCenterY` within bounds, treadmill toward `z = 0` at
  constant speed `S`. Despawn at `z >= 0`. Treadmill (rather than
  forward-moving bird) is the right model — keeps coordinates near
  origin, so float precision stays clean and gate cleanup is a trivial
  cull condition.

- [ ] **Procedural neon floor grid.**
  - Single static floor quad on the `y=0` plane: ~200 units forward,
    ~40 units wide. Two triangles, four vertices, world-space positions
    baked into the buffer (model matrix = identity). One quad is
    sufficient — perspective-correct interpolation handles everything,
    subdivision would only matter for vertex displacement.
  - Game-bundled fragment shader (`FlappyBird/Shaders/Grid.metal`) takes
    interpolated world `(x, z)` as a varying. Computes gridline distance
    analytically with `fract` + `fwidth` for free perspective-correct AA
    (sharp close, soft at horizon, no mipmap shimmer).
  - Animates with `worldZ + time * S` to fake forward motion — must use
    the same `S` as the gate treadmill or the ground stops feeling like
    ground.
  - Fades intensity to zero past ~150 units of view-space depth so the
    quad's far edge is invisible (horizon → black).
  - Color: synthwave palette per INSPIRATION.md.

- [ ] **Collision.** At the moment a gate's `z` crosses 0, sample bird
  `(x, y)` against the gap rect. Single point-in-rect check per gate per
  frame, no continuous AABB sweep — the perspective reframe earns this
  simplification (the original side-scroller needed continuous overlap
  while the pipe was in the bird's x-range; here, gates only "exist" at
  the bird's depth for one moment). On hit: freeze sim, show "game over"
  (camera shake or color tint is fine for v1 — no UI text yet).

- [ ] **Score.** Increment when a gate's `z` crosses 0 (same boundary as
  the collision check). Stored as int; display deferred to phase 3.

- [ ] **Restart.** Space on game-over resets state.

- [ ] **Camera y-lerp (optional polish).** Camera `eye.y` lerps toward
  bird `y` at ~3-5% per frame so big bird movements drift on screen but
  don't escape the frame. Preserves the altitude readout (because the
  lag is visible) while preventing the bird from clipping top/bottom.
  Skip in v1 if a fully-locked camera reads fine at the chosen FOV.

**Likely-wrong assumptions to revisit:**
- Fixed timestep vs. variable `dt`? Variable is what the engine passes
  today, but Flappy physics is sensitive to frame-rate spikes. Might need
  a sub-stepped fixed update; see how it feels first.
- Gate visual: two beams or full rectangular frame with a hole? Two beams
  is simplest and reads as "Flappy pipes in 3D." Frames might read better
  as "fly-through gates" but cost more geometry — try beams first.
- Fade distance for the floor grid: 150 units is a guess. May need to
  tune against the chosen FOV/near/far so the horizon falloff happens at
  a visually pleasing depth, not too close (kills the rush feel) or too
  far (the quad's edge becomes visible).

---

## Phase 3 — Polish (extracts engine work from real pain)

Only do these once phase 2 is playable and the *specific* lack hurts.

- [ ] **Text rendering** for score + game-over. SDF font atlas is the usual
  call; could also start with bitmap font baked into the app bundle. First
  user of `float4x4.orthographic(...)` (HUD is screen-space ortho on top of
  the perspective scene) — add the helper here, not in Phase 1.
- [ ] **Bird mesh + gate aesthetic.** Replace cube placeholders. The
  aesthetic per INSPIRATION.md is neon/wireframe/glow, not sprite-textured
  — most likely path is a low-poly bird mesh shaded with the same emissive
  glow treatment as the gates and floor grid. May still want texcoords in
  the standard vertex layout for normal maps or detail textures (versioned
  vertex layouts? or just bump everyone?).
- [ ] **CRT / synthwave post-processing.** Single fullscreen post-pass
  after the main scene: bloom (essential for neon-on-black), scanlines,
  chromatic aberration, optional vignette. First draw call that wants a
  multi-pass setup (render scene to offscreen target → post-process →
  drawable) — natural forcing function for adding intermediate render
  targets to the substrate.
- [ ] **Audio.** Flap, score, hit, ambient synthwave loop. New
  `ctx.audio` service in `GameContext` — first non-render subsystem, will
  probably surface boundary questions.
- [ ] **Particles** on collision / gate-pass. First draw call that wants
  instancing or a dynamic vertex buffer — good forcing function for that
  substrate.

---

## Explicitly NOT in scope yet

- **ECS / entity registration.** CLAUDE.md says wait for 2–3 games. Flappy
  is game #1; keep entities as plain Swift in `MyGame`.
- **Scene graph / retained-mode rendering.** Same reasoning. Immediate-mode
  `drawMesh` in `update` is fine.
- **Render graph / multi-pass.** One forward pass is enough until shadows
  or post-processing show up.
- **Asset loading from disk** beyond what `Bundle.main` already gives us.
  No glTF, no custom mesh format.
- **Material abstraction.** Shader name + uniforms struct per call is
  verbose-but-clear; bundle into `Material` only when 3+ call sites repeat
  the same triple.
