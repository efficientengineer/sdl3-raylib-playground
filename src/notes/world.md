# The world — what a `.tmap` becomes, and what it is made of

> Split out of `src/VOXFIELD_NOTES.md` (2026-09-21) when that file passed a thousand lines. Nothing
> was rewritten; the sections are exactly as they were. `src/ENGINE.md` is the map of `src/`, and
> `src/notes/` is the long-form record behind it.

Covers: the pivot and the shape of it, shaped blocks, the shadow map, colour-plus-arithmetic
blocks, the `.tmap` format (`## meta`, `## height`, trigger kinds, the event queue, field
sprites), the house generator, trees, ramps, the terrain flood fill, and the engine-wide
**Known issues** list at the end.

# VOXFIELD_NOTES — the voxel + sprite field

The owner's pivot, 2026-09-19: *"Let's try a completely different system, a voxel and sprite approach.
World mostly rendered like Minecraft, but we have sprites for characters and detail."* The tile field
was producing too many bugs. This file is the engine's side of that decision — what is built, what
was decided and why, and what is known to be wrong.

`src/voxfield.cpp` / `src/voxfield.h` are a new, self-contained module. **`tilefield.cpp` is parked**:
it still compiles and still runs, and the Dev panel's **Old tile field** button sends `SCR_FIELD` back
to it (and **Old 3D field** still reaches `field.cpp`). `SCR_FIELD` opens the voxel field.

Nothing new has to be authored. The world is built at load from the SAME `story/field/tmaps/*.tmap`
files, the same `tiles.md` entry list, the same walker sheets, the same atlas, the same
`story/field/tilesets/<set>/decals/`, and the same `story/palette/`.

## The shape of it

**A voxel is HALF a walk cell** (the owner, 2026-09-19: *"4 blocks should be able to fit into the
smallest block size now… more detail, feel different than Minecraft"*). A WALK CELL is unchanged and
is still one world unit: the maps, the triggers, the characters, the camera framing and the 16-texel
pattern grid are all in walk cells, and 16 texels to the cell means 8 to the voxel. Everything
structural is authored in voxels, 2x2x2 to the cell.

- `blk[y][z][x]` and `shp[y][z][x]`, `uint8`, up to **192 x 192 voxels and 48 up** (96 x 96 cells,
  24 cells high). X is east, **Z is south** (the `.tmap` row index), Y is up.
- **Chunked 32x32 voxel columns** (the same 16-cell footprint as before), meshed once at load into
  one static VBO each — halm is 9 chunks, **73 810 triangles, 29 ms to mesh**. That is 9x the
  triangles of the whole-cell build for 8x the voxels; hidden-face culling is what keeps it from
  being worse. **Greedy meshing was considered and not done**: 74 k triangles costs 2.4 ms a frame
  at 1080p, the mesh happens once at load, and a greedy pass would have to reason about shapes and
  per-vertex light as well as type. Revisit it if a 96x96 map lands.
- **Frustum culling** is on: six planes out of the view-projection against each chunk's own AABB
  (computed at mesh time). halm draws 6-9 of its 9 chunks depending on where you stand.
- **Winding is derived, never typed.** Each quad's first two edges are crossed and the result checked
  against the face's own normal; if it disagrees the quad is flipped. The first build had every top
  face wound the wrong way and the ground vanished under back-face culling — this is why that can no
  longer happen for any face.
- **Light: baked contact, mapped shadow.** Each vertex carries the face term (from its REAL normal,
  so a slope is lit between the two faces it lies between), the classic 0-3 neighbour AO, and the lamp
  level at that corner. The **cast shadow is a shadow map**, not a marched column — see below.

## Shaped blocks

A parallel `shp` grid gives every voxel a shape and an orientation, `shape * 8 + orient`, so an
untouched grid is all cubes. `SH_CUBE, SH_SLOPE (4), SH_SLOPE_OUT (4), SH_SLOPE_IN (4), SH_SLAB
(bottom/top), SH_STAIRS (4), SH_POST, SH_PANE (6)`. At half scale the small ones still earn their
place — a slab is a quarter of a walk cell and makes eaves, sills, thresholds and ridges; a post is a
fence upright that grows rails toward any solid neighbour; a pane is glass, a shutter and a railing.

- The mesher is a **quad emitter**, not a six-face switch. `vx_emit_box` does the boxy shapes;
  `vx_emit_prism` does the ramp family from its four corner heights (two top triangles with their own
  normals, a trapezoid a side, a bottom).
- **Culling is by coverage, never by "is it solid".** A face may be culled only against a neighbour
  whose shape FILLS the shared boundary (`shape_covers`). When in doubt it says no and the face is
  drawn — that is why a shaped neighbour can never punch a hole in the world.
- **Winding is still derived**, per quad, against the quad's own normal.
- **AO is generalised**: the three neighbours at the corner, sampled in the plane just outside the
  surface along the real normal. Only a full opaque CUBE occludes, so the corner under an eave does
  not go black.
- **Patterns follow the surface.** Each vertex carries its own `(u,w)` in world units, computed at
  mesh time. The six axis faces keep exactly the mapping they had, so nothing that already looked
  right moved; a sloped face gets a basis whose `u` runs along the horizontal edge and whose `w` is
  ARC LENGTH up the pitch, so a roof course is the same width on the slope as on the flat.

## The shadow map

The owner, this round: the marched column shadow was per-block, fully on or fully off. It is gone.

- **The world is static, so its shadow is too**: one depth-only render of the same chunk VBOs from the
  sun's orthographic view, fitted to the map, at map load and again only when the sun moves. 2048x2048
  where `GL_MAX_TEXTURE_SIZE` allows it, 1024 otherwise. **2.3-3.1 ms, once.**
- Sampled as a `sampler2DShadow` with hardware PCF, **four rotated-poisson taps, unrolled** — this
  repo has been bitten by a driver miscompiling a GLSL loop and the habit stays.
- Front faces culled plus a slope-scaled polygon offset plus a normal offset in the vertex shader: no
  acne and no peter-panning on voxels this small.
- The result feeds the **LIGHT LEVEL** the colormap is looked up at, not a multiply toward black, so
  shade is a cool palette colour and not a dark smear (PALETTE.md / D19).
- A face turned away from the sun (`dot(n, sun) <= 0.02`) skips the lookup: the baked face term
  already has it, and the test there is unreliable.
- **Snapped** (Dev toggle) samples at the world position quantised to the 1/16 texel grid, so the
  shadow edge is a staircase on the same grid the patterns are drawn on. **Default is off (smooth)**:
  in captures the soft edge read better against the AO, and snapping fought the PCF. Both are one
  checkbox apart.
- **Sprites sample the same map** a little above their feet, so a character walking under a roof or a
  tree darkens with it. They do NOT cast into it — a dynamic pass for four walkers was not worth a
  second depth render on the phone. The blob shadow stays. Say so rather than pretending otherwise.
- Per light table: **day** azimuth 312, elevation 38 (from the south-west, so shadows fall up and to
  the right and the camera sees them); **dusk** 296 / 16, long and low; **night** a faint moon at 0.28
  strength. `vx_sun_defaults` sets them at load and the Dev panel overrides.

## Blocks are palette colours plus arithmetic — no textures at all

`BLOCKS[]` gives each type two **master-palette ramps** (`story/palette/master.json`, by name) and a
pattern. Six steps of each ramp are baked into a 16 x N `GL_R8` **LUT texture**; the fragment shader
quantises the world position to **1/16 of a block**, runs the pattern to a 0..1 step, fetches that
step's palette index from the LUT, and looks the index up in `colormap.png` at the row for the
current light. So the world is 16-px-per-block pixel art that shares one palette with every sprite,
and night, dusk and a lamp pool are a row change (PALETTE.md / D19).

Types: grass, dry grass, dirt, mud, gravel, paving, stone, plaster, timber, plank, roof tile, thatch,
water, water bed, crop, leaves, trunk, door, window, hedge. Patterns: mottle, grass (with a lip of
turf over the top of the dirt sides), cobble (a warped grid with dark mortar and a lit lip), planks,
roof courses, water, thatch, crop rows, leaves, bark, dressed stone, plaster.

**Never paste a colour in here.** A new block type names a ramp and a fraction along it.

## What the .tmap becomes

- **Ground terrain id → the column's top voxel**; below it dirt then stone. The top voxel's MATERIAL
  is chosen per voxel and may be borrowed from a neighbouring cell on a hash, so grass, dirt and
  paving meet on a **ragged line instead of a cell edge**. Height stays per cell, so walking is
  unaffected. This is what item 7's "soften the square boundaries" turned into, and it needed no
  shader work at all.
- **Water is a channel with a bed.** Three voxels down to the bed, the surface 1.2 voxels below the
  land, wet stone for the banks, and **wet-stone shoulders pushed out into the water on a hash** for an
  irregular shoreline. A **lighter shallows band** near the bank and a **dark band under the north
  (camera-facing) bank** come from a distance-to-bank baked into the vertex's spare byte.
- **Buildings come from the rule-based generator** below.
- **Tree stamps** are one of three seeded silhouettes.
- **Fences, walls and hedges** are one block high; posts two.
- **Everything else stays a billboard** of its own atlas cells, standing on the map. That is the safe
  default — the well, the handcart, the market stall, barrels, crates, the trough. Nobody has to model
  anything for a map to work.
- **Triggers, spawn, NPCs and `lamp:` meta carry over unchanged**, as does `light: <table> <level>`.

### `## height` — terrain the map authors (chapter one)

Terrain height used to be entirely procedural (`vx_vnoise` in `vx_build_world`), which is fine for a
town square and no use at all for a hillside. A `.tmap` may now draw one.

```
## meta
base: 4                     # optional: the map's floor, in VOXELS. 4 is the old H_FLAT default.
## height
ccccccdeghkkkkkjhgfccccc    # one char a cell, the same grid shape as `## ground`
occcccceghikkkkkggdooooo
...
```

- A character is that cell's surface height in **VOXELS above the map base**, in **base 36**:
  `0`-`9` = 0..9, `a`-`z` = 10..35. A voxel is half a walk cell, so `c` is three walk cells up.
- `.` or a space is **unauthored**: that cell keeps today's procedural height. **A map with no
  `## height` section at all behaves exactly as it always did** — that is the compatibility rule that
  matters, and halm, hart_yard and west_road have no grid.
- `~` is a **bend**: the mean of its authored orthogonal neighbours. Nothing uses it yet; it exists so
  a map can meet two shelves without picking a side.
- An authored cell is **authoritative**. The noise does not touch it, it is **exempt from the
  two-voxel neighbour clamp** (that exemption is the point — a cliff has to be allowed to break the
  clamp, and hill_path's hidden find is a deliberate 4-voxel drop the clamp would have smoothed
  away), and `vx_verify_reach` will **not flatten it**.
- **Stamps and trigger rectangles flatten to their top-left cell's height**, so a cart never straddles
  a step and a trigger's floor is level under the whole rectangle.
- **Walk connectivity is unchanged.** `vx_can_step` still says one voxel of rise is a free step and
  two is a hop; more is a ledge. Walking off a ledge is a fall, never a wall — which is exactly what
  "a two-cell drop is a drop" means.
- **`vx_verify_reach` only reports on a map with a height grid.** Its reference flood is the flat tile
  map, which has no idea there is a hill, so on hill_path it would call the shelf above every
  switchback unreachable and flatten the lot. On such a map it logs the count and returns 0; the real
  test that the story can still be played is **`vx_build_nav`'s target check (`navreach`)**, which
  walks to every exit, door, message and NPC. high_pasture's `navreach=FAIL` is the loot shelf, which
  is *deliberately* jump-only.
- `VX_VY` went from 48 to **64 voxels** so a map can climb. `VX_HMAX` is the parser's ceiling.

### New trigger kinds

Trigger lines are `x z w d <kind> [args]`, as before.

| kind | args | fires | how |
|---|---|---|---|
| `fight` | `<encounter_id>` | `VXE_FIGHT` (`arg` = id) | **walk-in**, like `zone`. Also draws the encounter's field sprite on the trigger. |
| `pickup` | `<item_id> <text_id>` | `VXE_PICKUP` (`arg` = item, `arg2` = text) | **interact**: the `!` prompt and a button press, like `message`. Shows `<text_id>` in the field box first, then reports. **Takeable once.** |
| `goal` | `<Words_with_underscores>` | `VXE_GOAL` (`arg` = the line, underscores now spaces) | **walk-in** |
| `sprite` | `<sprite_id>` | nothing | draws a field sprite on the cell and no more |

**`nightsight`** is a bare trailing word on **any** trigger line:

```
12 8 1 1 pickup lens halm.lens_shelf nightsight
```

The trigger is hidden and non-interactive — and its sprite is not drawn — until
`vx_set_night_sight(v, >0)`. (`nightsight:` and `nightsight: yes` are accepted too; the bare word is
canonical.) It is stripped before the kind counts its arguments, so nothing else had to change.

### Events, and why there is a queue

`struct VxEvent { int kind; char arg[64], arg2[64]; }`. `fire()` used to keep only the FIRST event a
tick produced and throw the rest away. A pickup produces two — the text it showed and the item it
gave — and the chapter gates on both, so losing one breaks the chapter. Events now go on a
**fixed-size ring in `VoxField` (`VX_EVQ` = 16), drained one a tick**, no allocation, and a full
queue is logged rather than silently dropped.

- **`VXE_TEXT`** fires for **every** story text id the field shows — a `message`, an `npc` line, a
  `trap`. That is how a mandatory examine is observed: the field just draws the box as it always did.
  `vx_say_id` (the chapter asking for a line) does **not** fire it; the field reports what the PLAYER
  did, and echoing the chapter's own line back at it would gate on nothing.
- **`VXE_MAP`** fires when an exit or a door **completes** a map change, `arg` = the map just loaded.
  `vx_goto` does not fire it: the chapter put the party there and already knows.
- A pickup's **box opens first and unconditionally**, before anything is reported, so the player's
  feedback never depends on an event surviving.

### Field sprites — `story/field/sprites/<id>.png`

- An **indexed PNG on the master palette**, index 0 transparent, anchored **bottom-centre**, an
  upright camera-facing billboard, **64 px to the map cell**. A single still by default. **Not a
  walker sheet**: there are no direction rows.
- Optional sidecar `story/field/sprites/<id>.json`:
  `{ "frames": N, "frame": [w,h], "sheet": [w,h], "footprint": [w,h], "fps": 3, "loop": "pingpong"|"loop" }`.
  **`frame` is READ, never derived by dividing.** `footprint` is the billboard in map cells; without
  it the size is the frame at 64 px to the cell.
- Loading is **by file-exists and cached per id including the miss**, so a missing PNG costs one
  failed read a map load. When the PNG is not there the game draws a **placeholder**: five stacked,
  tapering, flat palette-ramp bands of the right footprint, which reads as "a thing stands here and
  it is not art yet" from across the map. The real art is picked up automatically the next time the
  map loads after the file appears. The Dev panel lists every sprite id the map uses and marks the
  placeholders with `*` — that is the id label, in the panel rather than painted into the world,
  where a 3D label would cost a font atlas and a pass of its own for a debugging aid.
- Chapter one's ids: `lid, burr, false_lantern, fleece, klee, sleeping_animal, machine_post,
  machine_arm, machine_swing`.

## The house generator (no wave function collapse)

A short list of rules in a fixed order, seeded per building, all of it in voxels — which is the whole
point of the half-size grid, because at the old scale a sill or a door recess was a whole walk cell.

1. a **stone foundation course** under the whole footprint;
2. **walls** six voxels (seven on a big plan, eight on a grand one), with **timber framing** for the
   plaster and plank styles: corner posts, a post every three voxels, a mid rail and a top plate;
3. a **jettied upper storey** on a wide plan, seeded — built BEFORE the roof, because a roof laid on
   the original footprint over jettied walls leaves an open tray;
4. the **roof**, gable along the long axis or **hipped** when the plan is nearly square;
5. **gable end walls** filled to the roofline;
6. a **door**: a one-voxel recess with the leaf set back, a timber lintel, a threshold slab inside;
7. **windows on a rhythm**: a recessed pane, a timber surround, a slate sill that oversails — and the
   pane is baked `warm = 0.9`, so at dusk and night it is lit from inside and the bloom catches it;
8. a **chimney** on a seeded spot, sized to clear its own pitch;
9. a **porch** on a grand building, hung on **brackets** — a porch that reached the ground would close
   the lane to its own door;
10. an **L-wing**, only where the tile map already says those cells are solid.

`guild_hall`, `barn` and anything 4x4 cells or bigger come out **grand**: taller walls, two rows of
windows, a porch.

**The roof is a HEIGHT MAP, not a shell.** For every column the surface level is decided first —
distance to the nearer eave for a gable, the lesser of the two distances for a hip, the two tying at a
corner giving `SH_SLOPE_OUT` — then the column is filled solid up to it and only the top voxel gets
the slope. This is the one thing to not undo: a hollow shell of 45-degree slopes read as a HOLE at a
42-degree camera, and three separate attempts at the course-by-course version all had it.

**Everything a house grows must clear head height.** `vx_can_stand` wants three voxels (1.5 cells)
wholly clear and will allow only a **top slab** in the fourth; a sill, an eave or a rail may oversail
a lane, a post or a step may not. Breaking that rule is what made the first build fail its own reach
check, and it is the rule to check first when it fails again.

## Trees

Three seeded silhouettes — a **round broadleaf**, a **stepped conifer**, a **small orchard tree** —
with a 2x2-voxel (one cell) trunk. At half scale the crowns are genuinely round rather than blobby.
A `vx_round_shell` pass turns every shell voxel with air above and air on exactly one side into a
slope leaning that way, and a two-sided one into an outer corner; it is also what softens a hedge.
A crown may oversail a walking cell but **never below four voxels**, the same number `vx_can_stand`
enforces, and the cutaway takes it away when the party walks under it.

## Ramps

A slope or stairs cell is walkable from its low and high ends only, and the walker's height while
crossing is the slope's own plane at the fractional position — **no hop** (`vx_surface_v`,
`vx_actor_y`). The flood fill knows the rule. In terrain generation, a walkable cell that steps **two
voxels** up to exactly one neighbour and is level with the one opposite becomes a 45-degree ramp
instead of a lip: halm has 2, west_road 13. One voxel of rise is a free smooth step; two is the hop
one old block used to be.

### Terrain height, and the flood fill that keeps it honest

Grass, dry grass and crop get a deterministic two-octave height in **half steps, 0..4 voxels**.
Everything else is **forced flat**: paths, paving, water, every stamp footprint, every trigger
rectangle, and one cell of margin round all of it. Then ten passes clamp every neighbour to within two
voxels. Finally **two flood fills from spawn** — one under the `.tmap`'s own rules, one
under the voxel rules — are compared, and any cell the tile map can reach but the voxel world cannot
is flattened and the check rerun, up to six rounds. The result is in the `SELFCHECK vox:` line as
`reach=ok` or `reach=FAIL`. On halm it passes first time.

## Known issues, in the order they matter

1. **~~Still not on the phone.~~ It is on the phone and measured** (2026-09-19). Adreno 650,
   2400x1080, 60 Hz, halm: 60 fps once the `glFinish` came out, GPU ~9-10 ms of a 16.7 ms budget. The
   shadow taps turned out **not** to be the thing to fear — `shadows off` is worth only 0.47 ms and
   `shadow pcf 1` 0.53 ms. `world opaque` is the frame, at 6.5-7.3 ms, and resolution is the only big
   lever (res 66% is -4.5 ms). See "Performance" above and `build_desktop/perf/`.
   **The remaining problem is not the average, it is the 1% worst**: the wall clock's p99 sits at
   33-45 ms against an average of 17, so roughly one frame in a hundred misses vsync outright. That is
   what still reads as "slow" and it is the next thing to chase — the frame-interval histogram in the
   HUD's full mode and the vsync buckets in the report are the tools for it.
2. **The map edge is a visible diorama cliff** where the world stops against the sky. The fog only
   fades by view distance, so an edge eight cells from the party is sharp. A one-cell skirt of ground
   dropped to the bottom of the world, or a ground plane under the whole map, would fix it.
3. **Sprite decoration on structures is not done** (brief item 4). Lamps still get their additive glow
   billboard and props still come from the atlas, but there are no wall-aligned signs, shutters, ivy
   or flower boxes. The window frames, sills and recesses are real geometry instead, which is where
   the budget went.
4. **Sprites do not cast into the shadow map**, only receive from it. Blob shadows stay.
5. **Chimney smoke is not animated** — the stack is there, the billboard puff is not.
6. **Doors read as a dark recess** rather than a leaf, because the leaf is one voxel behind the wall
   plane and in shade. It is charming at dusk and flat at noon.
7. **Particles (motes, fireflies) are not implemented.** `motes` rides the save blob unused.
8. **A running jump crosses about 3.6 cells** at the default speed and apex, which is far enough to
   clear most of Halm's fences (they are one block, and the apex is 1.25 cells). That is the price of
   the owner's apex number; the levers are the `gravity` and `jump apex` sliders, or making fences
   three voxels. Nothing in the maps depends on a fence being unjumpable yet.
9. **Jump-only regions are found but nothing uses them.** halm has 60 nav voxels reachable only by
   dropping or jumping; the count and sizes are logged for the hidden-loot pass that has not been
   written.
10. **NPC separation is a circle, not a shape.** A wide NPC standing in a one-cell doorway still
   yields, because the push-out moves them 65% and the player 35%, but they can be nudged a little
   out of their wander radius while it happens.
11. **`map.flag` is now polled in one place only** — `vx_tick`, before the default load, with the first
   tick polling immediately — so a flagged map loads once. star_logic's title poll only opens the
   field; it does not consume the flag. (Fixed this round.)
12. **`hill_path`'s nav has 13 jump-only regions and 339 jump-only voxels.** That is the authored
   hillside doing its job (a switchback shelf you can drop off but not climb back up), and
   `navreach` is `ok` — every exit, door, message and NPC is walk-reachable from spawn. It is listed
   here so nobody reads the region count as a regression.
13. **`high_pasture` reports `navreach=FAIL`.** One target — the loot shelf, 188 nav voxels across a
   three-cell gap — is deliberately reachable only by a running jump. `vx_selftest` does not fail on
   `navreach`, only on `reach_missing`, so this is a note and not a broken map. If a real target ever
   goes unreachable there it will look identical; check the logged region sizes.
14. **The walktest's "stuck" probe now walks out from the body**, four samples from `agent_r` to one
   cell, each tested at the body's own height. The old single plan-space sample a cell ahead read a
   four-voxel hillside as open ground and reported three false stucks on `hill_path`. Flat maps are
   unaffected (halm, hart_yard and west_road still report `stuck=0`).
15. **`~` in `## height` is parsed and nothing authors it.** It resolves to the mean of its authored
   orthogonal neighbours and is then treated as an ordinary authored cell; it does not build a
   sloped surface the way a `## objects` ramp stamp does.
16. **Field sprites are stills unless a sidecar says otherwise, and nothing animates yet** — no
   `story/field/sprites/` directory exists at all, so every one of chapter one's nine ids currently
   draws the placeholder. The frame-picking and ping-pong code is written but has never run against
   a real sheet.
