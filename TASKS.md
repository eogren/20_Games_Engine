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

- [x] **Renderer regression test that catches viewport-shaped bugs.** Done —
  `RendererSmokeTests.fullscreenDrawCoversTargetWithUVGradient` renders
  `drawFullscreenQuad` with a `uv_gradient` fragment emitting
  `float4(uv.x, uv.y, 0, 1)` and asserts corner pixels move in the
  expected directions (red L→R, green B→T). Test-only
  `Engine/Tests/EngineTests/Shaders/UVGradient.metal` wired via
  `.process("Shaders")` on the test target — establishes the pattern
  phase 1's standard mesh shader can reuse. (PR #7)

- [x] **`Game.load(_:)` async lifecycle + `meshLoader` on `GameContext`.** Done —
  `Game` gained `func load(_ ctx: GameContext) async throws` (default empty
  via protocol extension); `GameContext` gained `meshLoader: MeshLoader`,
  constructed once by `GameEngine` with the phase-1 vertex descriptor.
  `Host.run()` defers the `CADisplayLink` until `engine.load()` resolves so
  games see fully-populated assets in frame 0. Surfaced because the original
  plan assumed sync loading inside `update`, which doesn't fit `MeshLoader`'s
  async shape — needed a separate one-shot phase.

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

- [x] **Math: 4x4 matrices + camera helpers.** Done via the Bevy-shape
  `Transform` substrate (PRs #10–#12) plus `float4x4.perspective`
  (PR #14) and `float4x4.viewPerspectiveMatrix(cameraTransform:cameraPerspective:)`
  (PR #17). The standalone `float4x4.translation/rotationX/Y/Z/scale`
  builders originally listed here are superseded — Transform's quat-canonical
  rotation + cached 4x4 covers the per-object case, and the perspective +
  viewProjection helpers cover the camera case. `Transform.lookAt(_:)`
  replaces the proposed `float4x4.lookAt(eye:target:up:)` (sim-shape
  rather than matrix-shape; matches the sim/render split). Orthographic
  helper still deferred to Phase 3.

- [x] **Mesh type + loader (ModelIO/MetalKit-backed).** Done — `MeshLoader`
  (Engine/Renderer/MeshLoader.swift) wraps `MTKMesh` loading via `MDLAsset` +
  `MTKMeshBufferAllocator`, async-detached so a chunky parse can't hitch the
  render loop. Game-facing access is `ctx.meshLoader.loadMesh(from: URL)`.
  Shape diverged from the plan in two ways worth knowing: (1) instance class
  configured at construction with an `MDLVertexDescriptor`, not a static
  `Mesh.load(named:in:device:)` — keeps the layout decision out of every
  call site; (2) phase-1 layout is `position + texcoord` (stride 20), not
  `position + normal + texcoord` — normals roll in with the standard mesh
  shader since they're useless without lighting. OBJ-only for v1; missing
  attributes throw `MeshError.missingAttributes` instead of zero-filling.
  (PR #8)

- [ ] **Standard mesh shader (engine substrate).** Partial — `Sources/Engine/Shaders/StandardMesh.metal`
  ships `mesh_vertex` (PR #19) with per-frame `MeshGlobalUniform { viewProjectionMatrix }`
  at buffer 1 and per-draw `MeshModelUniform { modelMatrix }` at buffer 2.
  Lighting fields (`lightDir`, `lightColor`, `ambient`) and `normalMatrix`
  are deferred until a lit fragment surfaces — the cube draw uses a
  UV-only fragment for now, so the substrate hasn't needed them. World-
  space normal pass-through and the `lit` fragment land together when
  the demo actually wants shading.

- [x] **Shared shader-types header (Engine→Game→Tests).** `Sources/Engine/Shaders/EngineShaderTypes.h`
  declares `MeshVertexIn/Out`, `MeshGlobalUniform`, `MeshModelUniform`
  (PR #19). Engine `.metal` files include it directly (same source tree).
  Game `.metal` files (FlappyBird) include it via `MTL_HEADER_SEARCH_PATHS`
  in the Xcode project pointing at the engine's `Shaders` dir. Test
  shaders inline mirror structs — different bundle, different boundary,
  small enough to duplicate. Remaining gap: Swift-side bridge — the
  renderer still hand-mirrors `MeshGlobalUniform` / `MeshModelUniform`
  as private Swift structs in `Renderer.swift`. Layout drift between
  the C header and the Swift mirrors would silently produce garbage
  on the GPU. Worth folding in when a third consumer surfaces or the
  structs grow fields beyond `simd_float4x4`.

- [x] **Renderer.drawMesh API.** Done in PR #19. Final shape:
  `drawMesh(_ mesh: MTKMesh, fragmentShader: String, meshTransform: Transform)`
  pairs `mesh_vertex` with the named game fragment, issues one
  `drawIndexedPrimitives` per `MTKSubmesh`, and handles model-matrix
  upload internally. Per-call fragment uniforms (the `uniforms: U` arg)
  deferred per the typed-generic uniforms memo — re-introduce when a
  fragment surfaces that needs them. Companion `register(_:)` adds a
  mesh's vertex/index buffers to the residency set (dedup'd by
  `ObjectIdentifier`); must be called once before the first `drawMesh`
  for that mesh.

- [x] **Camera in GameContext + per-frame view binding.** Landed with a
  different shape than originally proposed. Substrate is
  `Renderer.setCamera(viewProjection:)` (PR #19), called by the game
  inside its `update` before any `drawMesh`. No `Camera` struct on
  `GameContext` and no engine-driven view-uniform upload — the game
  composes its own VP using `Transform.lookAt` + `float4x4.perspective`
  + `viewPerspectiveMatrix` and hands the matrix to the renderer. Keeps
  `Transform` out of the renderer API per the sim/render split, and lets
  a single frame switch cameras (split-screen / PIP) by calling
  `setCamera` again between draw groups. The `Camera` struct extracts
  later if 2-3 games end up writing the same eye/target/up/fov boilerplate.

- [ ] **MyGame: fullscreen-quad background + spinning lit cube on top.**
  - Keep the existing `drawFullscreenQuad("background", ...)` call. Add
    `Mesh.load(named: "cube")` and a `drawMesh(cube, ...)` issued *after*
    the quad in the same frame. Two PSOs, two draws, one render pass.
  - ~~Author a unit cube in Blender, export `cube.obj` into the FlappyBird
    app target's resources (Xcode-bundled, ends up in `Bundle.main`).~~ Done
    on the `cube` branch — `FlappyBird/FlappyBird/Resources/cube.obj` is in
    the synchronized resource folder; `MyGame.load(_:)` already loads it.
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
- ~~Where does `Mesh` live in the package — `Engine/Renderer/Mesh.swift`?
  Probably yes; it's render substrate, not game state.~~ Resolved —
  `Engine/Sources/Engine/Renderer/MeshLoader.swift`.

---

## Cross-Apple bringup — iOS port + tap input

Goal: same `MyGame` runs on macOS and iPad/iPhone, and the cube reacts to
a tap (iOS) or click/space-bar (macOS) by swapping which axis it spins
on. Two-platform support and a basic input substrate land together
because they reinforce each other — touch is the natural forcing
function for the Engine↔Platform input boundary, and a cube that reacts
to input proves both platforms are wired end-to-end.

Why now vs. after phase 2: phase 2 will add gate spawners, score,
restart, floor grid — all of which would accidentally couple to AppKit-
only assumptions if iOS doesn't exist yet to keep us honest. And the
touch substrate is shaped much better when the only consumer is a
"swap rotation axis" toggle than when half a dozen game systems need it
at once.

Scope rule: same as elsewhere. Goal state is "same `MyGame` running on
a Mac window and an iPad simulator, and the cube changes behavior when
you tap/click/press space."

- [x] **Decide iOS entry shape.** Resolved: SwiftUI `App` on iOS with a
  `UIViewControllerRepresentable` wrapping `Host.makeViewController()`;
  macOS keeps the AppKit `Host.run()`. The Platform-side wiring landed
  with the iOS `MetalView` / `Host` work below; the FlappyBird-side
  `App.swift` swap is part of the next iOS-target PR.

- [x] **Pointer substrate — Engine type, macOS wiring, MyGame demo.** Done —
  `Engine/Sources/Engine/Input/Pointer.swift` adds a `Pointer` class
  wrapping a `PointerState` value (next to `KeyboardState` in
  `Input.swift`); single boolean `tappedThisFrame` edge with
  `recordTap()` / `endFrame()`. `GameEngine` constructs and exposes it,
  clears the edge in `update(...)` after `keyboard.endFrame()`.
  `GameContext` gets a `pointer` field. macOS `MetalView` takes a
  `Pointer` at construction and overrides `mouseDown(with:)` to call
  `recordTap()`; `Host` injects `engine.pointer`. `MyGame` toggles
  through Y → X → Z → Y on `keyboard.wasPressed(.space) ||
  pointer.tappedThisFrame`, accumulating cube orientation as a
  `simd_quatf` so the active axis stays world-aligned across switches.
  **Position decision:** v1 carries no position — `tappedThisFrame: Bool`
  matches the only consumer (the toggle) and matches Keyboard's polling
  shape. `takeTap() -> Tap?` can fold in later when a position-aware
  consumer surfaces; the boolean call sites won't break.

- [x] **iOS `MetalView`** — UIView equivalent of the macOS one.
  Done in the iOS-platform-port PR. `final class MetalView: UIView` with
  `override class var layerClass: AnyClass { CAMetalLayer.self }` so
  the backing layer IS the Metal layer. `layoutSubviews` and
  `didMoveToWindow` drive `drawableSize` + `contentsScale` from
  `traitCollection.displayScale`. `touchesBegan` calls
  `pointer.recordTap()`. Depth-texture lifecycle is intentionally
  duplicated across the macOS / iOS branches of the same file —
  extract a shared helper if a third platform-glue surface appears.

- [x] **iOS `Host`** — UIKit equivalent of the macOS one. Done in the
  same PR. Single `#if`-gated `Host` class with shared init + a shared
  `runFrame(view:link:)` that both platforms' display-link selectors
  funnel into (so the per-frame contract evolves in one place). macOS
  exposes `Host.run()`; iOS exposes `Host.makeViewController()` which
  returns a private `HostViewController` owning the view + display
  link. `engine.load()` is awaited before the display link starts on
  both platforms. Title is accepted but ignored on iOS for portable
  call sites.

- [ ] **iOS target in the FlappyBird Xcode project.**
  - Lean toward a single multi-platform target over two separate
    targets. Resources (`cube.obj`, `.metal` files) shared. Verify the
    Metal toolchain compiles game `.metal` for both destinations under
    one target.
  - `App.swift` becomes `#if os(macOS) … #else … #endif` — AppKit
    `Host(...).run()` on macOS, SwiftUI `App` on iOS hosting
    `MetalView` via `UIViewControllerRepresentable`.

- [x] **Verify Metal 4 + simulator behavior.** Resolved at the SDK
  level, ahead of any runtime check. As of Xcode 17 / iPhoneSimulator26.4,
  the iOS Simulator SDK ships *stub* MTL4 headers — `MTL4RenderPass.h`
  is 17 lines on the simulator vs. 133 on the device SDK, and key
  types we use (`MTL4RenderPassDescriptor`, `MTL4RenderPipelineDescriptor`,
  …) are simply not declared. So Engine fails to *compile* against
  the simulator SDK regardless of arch, and the runtime
  `supportsFamily(.metal4)` check never gets a chance to run there.
  Decision: the iOS path is real-device-only for now. CI cross-compiles
  Platform with `Platform/build-ios.sh` (targets `generic/platform=iOS`,
  arm64 device SDK); simulator-based test runs against the renderer
  are blocked until Apple ships MTL4 in the simulator SDK.

- [ ] **iOS target in the FlappyBird Xcode project.**
  - Lean toward a single multi-platform target over two separate
    targets. Resources (`cube.obj`, `.metal` files) shared. Verify the
    Metal toolchain compiles game `.metal` for both destinations under
    one target.
  - `App.swift` becomes `#if os(macOS) … #else … #endif` — AppKit
    `Host(...).run()` on macOS, SwiftUI `App` on iOS hosting
    `Host.makeViewController()` via `UIViewControllerRepresentable`.
  - **CI surface:** add an `xcodebuild build -destination 'generic/platform=iOS'`
    step for FlappyBird (mirroring the existing macOS build step).
    Simulator destinations are blocked by the MTL4 SDK gap above; an
    iOS destination is build-only on CI and "deploy to a real device"
    for actual smoke testing.

- [ ] **Smoke-test on a real iPad/iPhone.** Real device is the *only*
  path that actually rasterizes — simulator is blocked by the MTL4
  SDK gap. Confirms both that the GPU path runs and that
  `touchesBegan` reaches `Pointer` through the real UIKit event chain.
  One real-device run before declaring bringup done.

**Explicitly NOT in this milestone:**
- **Multi-touch / drag / position-aware tap.** Phase 1's input demo is
  "did the user activate this frame" — single boolean edge. Multi-
  touch substrate and tap position want real consumers (camera
  dragging, swipe-to-flap variants) to shape them.
- **iOS lifecycle handling** (background, foreground, suspension).
  Phase 1 has no game state worth preserving across suspend; revisit
  in phase 2 once a running sim could break on backgrounding.
- **iOS-specific test target.** `PlatformTests` is `#if os(macOS)`-
  gated today. The simulator-MTL4 gap closes the obvious "run the
  existing suite on a sim destination" path anyway; revisit when
  there's a real iOS-only thing to test.
- **Simulator-based runtime testing.** Closed by the SDK gap above
  until Apple ships MTL4 in iPhoneSimulator.

**Open questions:**
- **`Pointer` API shape:** `tappedThisFrame: Bool` vs. `takeTap() -> Tap?`
  (where `Tap` carries position). Decide during implementation; cheap
  to migrate either way at this scale.
- **iOS shaders compilation.** The `.metal` toolchain produces a
  single `default.metallib` that should work cross-OS, but
  `Bundle.module`'s lookup needs to land in *both* product bundles.
  Verify on first build.
- **Multi-platform target vs. two targets.** Multi-platform is the
  modern default; only split if the build settings actually diverge.

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
