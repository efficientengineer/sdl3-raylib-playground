# FIELD_NOTES — what the engine actually does

Companion to `FIELD.md` (the contract). Where the two disagree, this file describes the shipped
build and says why. Code: `src/field.h`, `src/field.cpp`, hooks in `src/star_logic.cpp`.
Maps: `story/field/maps/halm.map`, `story/field/maps/hart_yard.map`.

---

## What works

- **Title → intro → field.** The intro playlist runs as before; when it ends the game walks out into
  `halm.map`. The Dev overlay has a **Field** button that jumps straight there.
- **2.5D renderer.** Ground and walls are real 3D geometry from the map's height grid; buildings are
  3D boxes; everything else is a camera-facing billboard. It all renders into an offscreen FBO during
  `tick()` and goes to `ImGui::GetBackgroundDrawList()->AddImage` as one nearest-filtered image,
  because the host clears the framebuffer *after* tick. GL state is handed back the way the ImGui
  backend expects it (FBO 0, viewport, blend on, depth off, program 0, VAO 0, buffer 0, unit 0).
- **Navmesh movement** with a walker radius, so nothing clips into a fence or a wall.
- **Camera zones** — follow / fixed / rail per zone, with a framing *guarantee*.
- **See-through occluders** — a dithered circle cut out of whatever stands between camera and player.
- **Structures** — profiles swept along paths or revolved: walls, kerbs, steps, a bridge, a well ring.
  Every open navmesh edge gets one, except where the terrain, a building or an **exit** explains itself.
- **Splat-mapped ground** — four blended layers with per-layer hardness and height masks, no per-cell
  tile grid.
- **Examine-to-read** — a "!" over the player's head, and the interact press opens the box.
- **Organic map layout** built to FIELD.md's "Map design rules" (checklist at the end of this file).
- Touch controls, NPC talk scenes through the existing cutscene player, map exits, message triggers,
  zone triggers logged to the Dev messages.
- **Hot reload keeps your place**: map, position, navmesh polygon, facing, the camera zone you were
  tuning and every debug toggle survive. An older blob falls back to the map's spawn, never crashes.

---

## Movement — the navmesh (supersedes FIELD.md's "Rules" paragraph)

Movement runs on a hand-authored convex-polygon navmesh. **The tile height grid, the walls, the props
and the buildings are rendering only.** Nothing blocks: obstacles are simply where the navmesh isn't.

### `## nav` syntax

```
## nav
1: 12.00 1.00 4.00, 17.00 1.00 4.00, 17.00 7.00 4.00, 12.00 7.00 4.00
20: 17.00 7.00 4.00, 18.00 7.00 4.00, 18.00 12.00 0.00, 17.00 12.00 0.00   # a ramp: one sloped poly
44: 5.00 26.00 0.00, 8.00 26.00 0.00, 8.00 29.00 0.00, 5.00 29.00 0.00 | hedge
```

- One polygon per line: `<id>: x z h, x z h, x z h[, ...]`, 3 to 8 vertices.
- `x z` are map units (a cell is 1.0). `h` is the vertex height **in half-steps** (the same unit the
  `## height` grid uses), so `4` is 1.0 world units up. Fractions are allowed. Each vertex carries its
  own height, so **a ramp or a hillside is one sloped polygon**.
- Winding does not matter — the loader normalises it from the signed area. Convexity does.
- Optional trailing `| fence` / `| hedge` / `| wall` / `| none` says what to draw on that polygon's
  open edges. The map-wide default is `edges: <kind>` in `## meta`.

### How it behaves

- Adjacency is computed at load from shared edges (the same two endpoints in either order, 0.01
  tolerance). An open edge whose midpoint lies **inside** another polygon logs
  `field: nav poly N edge E overlaps poly M but shares no vertices` — the one authoring mistake worth
  shouting about, and the only reason two polygons that look joined aren't.
- Player state is `(polygon, x, z)`. Height is **interpolated from the polygon's plane**, fitted
  through its three best-spread vertices — never sampled from the height grid.
- Moving applies the velocity, then: leaving through an edge **with** a neighbour walks into the
  neighbour; an edge **without** one projects the rest of the motion along it. Up to 4 crossings per
  frame, then a containment check that snaps back rather than leaving the mesh. Penetration is
  impossible by construction.
- **Walker radius** (`WALK_RADIUS`, 0.30 cells, Dev slider 0–0.8). The mesh resolves a point but the
  sprite has width, so after the slide-resolve the point is pushed out of every **unshared** edge of
  its polygon *and of its neighbours* that is closer than the radius, and the push is itself run
  through `mesh_move` so it can never end up off the mesh. Two iterations, which handles corners.
  Shared edges are deliberately left alone so crossings still line up exactly. The same pass runs on
  spawn, so you never appear inside a fence.
  *(The brief offered navmesh insetting as the first option; the push-out was taken because insetting
  a shared-edge mesh has to leave shared edges alone anyway, and a collapsed inset polygon is a much
  worse failure than a 0.3-cell push.)*
- NPCs are soft: a radius push-out (0.55), re-resolved through the mesh, then the wall buffer again.
- Triggers, exits and the NPC reach test are **rectangles in map units**.
- Fence strips stand exactly on the navmesh edge and buildings sit entirely outside it (the map
  generator asserts that), so what you see and what stops you are the same line. The walker sprite's
  anchor is its feet centre (the billboard's bottom edge, centred).

---

## Cameras — zones

```
## cameras
0 0 40 30 follow 0 50 30 13 0.8
14.0 13.5 9.0 8.0 follow -15 52 27 11.5 0.9
17.5 6.0 8.0 8.5 fixed 19.0 8.5 19.0 22.6 2.4 5.2 30
5.5 16.5 9.0 7.0 rail 5.5 6.5 26.0 14.0 6.5 26.0 18.6 1.2 17.4 32
```

- `follow <yaw> <pitch> <fov> <dist> <height>` — orbits the player. **Yaw is per zone**; a 90° turn is
  legal and the billboards keep facing the camera. (10 tokens.)
- `fixed <cx> <cy> <cz> <tx> <ty> <tz> <fov> [pan]` — FF7 style: the camera stays put, the player walks
  toward or away from it and perspective does the shrinking. (12 tokens, 13 with `pan`.)
- `rail <ax> <ay> <az> <bx> <by> <bz> <tx> <ty> <tz> <fov>` — the eye slides A→B with the player's
  progress along the zone's long axis, always looking at T. (**15 tokens** — an off-by-one here made
  the rail line parse as a follow shot with `height 6.5`, which is what the framing watchdog caught.
  A camera line that doesn't parse now logs and is ignored instead of silently becoming a follow.)
- The player's position picks the zone; **last definition wins** on overlap; a zone covering the whole
  map is required and is synthesised with a warning if `## cameras` is missing.
- Zone changes blend position, target and fov over **0.4 s** with a smoothstep.

### Framing — a guarantee, not a target

Two layers:

1. Every frame the engine binary-searches the smallest blend `u ∈ [0,1]` from the authored shot toward
   looking straight at the player that puts the player's head inside a limit rectangle:
   - **follow**: middle **50 %**. Below that it is the authored shot with a focus that **leads the
     player by 0.9 units** and lags with a 0.28 s time constant — so the lag only exists inside the
     rectangle. `u` blends the look-at's x, z **and y**, or a tall `height` would make u = 1 useless.
   - **fixed / rail**: **70 %**. (The brief said 80 %; at 0.8 a long street shot leaves the player half
     behind a foreground prop at the near end of the zone.) For `fixed` and `rail` only the look-at
     moves — this is the automatic `pan`, and `pan` in the map file just forces it on.
   `u` is applied **immediately** when it rises and released over 0.5 s, so the player can never slide
   out while the camera catches up.
2. After the camera is smoothed, the **drawn** camera is re-checked and hard-clamped: if the player is
   outside 90 % of the frame the look-at is pulled onto them by a second binary search. This is what
   makes the property true regardless of lag, zone edges, rail ends or a mis-authored shot — and it
   logs one Dev message every 4 s when it has to:
   `camera: clamped onto the player at halm 8.7,18.1 zone 3 u 1.00 (ndc 0.00,-1.65)`.
   That message is the debug aid the owner asked to keep; it is also how the rail bug above was found.

The follow focus is clamped to the map, so the look-at never runs off the edge.

### The shots in the two maps (a landmark is in frame in each)

| map | zone | shot | landmark |
|---|---|---|---|
| halm | whole map | follow, yaw 0, pitch 50, fov 30, dist 13 | the well / the hill |
| halm | the well square | follow, yaw −15, pitch 52, fov 27, dist 11.5 | the well |
| halm | the hill road | **fixed** (19, 8.5, 19) → (22.6, 2.4, 5.2), fov 30 — the depth shot up the bent road | Hart's hill and the gate |
| halm | the west street | **rail** (5.5,6.5,26)→(14,6.5,26) looking at the square | the well square |
| halm | the grain yard | **fixed** (8, 7, 21) → (6.8, 1.2, 12.6), fov 30 | the grain shed |
| hart_yard | whole map | **fixed**, low pitch: (11.5, 8.5, 28) → (11.5, 0.9, 8.5), fov 32 | the ladder house and the posts |

**Tuning on the phone**: the Dev sliders edit the *live* zone. **print zone** writes it back in map
syntax to the Dev message log, ready to paste into the `.map` file.

---

## See-through occluders

The owner's request: see the character when something is in front of them, and only then.

- In the fragment shader, an occluder fragment is cut when it is **between the camera and the player**
  (`gl_FragCoord.z < player_window_z - 0.0006`) and within a screen-space radius of the player's
  chest. The cut is an **ordered 4×4 Bayer dither** whose density ramps from a full hole at the centre
  to nothing at the radius: `discard if bayer(fragcoord) + d/radius < 1.0`. Screen-door, not alpha —
  no blending, no sorting, the depth test stays intact, and it reads as pixel art rather than glass.
- **Occluders** are buildings, wall/cliff faces, fence strips, props and other walkers (`u_occl = 1`).
  Ground, floors, water and **the player's own billboard** are never cut (`u_occl = 0`).
- The uniform is `u_see = vec4(player screen x, y in FBO pixels, player window depth, radius px)`;
  radius 0 turns it off entirely, so the common case costs one compare.
- **Radius fade**: a CPU test each frame asks whether anything actually blocks the player — a ray from
  the eye to the chest against every building's oriented box, every unshared navmesh edge's strip,
  every prop's cylinder, and a march over the wall heightfield. The radius eases toward
  `occluded ? dev_radius : 0` with a 0.09 s time constant (≈0.2 s in and out), so it never pops.
- Dev: **see-through occluders** toggle and a **radius px** slider (default 46 px at 360 high).

---

## Structures — profiles, sweeps and lathes

Nothing in the world is a paper-thin strip. A **profile** is a 2D cross-section; sweep it along a path
and you get a wall, a kerb, a bridge or a flight of steps; revolve it about a vertical axis and you get
a well ring, a column or a tower. Walls are just the simplest case.

### `## profiles` (and the shared `story/field/profiles.md`, loaded first)

```
## profiles
id: x y tile scale, x y tile scale, ... [closed]
```

`x` is sideways in the plane perpendicular to the path (a **radius** for a lathe), `y` is up, and the
tile and scale belong to the segment **starting** at that point. `closed` joins the last point to the
first. `story/field/profiles.md` is loaded before every map, so cross-sections can be shared.

**Built-ins**, usable inline wherever a profile id is expected — they are cached under their spec:

| spec | shape |
|---|---|
| `box:w:h[:side:top]` | a rectangle: what a wall is |
| `slab:w:h[:tile]` | the same, read as a tread or a plinth |
| `kerb:w:h[:tile]` | a chamfered kerb |
| `bridge:w:h[:deck:parapet]` | a deck with a parapet each side |
| `stair:rise:run:n[:tile]` | a staircase **cross-section**: n steps rising sideways, for a stair swept along its width |
| `ring:radius:h:thickness[:tile]` | an annulus, for lathes |

### `## sweeps`

```
## sweeps
id profile [spline] [caps] [along S] [scale s0 s1]  x z [h], x z [h], ...
```

- The path is in map units. A third number on a point is an **explicit height**; without one the base
  follows the terrain, so a wall climbs the hill and a stair or a bridge can be told exactly where to be.
- `spline` runs a Catmull-Rom through the points, resampled every ~0.5 cells.
- `caps` closes the ends with the profile polygon (a fan from the profile's centroid — profiles are
  small and star-shaped; a genuinely concave cap would need ear clipping).
- `along` scales the texture repeat along the path (1 = one tile per cell); `scale s0 s1` lerps the
  whole profile's size from the start of the path to the end.
- Joins are **mitred**: the profile's `x` is stretched by 1/cos of the half-angle, clamped at sharp
  corners. Light is baked per face from that face's own normal. Sweeps are see-through occluders.

### `## lathes`

```
## lathes
id profile x z [h] [segments]
```

The profile revolved about the vertical axis at `x z`, `segments` around (default 16). Used in Halm for
the well ring under the well sprite.

### Blockers

Every unshared navmesh edge still gets something standing on it; `_generate.py` now emits those as
**sweeps with a box profile**. It chains the open edges into runs (refusing to join two boundaries that
merely touch at a vertex, which would zig-zag) and offsets each run **outward by w/2**, so the walkable
edge is exactly the wall's inner face and the walker radius does the rest. An edge is left open when:

- the height grid differs across it → it is already a cliff step;
- it is inside a building footprint (+0.8) → it is already a wall;
- it is inside or within 0.7 of an **exit** rectangle → it is the way out. Halm's gate through Hart's
  terrace wall is a real gap, flanked by two short square posts (two-point box sweeps), with the road
  painted through it and off the north edge of the map.

`## walls` (`id w h tex_side tex_top [cap] [spline] x z, ...`) is kept as **sugar**: it builds a
`box:w:h:side:top` profile and one sweep. A map with no sweeps at all falls back to the engine's old
one-quad-per-edge strips.

### The three uses in the shipped maps

- **Halm's square** — the low walls round the lopsided square and the kerb round the well island are
  box sweeps; the well itself sits inside a **lathe** ring (`ring:1.05:0.42:0.22:wall_stone`, 20 segments).
- **Buildings** — every building in both maps is a `house` profile swept along its length (see below).
  *(Stone steps up the hill road were tried and removed: quantised sweep heights read as a stack of
  slabs, not a stair. The ramp is a smooth slope again, in the navmesh and on screen. The `stair`
  profile stays in the built-ins for the Stair landmark, where a proper stair cross-section swept
  along a curve is the right shape.)*
- **The stream** — `fordbridge`, a `bridge:3.20:0.22:plank:wall_stone` sweep with explicit heights so
  the deck arcs, where the ford lane crosses the water. The lane's navmesh now runs across it.

The `{{STAIR}}` landmark on `stair_shrine` is meant to be built the same way: a `stair` profile swept
along a curve with explicit heights, so the bottom step can be wider than a town and thirty feet up.

## Painted backdrops (FF8 style)

The 3D scene is the **block-out**. A zone can have a painting made over a capture of it; the game then
draws the painting and uses the block-out only for depth, so sprites layer into it correctly.

### Zone ids

`## cameras` lines take an optional **id as the first field**:

```
## cameras
base       0 0 40 30 follow 0 50 30 13 0.8
square     14.0 13.5 9.0 8.0 fixed 18.60 12.00 30.00 18.60 1.20 17.40 26
```

A line that starts with a number is still parsed as before and gets the automatic id `z0`, `z1`, …
The id is what the capture and the painting are keyed on, so zones can be reordered freely.

`ortho <half-height>` may appear anywhere after the mode as an alternative to the `fov` value: the
zone then uses an orthographic projection of that half-height in world units. Captures, painted mode
and the framing maths all work with either.

### Capture

Three ways to fire one, all equivalent:

- **Dev → Capture view 2x** (or **1x**);
- a `capture` trigger in `## triggers` (`x z w d capture 2`);
- **`capture.flag`** in the pref dir, polled 2.5x a second and truncated once consumed, exactly the way
  `reload.flag` works — so a capture can be driven from the Mac with no taps at all:

  ```bash
  printf 'square 2x' > /tmp/capture.flag
  adb push /tmp/capture.flag /data/local/tmp/capture.flag
  adb shell "run-as com.playground.sdlraylib cp /data/local/tmp/capture.flag files/capture.flag"
  ```

  Contents are optional and order-free: a **zone id** and `1x`/`2x` (default 2x, current zone). Naming a
  zone moves the player to that zone's centre on the navmesh first, so the right shot becomes active,
  and the capture waits six frames for the camera to settle. A non-empty `capture.flag` also pulls the
  game out of the title or a cutscene and into the field, so the whole thing is scriptable.

Each capture writes two files into the phone's `files/field/views/`:

| file | what |
|---|---|
| `<map>_<zone>.png` | the colour view — **this is what gets painted over** |
| `<map>_<zone>_depth.png` | linear distance from the camera, /40 world units, greyscale — reference and debug only |

The capture renders the zone's shot **exactly as authored** (framing blend forced to 0), so the
painting is made for the written camera. It is always a clean **640×360 × scale** — 1280×720 at 2x —
whatever the live `fill width` setting is, so the painter always gets 16:9. That costs nothing at
display time because **a painted zone adopts its painting's aspect**: the internal viewport is resized
to match, the painting fills it exactly, and the existing FBO→screen fit centres the result on a wider
phone. So a 16:9 painting shows with side bars on a 20:9 screen, like FF8 on a widescreen TV.

A capture never contains walkers: the player, the NPCs and the examine mark are skipped, because the
painting is a **backdrop** and they are drawn live on top of it.

`./fast_reload.sh --pull-views` copies the colour views into `story/field/views/`. **The depth PNGs are
not pulled** — they stay on the phone as reference; they are debug output, not art.

### Capturing on the Mac, with no phone at all

```bash
./capture.sh halm square 2      # -> story/field/views/halm_square.png at 1280x720
```

`capture.sh` configures and builds the desktop targets into `build_desktop/` (SDL3 comes from the same
FetchContent as the Android build), then runs the real host once with

```
FIELD_CAPTURE=<map>:<zone>:<scale>:<absolute output path>
```

The game lib reads that env var in `game_create`; on the first tick it loads the map, places the camera
at that zone **as authored**, renders the capture exactly as the Dev button does, writes the colour and
`_depth` PNGs and sets `wants_quit`. A window flashes up for a moment; that is the renderer doing its
frame. Android never sets the variable, so it is inert there and **no deploy or host change is needed**.

Two things make this work on desktop GL:

- the inline shaders carry **no `#version` line**; `make_shader` prepends
  `#version 300 es` + the `precision` qualifiers under `__ANDROID__` and `#version 330 core` otherwise.
  Nothing else in them differs between the two dialects.
- `field_read` falls back to `story/<rel>` after the pref path and the APK assets, so a desktop run from
  the repo root finds `story/field/maps/...` and the art without a staging step.

The Mac and phone captures of the same zone are identical to the eye, which is the point: the painter
works from one and the phone shows the painting back.

### Painted mode

If `story/field/views/<map>_<zone>_paint.png` exists (phone `files/` first, then the APK assets), that
zone is painted. Draw order per frame:

1. the painting, as a full-viewport quad with **depth test and depth writes off** — a backdrop;
2. the whole block-out (splat ground, water, walls, sweeps, buildings, prop billboards) with
   **colour writes off and depth writes on** — it contributes nothing but depth;
3. walkers, NPCs and the examine mark, depth-tested as usual, so they sit correctly in the painting;
4. the see-through cut **reverses**: a painting's pixels cannot be dithered away, so where the
   block-out hides the player the player is drawn a second time with `GL_GREATER` depth and no depth
   write, as a dithered silhouette — which is what the FF games do.

Rules and fallbacks:

- **A painted zone must be `fixed`.** A painting is one camera. A painting found on a follow or rail
  zone logs `field: … is a painting but zone <id> is not a fixed shot, ignoring it` and is skipped.
- **A painted camera never moves.** The framing blend and the hard clamp are disabled in a painted
  zone or the painting would slide out of register; if the player walks off frame it only logs
  `camera: player off frame in painted zone <id> …`.
- The painting is resampled to the viewport with **nearest**; a different aspect is **letterboxed**
  inside the FBO and logged once.
- A zone with no painting stays live 3D, so the two can be mixed freely in one map.

---

## Ground — the splat map

Per-cell tile ids made every cell boundary read as a grid. The ground is now one pass over a **splat
map**: an RGBA image over the whole map whose four channels weight four ground textures.

```
## meta
splat: halm_splat.png 8                                  # file, texels per cell
splat_layers: grass:1 dirt:1:0.15 stone:8:0.6 plank:6:0.4  # name[:hardness[:height influence]]
tile_scale: 1 1 2 1                                      # cells one tile of that layer spans
splat_blend: dither                                      # `hard <n>` still works as a default hardness
```

- The splat is sampled **once, bilinear, in world UV** over the whole map; each layer is sampled in
  world space at its own repeat scale (stone at `2` spans two cells, so it does not repeat per cell).
  Five samples per ground pixel, no more: the height mask rides in the tile's alpha.
- Channel order is `splat_layers`. **Whatever the four channels leave unused goes to layer 0**, so a
  hand-painter can paint only the roads and leave grass as the base.
- **Per-layer hardness.** Weights are contrasted around the winner — `pow(w_i / max(w), sharpness_i)`
  — so each layer meets its neighbour at its own hardness: `grass:1` and `dirt:1` blend smoothly into
  each other while `stone:8` cuts a crisp edge against either.
- **Height-aware blending** (this is what makes the edge pixel-art rather than splat-resolution).
  Every tile texture carries a **height mask in its alpha channel**, packed at load from
  `story/field/tiles/<id>_h.png` if the pipeline supplies one, else contrast-stretched from the tile's
  own luminance. The effective weight is `w_i + (height - 0.5) * k_i`. Where the square's cobbles fade
  into dirt, the mortar and moss between the slabs lose first and the slab faces hold out longest, so
  the boundary follows the artwork one pixel at a time. `dither` breaks ties with an ordered 4×4
  pattern so it stays pixel art.
- **Water** is still a cell type of its own, drawn in a separate pass with its scrolling UV; the
  shoreline is painted into the splat as a dirt bank along the stream.
- `## ground` is now **legacy**: it still marks the water cells, and if a map has no splat PNG the
  engine rasterises one from it at load with a one-cell soft falloff, which already beats hard tiling.
- **Authoring**: `_generate.py` writes the splat from the same desire lines the streets come from —
  road cores at full dirt, stone inside the square, the grain yard and the terrace, verges fading over
  a cell, worn patches at the well and at every building's door, a bank along the stream. Run it with
  `--keep-splat` to leave a hand-painted PNG alone. The PNG lives in `story/field/maps/` and is pushed
  and bundled like any other field art, so it can be painted and hot reloaded.
- Dev: a **dither** slider, plus **hard** and **height** sliders per layer, live.

---

## Examining, and where the words come from

- `message` triggers and NPC talks **never fire on entry**. When the player stands in a message
  rectangle (or within 0.6 cells of it, facing it) or in front of an NPC, a **"!" billboard** bobs over
  the player's head and the right-half **interact tap** opens the box or starts the talk. The mark is
  the prop id `mark_examine`, so art can replace it.
- Still firing on entry, because they are transitions, not reading: **exits**, `scene` triggers,
  `zone` triggers, and the new `trap` trigger kind.
- **No prose is authored in the engine or in a map.** A `message` trigger's argument and an NPC's
  `say:` argument are **ids** into `src/field_text.h`, which `./story_prompt.py export` generates from
  `story/field/text.md`. An id with no entry shows as `[the.id]` in the dialogue box, on purpose.

Ids these two maps use, for the writer:

| id | where |
|---|---|
| `halm.well` | the well in the square (exists) |
| `halm.guild_hall_door` | the guild hall door (exists) |
| `hart_yard.practice_posts` | the post line in Hart's yard (exists) |
| `halm.marta` | villager by the square |
| `halm.ostler` | villager on the west street |
| `halm.grainwife` | villager at the grain yard gate |
| `halm.gate_watch` | villager at the hill gate |
| `hart_yard.hart` | Hart, in his yard |

---

## 3D buildings — a house profile swept along its length

```
## buildings
# x z w d h id [rot deg] [pitch p] [eave e]
22.4 12.2 5.0 3.4 3.6 guild_hall rot 60.9 pitch 0.85 eave 0.42
```

A building is no longer a box with a gable stuck on: it is the **`house` profile** — floor line, wall
up to the eave, a soffit out over the overhang, the roof pitch up to the ridge and down the other side
— swept along the building's **length**. So:

- the two long walls and the roof slopes come from the swept profile (`<id>_side.png` on the walls and
  soffits, `<id>_roof.png` on the slopes);
- the **end caps are the gable ends**. The **front** cap (the first path point, the face the default
  camera looks at) takes `<id>_front.png` stretched once over the profile's bounding box, so a painted
  door and windows land exactly where the artist put them — including up into the gable. The back cap
  takes `<id>_side.png`.
- `rot` turns the whole thing about the footprint centre; `pitch` is the roof's rise per unit run
  (default 0.55) and `eave` the overhang (default 0.18). Both are optional, so every existing
  `## buildings` line and the art contract are unchanged.
- The base is sampled once at the footprint centre, so a house stands level on a slope.
- Buildings block nothing; the navmesh already excludes them.

Silhouettes are deliberately not uniform: the guild hall has a steeper pitch and a deeper eave, and
Hart's **ladder house is an L** — two `## buildings` entries with the same id and different rotations,
which is all a wing takes.

### Face-texture contract (for the art pipeline's future `building` command)

| file | size | how it is mapped |
|---|---|---|
| `story/field/buildings/<id>_front.png` | **128×128**, opaque | stretched **once** across the whole front wall, so a painted door and windows land exactly where the artist put them whatever the building's width |
| `story/field/buildings/<id>_side.png` | **128×128**, opaque, seamless both ways | tiles **once per cell horizontally and once per world unit vertically** on the back, both ends and the gables |
| `story/field/buildings/<id>_roof.png` | **64×64**, opaque, seamless | tiles **once per cell** in both directions on each roof slope |

Ids in use: `house_a`, `house_b`, `guild_hall`, `grain_shed`, `ladder_house`. Placeholders are
generated procedurally (plaster with a painted door and two windows on the front, timber-framed
plaster on the sides, red pantiles above) until the PNGs exist. Lookup order is the phone's
`files/field/buildings/…` first, then the APK assets, then the placeholder.

---

## Art the engine loads

Exactly as FIELD.md says: `files/field/<kind>/<id>.png` on the phone first, then the APK assets, then a
procedural placeholder. `fast_reload.sh` pushes changed files under `story/field/**` into
`files/field/<kind>/`; `deploy.sh` rsyncs `story/field/` into `android/app/src/main/assets/field/` and
wipes `files/field` after install so a stale push can't shadow the APK.

| kind | size | notes |
|---|---|---|
| `tiles/<id>.png` | 64×64 opaque, seamless | ground and vertical faces. Mipmapped, `GL_REPEAT`, nearest mag. Ground: `grass dirt stone plank water`. Cliff faces: `cliff wall_plaster wall_timber`. **Wall boxes (new): `wall_stone`, `wall_stone_top`, `fence_wood`, `fence_top`, `hedge`, `hedge_top`** — the `_top` ones are seen from above and tile along the wall's length; the others are seen front-on. |
| `tiles/<id>_h.png` | 64×64, optional | height mask for a ground layer; if absent it is derived from the tile's own luminance. See "Ground splat". |
| `props/<id>.png` | any, **alpha** | 1 px = 1 internal px at 64 px/cell. Bottom opaque row is the ground line, horizontal centre the anchor. |
| `walkers/<id>.png` | 4 rows (S,W,E,N) × 4 cols of 32×48 | `falke ottilie hart stolz villager_a villager_b clerk` |
| `edges/<kind>.png` | 64×64 **alpha**, tiles horizontally | legacy fallback strips only: `fence hedge wall` |
| `buildings/<id>_{front,side,roof}.png` | see table above | new |
| `maps/<name>.map` | text | pushed and bundled the same way |

> **Note for the art pipeline.** The props currently on the phone render as opaque black rectangles:
> their PNGs have no transparency. Props, walkers and edge strips are alpha-tested at 0.5, so a fully
> opaque sprite draws its whole bounding box. Tiles are the only kind meant to be opaque.

---

## Controls (touch, landscape)

- **Left half**: a floating virtual joystick appears wherever the thumb lands. 8-direction snap, dead
  zone ≈ 2.2 % of the screen width.
- **Right half**: tap (< 0.35 s) = action/confirm; hold > 0.35 s = **run** (×1.75) while held. A thin
  ring marks the pad; it fills while touched and shows `RUN`.
- Both fingers work at once. The field reads **SDL touch fingers directly**
  (`SDL_GetTouchDevices` / `SDL_GetTouchFingers`), not ImGui's mouse, because the ImGui SDL3 backend
  only ever sees the primary touch. With no touch panel the mouse stands in for one finger.
- **The stick is in screen space, always.** "Up" walks away from the camera along the ground projection
  of the view direction, "right" is camera-right, in every camera mode — a 90° zone yaw or a fixed shot
  never inverts the controls. Facing is stored in world terms and converted to the right S/W/E/N sprite
  row for the current camera.
- Input is suppressed while the Dev panel is open. The action button dismisses a message box if one is
  up, otherwise talks to the nearest NPC within 1.8 units in front.

---

## Dev overlay

`Dev` (top right) → the panel, which shows, when the field is on screen:

- `map <name>  x <x> z <z>  poly <id>  zone <n>  facing <S|W|E|N>` and the map's `landmark:` line
- map buttons **halm** / **hart_yard**, **Respawn**
- **Nav** — the navmesh as a coloured wireframe with polygon ids projected on top (open edges darker)
- **Blockers** — a red line along every unshared edge
- **fill width** — off renders a strict 640×360 pillarboxed; on (default) widens the internal buffer to
  the screen aspect at a fixed 360 height (800×360 on the phone), so no black bars
- `zone <n> mode <follow|fixed|rail>` and **print zone**
- live sliders for the current zone; **see-through occluders** + **radius px**; **walk radius**

Camera clamp warnings and `encounter <table>` from zone triggers arrive in the **Messages** list.

---

## Map design — how Halm and Hart's yard are built

Both maps are generated by a small authoring script (desire lines → junction hulls → offset ribbons)
rather than hand-typed, because every shared navmesh edge has to be an *exact* vertex match while
nothing is axis-aligned. The method, for the next map:

1. Mark the **origin point** (Halm: the well at 18.6, 17.4).
2. Place **junction nodes** and, for each, the directions and mouth widths of the streets leaving it.
   The junction polygon is the convex hull of the streets' mouth points, so each street's mouth is an
   exact hull edge. The well square is that hull turned into a **ring** around a central island, which
   is why the well itself is solid ground you walk around.
3. Draw each street as a **ribbon**: a bending centre line with a half-width per point, offset to left
   and right polylines, one convex quad per segment. Consecutive quads share a whole edge; the first
   and last edges are pinned to the junction mouths (oriented to the ribbon's own left/right, or the
   end quad self-intersects).
4. Terrain (hill, stream) comes before buildings; the ground tiles are painted **from the navmesh**, so
   the roads follow the bends exactly and grass fills the rest.
5. Buildings last, fronts to the path (`rot` computed from the direction to the street), with checks
   that no building overlaps a walkable polygon and no two neighbours within 9 units are within 8° of
   parallel.

**Street widths (binding for the next map): streets are 3–4 cells across, lanes never under 2.**
In Halm the mouth half-widths are 1.50/1.45/1.35 for the three main streets (3.0–3.0 cells), 1.05 for
the one deliberate pinch (2.1 cells, still comfortable), and 0.95–1.24 for the outer lanes.

---

## Deviations from FIELD.md, and why

1. **Camera yaw.** FIELD.md says the camera looks along +Z. It looks along **−Z** (GL's default), so map
   row 0 is the far/north edge and a `.map` file reads with north at the top. A zone's `yaw` rotates
   from there.
2. **Internal resolution.** 640×360 is the base, but **fill width** (default on) widens the buffer to
   the screen's aspect at a fixed 360 height rather than pillarboxing a 20:9 phone with 240 px of black
   each side. Turn it off in the Dev panel for the strict 640×360.
3. **Billboards are full camera-facing quads**, not Y-only. A Y-only billboard under a 50° pitch splays
   badly at the edges of frame (world-up lines diverge) — the first build looked like the houses were
   falling over. Leaning back along the camera's up vector also lifts a sprite clear of the ground
   behind it, so the depth test alone is enough: no sorting, no `1/cos(pitch)` stretch.
4. **`## props`' `solid` / `walk` flag is gone** (parsed and ignored). Blocking is the navmesh's job.
5. **`## npcs` gained an additive `say:` form**: `x z walker facing Name say:<line>`. A bare trailing
   token is still a scene id, exactly as FIELD.md's example.
6. **New sections**: `## nav`, `## cameras`, `## buildings`; new `## meta` keys `edges:` and `landmark:`.
7. **Exits and triggers fire on the rising edge**, so a spawn inside its own door rectangle doesn't
   bounce the player straight back.
8. Nav vertex heights are **half-steps**, matching `## height`; building heights are **world units**
   (they aren't terrain).
10. `## ground` is legacy; the ground is a splat map (see "Ground — the splat map"), and blockers are
    extruded boxes rather than FIELD.md's unstated strips.
11. `src/field.cpp` is ~2700 lines rather than the ~1200 the brief asked for; the navmesh, camera zones,
   buildings, blockers and see-through all arrived after that budget was set.

---

## Still stubbed / open

- `zone` triggers only log `encounter <table>` to the Dev messages. Battle is the next stream.
- `trap` triggers fire their text on entry and do nothing else yet.
- **The player draws as a flat coloured block**, because `story/field/walkers/falke.png` on the device
  is a 631-byte stub: a solid rectangle with transparent margins, not a 4x4 walk sheet. The engine is
  loading it correctly — delete it and the procedural placeholder figure comes back, or ship the real
  sheet. (The "!" mark and the interact press work correctly on top of it, which is how it was
  identified.) The engine rejects sprites with **no** transparency outright and logs
  `field: <kind>/<id>.png has no transparency, ignoring it`; this one has margins, so it passes.
- Ground tile PNGs on the device currently read cyan/pink; that is the pipeline's palette, not the
  engine. Every missing asset logs `field: no <kind> art for "<id>", drawing the placeholder`.
- No post-process pass yet (the FBO is drawn straight through).
- NPCs don't walk; they stand and face a fixed direction.
- One Halm NPC ("Hunter", x22 z19) is wired to the real scene `003b_after_the_warning` to prove the
  field → cutscene → field round trip. Placeholder wiring, not a story decision.
- Talking to an NPC whose scene id isn't in the playlist shows
  `(scene "<id>" is not in the playlist yet)` in the field's dialogue box rather than failing.

---

## FIELD.md hand-off checklist

| item | answer |
|---|---|
| origin point named | **the well** (18.6, 17.4). The square is the junction it made; everything radiates from it. Hart's yard: the ladder house and the post arc. |
| desire lines drawn before buildings | Yes — the nine streets and both junctions exist in the generator before the `blds` list, and buildings take their rotation from the street they face. |
| no straight street longer than three houses | Yes. Every street bends at its junction mouth; the hill road bends twice climbing the shoulder. The longest straight run is one ribbon segment, ~3 cells. |
| no two neighbouring buildings parallel | Checked automatically: any two buildings within 9 units whose yaws are within 8° (mod 180) fail the build. None do. |
| stream and slope paths curved | The stream is a 7-point polyline crossing the east side at an angle to every street; the hill road is a curving ramp (one sloped navmesh chain), not a straight one. |
| edges ragged and the border hidden | 23 trees at uneven spacing round the border, the hill covers the north, the stream the east, the orchard lane dead-ends into the south-west corner. The exit road runs *off* the north edge rather than stopping at a fence. |
| landmark in every camera zone | Yes — table above. The depth shot is aimed at Hart's hill and the gate. |
| every building has a reason | guild hall (door trigger), grain shed (the yard and the Grainwife), ladder house (Hart's gate), well square houses (Marta, Ostler, the Hunter), the hill house (Gate Watch). 10 buildings, in the 6–12 band. |
| `landmark:` set | halm: *"the lopsided well square, and the road that climbs round the shoulder of Hart's hill"*. hart_yard: *"the six posts set in a crooked arc round the ladder house"*. Shown in the Dev overlay. |
| graph-paper test passed | Yes — checked from above on the phone with the Dev camera and in the generated `## ground` grid; no right angles, no repeated spacing, no axis-aligned street. |

---

## Proof it ran on the phone

`./fast_reload.sh`, device `192.168.1.217:5555` (final build of this pass):

```
=== Compiling star_logic.cpp ===
=== Compiling field.cpp ===
=== Linking libgame_logic.so ===
=== Pushing to device ===
  field maps/halm.map
  field maps/halm_splat.png
  field maps/hart_yard.map
  field maps/hart_yard_splat.png
=== Setting reload flag ===
=== Waiting for host to apply reload ===
09-18 13:17:54.809 29060 29113 I QuestGlory: Hot reload SUCCESS (428784 bytes, gen 1)
=== Done ===
```

Confirmed on the device afterwards: `adb shell pidof com.playground.sdlraylib` still returns the same
pid, `logcat -d | grep -cE "F DEBUG|signal [0-9]"` returns 0, and the Dev message log has had no
`camera: clamped` line since the rail-parsing fix. Screenshots through the pass show Halm rendering
with 3D rotated buildings, fenced and walled edges, the gate gap with its road running off the map,
the organic radial street plan, the player framed in every zone and kept clear of walls, walking on the
navmesh, the zone trigger posting `encounter halm_outskirts`, and an NPC playing
`003b_after_the_warning` through the cutscene player and returning to the field.
