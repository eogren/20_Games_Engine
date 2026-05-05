# Inspiration & aesthetic references

The game develops along three tracks:

- **Code** — engine + game logic. See `TASKS.md`.
- **Art** — visual style, palette, geometry, post-processing.
- **Music** — soundtrack and sound effects.

This file is the shared aesthetic reference for the art and music tracks,
plus the game references that motivate them. Engineering decisions live in
`TASKS.md`; aesthetic decisions live here. When the two need to agree
(e.g. the floor grid spec), TASKS.md describes *what to build* and this
file describes *what it should look/sound like*.

---

## Game references

- **Outrun** (1986; sequels; the broader synthwave/Outrun-revival genre).
  The defining "driving toward the horizon" aesthetic: hot pink + deep
  purple + cyan + electric blue, sun-on-horizon silhouette, glowing
  receding grid floor. Primary reference for the floor grid color
  language and overall palette. The vanishing-point grid is *the* genre
  signature — this is why the game is a tunnel runner rather than a
  side-scroller.

- **Tron** / **Tron: Legacy**. Neon-on-black, glowing line-art geometry,
  emissive surfaces with no diffuse texture detail. Primary reference for
  the *visual language* of in-game objects: gates as glowing wireframe
  rectangles, bird as a glowing silhouette, no sprite textures, no
  realistic materials — everything is shape + glow.

- **Star Fox** (SNES, 1993) tunnel sections. Direct gameplay/camera
  reference: third-person camera, player ship low in frame, ring/gate
  obstacles approaching down a tunnel. The "fly through the thing"
  mechanic and the "you can see the bird as a character, not as a
  cursor" framing both come from here.

- **Flappy Bird** (Dong Nguyen, 2013). Source mechanic: tap-to-flap,
  gravity, single-axis movement, gap-threading, score = gaps cleared,
  collision = death. Tone: simple, punishing, replayable. We keep the
  mechanic and core feel; we swap the camera, presentation, and
  aesthetic genre.

---

## Art track

### Palette

Synthwave canon, deep saturation:

- Hot magenta / pink (highlight, gridlines near camera)
- Cyan / electric blue (gridlines distant, secondary glow)
- Deep purple / indigo (ambient, mid-distance)
- Electric orange (occasional accent — sunset, hit feedback)
- True black (background, sky)

Distance-based color gradient on the floor grid is the canonical move
(warm/pink up close → cool/cyan toward the horizon).

### Geometry & shape language

- **Floor**: procedural neon grid, *not* textured. Glowing gridlines on
  black, fades to horizon. See TASKS.md Phase 2 for the implementation
  spec; aesthetically this is the synthwave centerpiece and should look
  unmistakable from the first frame.
- **Bird**: shape language TBD. Likely a low-poly silhouette with
  emissive glow rather than a textured sprite — sprite-bird would clash
  with everything else. Could lean fully wireframe (Tron-style) or
  filled-with-glow-edge (Outrun car-style). Worth prototyping both.
- **Gates**: glowing rectangular openings — paired horizontal beams in
  v1 (matches "Flappy pipes" mental model), possibly full frames later.
  Beams glow; passing through could pulse the glow brighter as score
  feedback.
- **Sky**: black. Optionally a stylized sun/horizon line in mid-distance
  — the "sun on the horizon" silhouette is iconic Outrun but optional.
  Stars / scanline effect on the sky can come from the post-process pass
  rather than dedicated geometry.

### Open question: side-scenery

Sugar on top, not a core decision — list the options now so we don't
forget, defer the call until the floor grid is up and we can see what
the unfilled scene actually feels like. Options:

- **Pure grid, nothing on the sides.** Most abstract / most Outrun-pure.
  Lowest geometry budget. Risk: scene may feel empty once you've stared
  at it for a minute, and at fixed bird z the only motion cue is the
  grid scroll itself.
- **Wireframe buildings / city silhouettes scrolling past.** More Tron
  city than Outrun open road. Adds off-axis landmarks that sweep across
  the screen as they approach (perspective division does this for free
  — close things move fast in screen space, distant things move slowly),
  which is a much stronger speed cue than the floor grid alone. Note:
  the sweep effect requires *off-center* placement; objects directly down
  the flight path just grow head-on without sweeping. Costs: more meshes,
  more spawn/despawn bookkeeping, and risk of visual clutter that
  competes with the gates for attention.
- **Distant mountain silhouettes on the horizon.** The classic Outrun
  layered-mountain look — static or very slow parallax, drawn as flat
  silhouettes against the sky. Cheap, atmospheric, doesn't compete with
  gameplay focus. Probably the lowest-risk middle ground.
- **Horizon palm trees / lampposts / synthwave road furniture.** Strong
  genre signal, light geometry. Could even be billboarded sprites rather
  than full meshes.

Worth picking after the grid lands, since the call partly depends on
"how much does the empty scene actually need filling." Default plan:
ship Phase 2 with pure grid, evaluate, add scenery as Phase 3 polish if
the scene reads as too sparse.

### Post-processing

CRT / synthwave wash applied as a single fullscreen pass after the main
scene (TASKS.md Phase 3):

- **Bloom** — essential. Neon on black without bloom looks dead; with
  bloom it looks like the genre.
- **Scanlines** — adds the "old monitor" texture that ties everything to
  the 80s reference.
- **Chromatic aberration** — subtle color-channel offset, especially at
  screen edges. Don't overdo it.
- **Vignette** — optional, mild.

These are deferred until after gameplay is solid, but they're a big part
of what sells the aesthetic — the unfiltered scene will look surprisingly
flat.

---

## Music track

Open. Placeholder for what to fill in:

- **Tonal direction.** Synthwave / Outrun. Driving 4-on-the-floor,
  analog-style synth leads, gated reverb drums, occasional arpeggiators.
  Reference artists / playlists once chosen.
- **In-game music.** One main loop during play, possibly a slower/sparser
  variant on the title/game-over screen. Tempo should support the "sense
  of forward motion" since the bird itself doesn't translate.
- **SFX.** Flap, score-pass, gate-collision, game-over sting, restart.
  Synth-tone palette to match the music — not realistic foley.
- **Implementation.** Audio subsystem isn't built yet; see TASKS.md
  Phase 3 for the engine-side plumbing (`ctx.audio` in `GameContext`).
  Music/SFX work itself can start in parallel — output `.wav`/`.aiff`
  assets ready to drop in once the engine subsystem lands.
