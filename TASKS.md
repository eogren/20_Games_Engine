# Tasks — toward 3D Flappy Bird

Target: Flappy Bird rendered in 3D with an orthographic camera tilted ~5–10°
around the X axis. This list drives engine substrate work from concrete game
needs (per CLAUDE.md: substrate first, extract workflow later).

Scope rule: each phase should end with something visibly different on screen.
If a phase doesn't, it's probably over-engineered — collapse it.

---

## Independent quick wins (do anytime)

Small fixes that don't depend on or block phase 1.

- [ ] **Pass game title into `Host`.** Currently `Host.swift:45` hardcodes
  `window.title = "Game"`. Add a `title: String` parameter to
  `Host.init(game:title:)` (default `"Game"` to keep callers compiling),
  store it, apply in `run()` before `makeKeyAndOrderFront`. FlappyBird's
  `App.swift` passes `"Flappy Bird"`. iOS branch is empty so no parallel
  change needed yet.

- [ ] **Auto-enable Metal validation in Debug builds.** During the Metal 4
  port we ran the app under `MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1`
  manually to catch binding/residency bugs. Make Debug runs do this by
  default so we keep validation pressure on without thinking about it.
  Right path is to commit a *shared* scheme at
  `FlappyBird/FlappyBird.xcodeproj/xcshareddata/xcschemes/FlappyBird.xcscheme`
  with the Run action's Diagnostics tab toggling on:
  - Metal API Validation
  - Metal Shader Validation
  (Both already on by default in fresh Xcode schemes, but the *shared*
  copy is what gets committed — currently FlappyBird's scheme lives under
  `xcuserdata/` and isn't versioned, so each clone gets a fresh default
  rather than our agreed-on flags.) Verify Release runs leave them off so
  we don't ship validation overhead. No engine code changes needed.

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
  - `float4x4.orthographic(left:right:bottom:top:near:far:)` — Metal NDC z
    range is [0,1], not [-1,1]; get this right or depth-test silently fails.
  - `float4x4.translation(_:)`, `.rotationX(_:)`, `.rotationY(_:)`,
    `.rotationZ(_:)`, `.scale(_:)`.

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

- [ ] **Renderer.drawMesh API.**
  - `drawMesh(_ mesh: Mesh, fragmentShader: String, uniforms: U, modelMatrix: float4x4)`
    — pairs `standard_mesh_vertex` with the named game fragment, uploads model
    + normal matrix to vertex buffer 2, fragment uniforms to fragment buffer 0,
    issues `drawIndexedPrimitives`.
  - View uniforms (viewProj, light) live somewhere set once per frame — see
    next item.

- [ ] **Camera in GameContext + per-frame view binding.**
  - Add `Camera` struct (position, rotation, ortho extents, near/far) with a
    `viewProjection(aspect:)` method.
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
  - Ortho camera tilted 8° on X, looking down -Z. Cube orbits a small
    circle in the XY plane while spinning on Y — the orbit exercises
    `translation`, the spin exercises `rotationY`, and the composed model
    matrix proves the multiply order is right. Catching sign/handedness
    bugs here is much cheaper than debugging them in phase 2 when six
    things are using transforms at once.
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

## Phase 2 — Flappy Bird game logic (no new engine work)

Goal: playable Flappy Bird with primitive-mesh stand-ins. Bird = cube,
pipes = stretched cubes, ground = plane. Ugly but playable.

- [ ] **Bird entity.** Position (Y only — X is fixed), velocity, gravity.
  Space (already wired through `Keyboard`) sets velocity to a fixed flap
  impulse on `pressed` edge. Visual rotation tied to velocity for that
  Flappy "nose-dive" feel.
- [ ] **Pipe pair.** Top + bottom cube, parameterized by gap center + gap
  size. Scrolls left at constant speed.
- [ ] **Pipe spawner.** Spawn at fixed time interval, randomize gap center
  within bounds, despawn off-screen left.
- [ ] **Collision.** AABB bird vs. each pipe AABB + ground plane. On hit:
  freeze sim, show "game over" (camera shake or color tint is fine for v1
  — no UI text yet).
- [ ] **Restart.** Space on game-over resets state.
- [ ] **Score.** Increment when bird X passes pipe X. Stored as int; display
  deferred to phase 3.

**Likely-wrong assumptions to revisit:**
- Fixed timestep vs. variable `dt`? Variable is what the engine passes
  today, but Flappy physics is sensitive to frame-rate spikes. Might need
  a sub-stepped fixed update; see how it feels first.

---

## Phase 3 — Polish (extracts engine work from real pain)

Only do these once phase 2 is playable and the *specific* lack hurts.

- [ ] **Text rendering** for score + game-over. SDF font atlas is the usual
  call; could also start with bitmap font baked into the app bundle.
- [ ] **Textures.** Bird sprite mapped onto a quad mesh; pipe texture. Adds
  texcoords to the standard vertex layout (versioned vertex layouts? or
  just bump everyone?).
- [ ] **Audio.** Flap, score, hit. New `ctx.audio` service in `GameContext`
  — first non-render subsystem, will probably surface boundary questions.
- [ ] **Particles** on collision / pipe-pass. First draw call that wants
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
