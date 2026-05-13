# Inspiration & aesthetic references

The games develop along three tracks:

- **Code** — engine + game logic. See `TASKS.md`.
- **Art** — visual style, palette, geometry, post-processing.
- **Music** — soundtrack and sound effects.

This file is the shared aesthetic reference for the art and music tracks, plus the game references that motivate them. Engineering decisions live in `TASKS.md`; aesthetic decisions live here. When the two need to agree (e.g. a procedural shader spec), `TASKS.md` describes *what to build* and this file describes *what it should look/sound like*.

Different games sit in different aesthetic worlds — there's no engine-wide "look" being enforced. Each per-game section below is internally consistent; the engine substrate stays neutral so any aesthetic can run on it.

## Per-game aesthetic mapping

| Game | Aesthetic | Status |
| --- | --- | --- |
| Pong | Cyberpunk | Tentative direction; first art pass surfaces concrete decisions. |
| Flappy Bird | Synthwave | Under review — side-scroller vs. synthwave's vanishing-point grid has known alignment tension (see Flappy section). |

---

## Pong — cyberpunk

### Game references

- **Cyberpunk 2077** (CD Projekt Red, 2020). The modern reference for the genre's full visual language: dense neon urban grid, magenta/cyan/yellow/red palette, glitching HUD chrome, scanline/CRT artifacts on diegetic displays. Pong is much sparser than a Night City scene, but the *palette*, *HUD vocabulary*, and *glitch language* are the direct carry-over.

- **Hotline Miami** / **Hotline Miami 2** (Dennaton, 2012/2015). Neon-arcade-on-black with high-contrast geometric primitives, pulsing palette transitions tied to music beats, and a deliberately violent-arcade UI texture. Closest reference for "Pong's mechanic running inside a cyberpunk arcade cabinet." Particularly the title-screen / inter-level transitions.

- **TRON** / **TRON: Legacy**. The lightcycle arena scenes — geometric primitives glowing neon-on-black in a virtual arena — are essentially cyberpunk-Pong in motion. Primary reference for the in-game *world feel*: two paddles facing off in a virtual deathmatch arena. Note: TRON sits on the cleaner end of the genre, close to synthwave; pulling toward the grungier Cyberpunk 2077 end gives Pong its own visual identity rather than collapsing into Flappy's world.

- **Geometry Wars** (Bizarre Creations, 2003). Direct visual analog for neon-on-black primitives with heavy bloom and particle trails. The closest reference for *how Pong's pieces should look in motion* — what a glowing paddle and a hot-trailed ball read like on screen.

- **Pong** (Atari, 1972). The mechanic: two paddles, single ball, vertical-only paddle motion, score on out-of-bounds, infinite rally. Like the Flappy section, we keep the mechanic and the core feel; we swap the presentation entirely. The original's stark black-and-white CRT sits on the through-line — we're not abandoning the CRT, we're saturating it.

### Palette

Cyberpunk core, high contrast:

- **Magenta / hot pink** — paddle 1, primary HUD readout
- **Cyan / electric blue** — paddle 2, secondary HUD readout
- **Amber / yellow** — HUD accents, score chrome (Cyberpunk 2077 / Ghost in the Shell signature)
- **Red** — sparingly: hit flashes, fault feedback
- **True black** — background, arena floor base

Two-player asymmetry rule: paddles are always magenta vs. cyan to read player identity at a glance (Hotline Miami / TRON-deathmatch convention). The ball stays neutral (white core with bloom) and never takes a paddle color — avoids ambiguity when the ball touches a paddle.

### Geometry & shape language

- **Paddles**: glowing rectangles, hard edges, faint motion-trail when moving (phosphor persistence). No texture — pure emissive primitive. Same shape language as TRON light-walls.
- **Ball**: small glowing dot with a velocity-scaled trail. Trail length / intensity scales with speed, establishing a visual speed cue without needing on-screen telemetry. Trail color stays neutral so it reads across both paddle colors.
- **Center line**: dashed (Pong canon), but each dash glows and pulses subtly — possibly synced to background music tempo. The cyberpunk version of "divider in a deathmatch arena." Optional CRT-decay flicker.
- **Arena boundary / back wall**: faint flat grid pattern, *not* perspective. Pong is 2D — this distinguishes it from Flappy's synthwave vanishing-point grid. Possibly overlay a slight gradient or scanline texture so the empty space has depth without competing for attention.
- **Score readouts**: cyberpunk HUD chrome — segmented digital font (Cyberpunk 2077 quest-counter / Hotline Miami hit-counter), occasional glitch frame on score change. Sits in the upper corners, paddle-side-colored (magenta-on-magenta-side, cyan-on-cyan-side) to reinforce player identity.
- **Optional in-arena chrome**: a thin bezel framing the arena, suggestive of an arcade-cabinet CRT or a holographic-display frame. Genre signal without adding gameplay-visible elements; skip if it feels like clutter.

### Post-processing

CRT / cyberpunk wash, applied as a single fullscreen pass after the main scene:

- **Bloom** — essential. Same reason as Flappy: neon-on-black without bloom looks flat; with bloom it looks like the genre.
- **Scanlines** — denser / more visible than Flappy's. Cyberpunk's CRT is grittier.
- **Chromatic aberration** — slightly stronger than Flappy's subtle version; helps sell "old display in a future world."
- **Glitch artifacts** — occasional horizontal RGB-shift band rolling across the screen, more frequent on hits and score changes. Synthwave doesn't lean into glitch the way cyberpunk does — this is one of the load-bearing differentiators between the two aesthetics.
- **Vignette** — moderate, tighter than Flappy's. CRT-bezel feel.

### Music

Open. Direction:

- **Tonal direction.** Darksynth / cyber-industrial — Perturbator, Carpenter Brut, Daniel Deluxe end of the spectrum, not the warmer Outrun synthwave. Faster BPM, more aggressive lead, distorted bass. Reference artists / playlists to confirm once art direction is firmer.
- **In-game music.** One main loop during a rally; sparser variant on title/game-over. Tempo supports rally pacing; could subtly modulate as rally length grows for tension.
- **SFX.** Paddle hit (digital tone, slight pitch shift by hit location on paddle), score (glitchy HUD chime), wall bounce (sharper, brighter tone), fault/miss (bit-crushed low rumble). All synth-sourced, deliberately not real foley.
- **Implementation.** Audio subsystem isn't built yet; see `TASKS.md` for the engine-side plumbing (`ctx.audio` in `GameContext`). Music/SFX work itself can start in parallel — output `.wav`/`.flac` assets ready to drop in once the engine subsystem lands.

---

## Flappy Bird — synthwave (under review)

**Open question.** Synthwave's iconic visual language is "rushing toward a vanishing point" — the receding grid floor with sun-on-horizon. A side-scrolling Flappy Bird with that backdrop is aesthetically incoherent: gameplay motion (horizontal) goes the wrong direction relative to the aesthetic's signature axis (depth). The previous solve was to make Flappy a 3D tunnel-runner so the gameplay axis aligns with the grid's vanishing-point axis. That works but costs a lot of substrate for game #2 (3D, perspective camera, procedural floor shader, bloom post-pass). Alternatives still open:

1. **Stick with the tunnel-runner adaptation** and accept the substrate cost.
2. **Keep Flappy 2D** and swap the aesthetic to something native to a side-scroller camera (pastel low-poly, comic-vector, even cyberpunk).
3. **Keep synthwave** but pick a different game #2 — outrun runner, top-down racer, vertical shmup — that's natively axis-aligned with the genre.

The content below assumes path 1 (synthwave + tunnel-runner), since that was the previous direction and most of the spec carries over if it stays. Will get pruned / reshaped when the call lands.

### Game references

- **Outrun** (1986; sequels; the broader synthwave/Outrun-revival genre). The defining "driving toward the horizon" aesthetic: hot pink + deep purple + cyan + electric blue, sun-on-horizon silhouette, glowing receding grid floor. Primary reference for the floor grid color language and overall palette. The vanishing-point grid is *the* genre signature — this is why the game is a tunnel runner rather than a side-scroller.

- **Tron** / **Tron: Legacy**. Neon-on-black, glowing line-art geometry, emissive surfaces with no diffuse texture detail. Primary reference for the *visual language* of in-game objects: gates as glowing wireframe rectangles, bird as a glowing silhouette, no sprite textures, no realistic materials — everything is shape + glow. (Pong leans on TRON for the same reason in a different palette context.)

- **Star Fox** (SNES, 1993) tunnel sections. Direct gameplay/camera reference: third-person camera, player ship low in frame, ring/gate obstacles approaching down a tunnel. The "fly through the thing" mechanic and the "you can see the bird as a character, not as a cursor" framing both come from here.

- **Flappy Bird** (Dong Nguyen, 2013). Source mechanic: tap-to-flap, gravity, single-axis movement, gap-threading, score = gaps cleared, collision = death. Tone: simple, punishing, replayable. We keep the mechanic and core feel; we swap the camera, presentation, and aesthetic genre.

### Palette

Synthwave canon, deep saturation:

- Hot magenta / pink (highlight, gridlines near camera)
- Cyan / electric blue (gridlines distant, secondary glow)
- Deep purple / indigo (ambient, mid-distance)
- Electric orange (occasional accent — sunset, hit feedback)
- True black (background, sky)

Distance-based color gradient on the floor grid is the canonical move (warm/pink up close → cool/cyan toward the horizon).

### Geometry & shape language

- **Floor**: procedural neon grid, *not* textured. Glowing gridlines on black, fades to horizon. See `TASKS.md` Phase 2 for the implementation spec; aesthetically this is the synthwave centerpiece and should look unmistakable from the first frame.
- **Bird**: shape language TBD. Likely a low-poly silhouette with emissive glow rather than a textured sprite — sprite-bird would clash with everything else. Could lean fully wireframe (Tron-style) or filled-with-glow-edge (Outrun car-style). Worth prototyping both.
- **Gates**: glowing rectangular openings — paired horizontal beams in v1 (matches "Flappy pipes" mental model), possibly full frames later. Beams glow; passing through could pulse the glow brighter as score feedback.

  Shape variants worth prototyping once v1 reads:
  - **Glowing wireframe outline** (Tron-direct). Same beam/frame mesh, fragment shader does an edge-distance term so only the silhouette glows. Near-free geometrically; the work is in the shader. Best fit for the synthwave-tunnel aesthetic — neon outlines on a black grid is exactly the look.
  - **Diamond** (rectangle rotated 45° around z). Same mesh, free variation. Could alternate orientation every N gates for rhythm.
  - **Flared / trapezoidal** (back edges wider than front). Cheap perspective-amplification trick — gates appear to "open up" toward you as they approach. Reads as speed.
  - **Hoop / ring** (circular instead of rectangular). Most distinctive silhouette ("fly through the hoop" — Star Fox arches). Mesh is more work: real torus geometry or a flat ring quad with alpha cutout.

  Frame *size* is a knob independent of shape: square reads as a neutral doorway; wider-than-tall is a generous letterbox slot (matches Flappy's vertical-only movement); taller-than-wide is a tense narrow gap; just barely larger than the bird is Star Wars trench. Wider gates feel more forgiving for the same gap-height because the eye reads the whole opening as the target.
- **Sky**: black. Optionally a stylized sun/horizon line in mid-distance — the "sun on the horizon" silhouette is iconic Outrun but optional. Stars / scanline effect on the sky can come from the post-process pass rather than dedicated geometry.

#### Open question: side-scenery

Sugar on top, not a core decision — list the options now so we don't forget, defer the call until the floor grid is up and we can see what the unfilled scene actually feels like. Options:

- **Pure grid, nothing on the sides.** Most abstract / most Outrun-pure. Lowest geometry budget. Risk: scene may feel empty once you've stared at it for a minute, and at fixed bird z the only motion cue is the grid scroll itself.
- **Wireframe buildings / city silhouettes scrolling past.** More Tron city than Outrun open road. Adds off-axis landmarks that sweep across the screen as they approach (perspective division does this for free — close things move fast in screen space, distant things move slowly), which is a much stronger speed cue than the floor grid alone. Note: the sweep effect requires *off-center* placement; objects directly down the flight path just grow head-on without sweeping. Costs: more meshes, more spawn/despawn bookkeeping, and risk of visual clutter that competes with the gates for attention.
- **Distant mountain silhouettes on the horizon.** The classic Outrun layered-mountain look — static or very slow parallax, drawn as flat silhouettes against the sky. Cheap, atmospheric, doesn't compete with gameplay focus. Probably the lowest-risk middle ground.
- **Horizon palm trees / lampposts / synthwave road furniture.** Strong genre signal, light geometry. Could even be billboarded sprites rather than full meshes.

Worth picking after the grid lands, since the call partly depends on "how much does the empty scene actually need filling." Default plan: ship Phase 2 with pure grid, evaluate, add scenery as Phase 3 polish if the scene reads as too sparse.

### Post-processing

CRT / synthwave wash applied as a single fullscreen pass after the main scene (`TASKS.md` Phase 3):

- **Bloom** — essential. Neon on black without bloom looks dead; with bloom it looks like the genre.
- **Scanlines** — adds the "old monitor" texture that ties everything to the 80s reference.
- **Chromatic aberration** — subtle color-channel offset, especially at screen edges. Don't overdo it.
- **Vignette** — optional, mild.

These are deferred until after gameplay is solid, but they're a big part of what sells the aesthetic — the unfiltered scene will look surprisingly flat.

### Music

Open. Placeholder for what to fill in:

- **Tonal direction.** Synthwave / Outrun. Driving 4-on-the-floor, analog-style synth leads, gated reverb drums, occasional arpeggiators. Reference artists / playlists once chosen.
- **In-game music.** One main loop during play, possibly a slower/sparser variant on the title/game-over screen. Tempo should support the "sense of forward motion" since the bird itself doesn't translate.
- **SFX.** Flap, score-pass, gate-collision, game-over sting, restart. Synth-tone palette to match the music — not realistic foley.
- **Implementation.** Audio subsystem isn't built yet; see `TASKS.md` for the engine-side plumbing (`ctx.audio` in `GameContext`). Music/SFX work itself can start in parallel — output `.wav`/`.aiff` assets ready to drop in once the engine subsystem lands.
