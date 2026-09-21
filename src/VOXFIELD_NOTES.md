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

### What the chapter script drives (`src/chapter01.h`)

`voxfield.h`'s bottom block. The field owns the world; the chapter owns the story; neither knows what
the other's nouns are.

| call | what it does |
|---|---|
| `vx_current_map` | the map the party is standing on |
| `vx_goto(map, x, z, facing)` | load and place. Triggers the party is standing in are marked already-entered, so a teleport never fires one. |
| `vx_set_light(table, level)` | re-points the colormap row, re-runs `vx_sun_defaults` for that table and **re-renders the shadow map**. A step-change call, not a per-frame one. |
| `vx_set_party_lamp(radius, level, on)` | the lantern that follows the leader — see below |
| `vx_set_night_sight(add)` | adds to the **effective ambient** (one place: `vx_set_common`), and makes `nightsight` triggers live |
| `vx_set_party(count)` | how many are following |
| `vx_disable_trigger(arg_id)` | matches on `arg` **or** `arg2`; cleared by a map load |
| `vx_say_id(id)` | a `story/field/text.md` id in the field's own box, without reporting it back |
| `vx_busy()` | a box is open, or a map change is fading |
| `vx_freeze(on)` | input ignored and the party stops walking; the world keeps rendering |

**The dynamic party lamp** is the only real rendering work here. The `.tmap`'s static `lamp:` lines
are **baked per-vertex into `VxVert.warm` at mesh time**, so a moving light cannot use that path at
all. Instead there is **exactly one** dynamic point light, as shader uniforms:

- `uniform vec4 u_dlamp` (world xyz, radius) and `uniform float u_dlev` in the world fragment shader,
  added to the warm term right where the baked `v_light.y` is read. It is a **uniform branch** — one
  value for the whole draw — so no wavefront diverges on it and the discard-free shader split is
  untouched.
- On the CPU, `vx_dyn_lamp_at` adds the same falloff inside `vx_light_at_foot`, so sprites light with
  it too. It is deliberately **not** in `vx_lamp_at`, which is what the mesh bake calls.
- It is looked up in the **`lamp` colormap row (`cmap_lamp_row0`)** exactly as the baked lamps are, so
  the lantern pool stays warm inside a blue night.
- It rides the leader, updated once a tick before the world draw reads the uniform.

Two capture-only switches, so the look can be judged before `chapter01` exists:
`VOX_LAMP="radius,level"` is `vx_set_party_lamp`, `VOX_NIGHTSIGHT=<add>` is `vx_set_night_sight`.
Neither is a setting anybody is meant to find.

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

## Movement — a generated navmesh, free 8-way walking, and jumping

The owner, 2026-09-20: *"we need to use a navmesh for movement. We could start with the movement we
have now as a basis, but I want 8 directional movement instead of the 4 we have now, and jumping"*,
then *"Fully free is better for touch."* And the standing reason for a navmesh at all: *"lighter and
more bug resistant than relying on real 3D movement with colliders."*

**The party is a point with a radius.** Grid walking, the turn hold and the one-tap-to-turn are gone
from this field (they stay in the parked tile field). `VxActor` carries a float `x`/`z`/`y`; `tx`/`tz`
are `floor()` of them and exist only so the trigger grid, the Dev readout and the save keep working.

### The nav data, generated at load

No hand authoring: `vx_build_nav` reads the voxel grid the map builder just made. Resolution is the
VOXEL — half a walk cell, 2x2 to the cell — which is what lets a fence post block half a cell without
the whole cell going solid.

A nav voxel is walkable when its cell is walkable in the tile map, the voxel column's top is that
cell's own walking surface (nothing built on it), that top face is something you can stand on, and
**four voxels of headroom** are clear — the same rule `vx_can_stand` enforces for the houses, applied
per voxel. `nav_h` is the surface height at the voxel's centre, read from `vx_surface_v`, so a ramp is
sampled twice across and comes out as a continuous plane.

Connectivity is by height: **one voxel or less is a walk edge, more is a LEDGE.** A ledge may be
dropped off or jumped off; it may not be walked up. That test is made live against the body's own
height rather than baked, because after a jump the body can be anywhere.

- `nav_ok`, `nav_h`, `nav_reg`, `nav_clr` — one byte or float a nav voxel, about 250 KB a map.
- `nav_reg` is the connected components under WALK connectivity, seeded at the spawn so **region 1 is
  everything you can walk to**. Everything else is a **jump-only region** — which is where hidden
  loot goes later; the count and the sizes are in the `SELFCHECK vox: nav` line.
- `nav_clr` is a chamfer distance transform, for the overlay and the corridor-width log only.
- **Build time: halm 0.77 ms, hart_yard 0.20 ms, west_road 0.33 ms.** Once, at map load.

### Erosion, and why the radius is 0.24

The clipping bug from the old 3D field ("the player clips into fences and walls") is fixed by
**erosion by the agent radius**, done exactly and continuously rather than as a second grid: the body
is a circle and every move is resolved out of the square of any blocked nav voxel it overlaps. That
IS the Minkowski erosion, with no quantisation, so the centre can never come closer than `agent_r` to
a wall, a fence, water, or a ledge that steps up.

`VX_AGENT_R` is **0.24 walk cells**, and the number is a proof, not a taste:

> A nav voxel is 0.5 cells across, so its centre is 0.25 cells from each of its own edges. With the
> radius **under** 0.25 the circle at any free voxel's centre can never reach a blocked voxel, and the
> straight segment between two 4-adjacent free centres runs down the middle of a 0.5-wide free strip.
> So **walk connectivity on the nav grid always survives erosion**: the reachability proof and the
> collision can never disagree, and no corridor the tile map considers walkable can be closed by the
> radius.

Raise the default past 0.24 and that guarantee goes, and the widening path below starts to matter.
The Dev slider goes to 0.45 so the owner can see what that looks like; it re-derives the nav data on
release. On all three maps the widening pass has never had to run (`widened=0`).

### The move itself

- desired velocity from the input, smoothed with a 0.06 s time constant on the ground (a JRPG wants
  the character to start when you push), the same constant divided by the air-control fraction in the
  air;
- **sub-stepped** so no single step exceeds half a nav voxel — the body cannot tunnel through a
  one-voxel fence however large `dt` is (and `dt` is clamped to 0.1 s so a hitch is not a teleport);
- each sub-step resolved by up to 3 depenetration passes, deepest blocker first, which is what makes
  an inside corner converge instead of oscillating. Sliding falls out of it: the push is along the
  square's normal, so the tangential motion survives.
- **speeds are equal in every direction** — a keyboard diagonal is normalised.
- **Every spawn, teleport and door arrival SNAPS** to the nearest valid nav point by a ring search
  (`vx_nav_snap`) and logs when it had to move more than half a cell. That is the second old bug
  ("not landing on a walkable poly at spawn") closed by construction.
- **The invariant**: a frame may not end with the centre outside the region. If one does it is a bug,
  so it logs `BUG — the leader ended a frame off the navmesh` **once**, snaps back and carries on.
  The walk test asserts this never happens.

### Jumping

No physics engine, five numbers.

| | default | why |
|---|---|---|
| `sp_walk` | 4.4 cells/s | a shade under the old grid walk, which was 1/0.16 s = 6.25 and read as a sprint once movement went free |
| `sp_run` | 7.2 cells/s | the old run, 1/0.10 s = 10, likewise pulled back |
| `jump_apex` | 1.25 cells | the owner's number: clears a 2-voxel (one old block) step with room to see it |
| `gravity` | 40 cells/s² | tight. Apex 1.25 at g 40 is `v0` 10 and **0.50 s of air**, so a running jump crosses about **3.6 cells** — well past the 1.5 the brief asked for, and the arc still snaps |
| `agent_r` | 0.24 cells | see above |

Coyote time 80 ms, jump buffering 100 ms, air control 40% of the ground acceleration. All five are Dev
sliders and all five ride the reload blob.

While airborne the body is **off the navmesh**: the same circle is tested against voxel SOLIDS
instead, any column with something in the band from the feet to head height, with the same sliding.
Head bump under a ceiling, a crown or an eave kills upward velocity. Landing takes the nav surface at
the current x,z if it is valid, or nudges up to the radius to the nearest valid point; with no valid
landing at all — water, the void off the map edge, a top too narrow to stand on — the body **returns
to the takeoff point with a quick fade and a log line, and takes no damage**. Falling off the world
does the same. Walking off a ledge is the same fall with no impulse, and dropping any height onto
walkable ground is allowed.

**The blob shadow is what makes a jump readable**: it stays on the ground under the body and shrinks
and fades with height. The **camera follows the ground-projected position** with a softened height, so
a jump does not bob the whole town; it is clamped to within 6 cells of the body so it can never lose
the party — the third old bug.

### Facing, with four rows of art

Eight directions of movement, four rows of art (S, side, N; the side row mirrored for E), so a
diagonal sits exactly on a boundary and flickers if you take the nearest row every frame. The facing
is **kept until the move vector is more than 55 degrees off its axis**. The walk cycle is driven by a
phase that advances with the body's ACTUAL speed, so a creep on the touch stick ambles, a run strides,
and a body pressed into a wall stops moving its legs. Stopped is the stand frame.

### The followers, and the NPCs

**Ottilie walks the leader's breadcrumb trail** — a ring of 512 points at 0.06 cells apart with their
own arc length — sampled 1.15 cells behind per party slot. Same route, same jumps, at the same spots,
with no second simulation to get wrong, and **no collision between party members at all**, so she can
never be the thing standing in your way. If the trail is too short, the sample lands off the region,
or she is more than 8 cells away for 2 seconds (or 14 cells at all), she teleports beside the leader
with a quick fade.

**NPCs wander freely** on the nav region: a target within the home radius, taken only if the straight
line to it stays inside the region, walked at 1.5 cells/s, abandoned if they walk into something. They
stop and turn to face the player on interact. They block the player **softly** — a push-out circle
where the NPC takes 65% of the correction and the player 35%, both re-resolved against the region — so
an NPC can never wedge the player: being pushed at is what makes the NPC step aside.

### Triggers

Still **cell-based**, because the .tmap means them that way. `exit`, `door`, `zone`, `trap` and
`scene` fire when the leader's cell changes **on the ground**; an exit whose rectangle touches the map
border fires in the air too, so a jump off the edge still takes you to the next map. `message` and
`npc` need the interact press, with the `!` shown when the target is within 0.9 cells and roughly
faced — a 100-degree cone — or under the feet. Dialogue freezes movement and cancels jump input.

### Reachability

`vx_verify_reach` (cell level, unchanged) still runs first. Then the nav flood proves, under **walk
connectivity only — nothing the story needs may require a jump** — that every exit, door, message
trigger and NPC is in region 1. When one is not, `vx_build_nav` removes the **decoration** voxels
(anything above the surface that is not a full cube: sills, rails, eaves) from that cell and its
neighbours, logs how many, and rebuilds, up to four rounds. Structure is never removed. The verdict is
`navreach=ok|FAIL` in `SELFCHECK vox: nav`, with `widened=` and the jump-only region sizes beside it.

**A bug found doing this and fixed here:** `vx_can_stand` tested a ramp cell's own first two voxels
for a ramp SHAPE, but the generator fills the far row with a solid CUBE and puts the slope on top of
it. Every ramp cell in the game therefore failed the stand test, the reach pass declared it
unreachable, and the flatten-and-retry **deleted every ramp** — halm's two and west_road's thirteen —
after the `ramps=` counter had already been printed. Ramps now survive, and west_road's jump-only
voxels went from 4 to 0 and halm's from 76 to 60 as a result.

### Debug

Dev panel, under the map buttons: **Nav view**, **No clip** (off by default), and sliders for walk,
run, jump apex, gravity and radius. Nav view draws the **eroded** region — where the centre may be —
in translucent blue, jump-only regions in magenta, every ledge edge as an orange bar, the body's
radius circle in cyan and the breadcrumb trail in yellow. Blue and not green because most of this
world IS green grass. **The gap between the orange line and the blue is the buffer**; looking at that
gap in a capture is how the clipping bug is checked for. `Print cell` adds the nav verdict, the
region and the clearance at the body's own voxel.

```
./capture.sh --vox halm --at 21,21 --nav 1        the overlay, in a capture
./capture.sh --vox halm --nav 1 --radius 0.4      what a bigger body would be allowed
./capture.sh --vox-walktest [map|all]             the movement bot
```

### The walk test

`capture.sh --vox-walktest` drives the **same movement code** a thumb drives, with no rendering and no
phone, and exits non-zero on failure. Three parts:

1. **6 000 frames of scripted input** at jittered dt (8–35 ms) pressed into walls, fences and corners.
   Asserts the body never leaves the eroded region, never NaNs, and is never stuck for more than 3 s
   while the input is held along a path that is open a cell ahead.
2. **A\* on the nav grid** (the bot's own, walk connectivity only — nothing in the field pathfinds)
   from the spawn to every exit, door and NPC, then the bot walks the path and has to arrive.
3. **200 jumps** from random valid spots on random headings, each of which must end on valid ground or
   in a clean respawn.

```
WALKTEST halm:      ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 6/6 | jumps 200/200 clean (4 respawns)
WALKTEST hart_yard: ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 2/2 | jumps 200/200 clean (1 respawn)
WALKTEST west_road: ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 1/1 | jumps 200/200 clean (2 respawns)
```

The respawns are jumps that ended over the stream or off the map edge — the clean outcome, not a
failure.

### Input

**Touch is a fully free floating analog stick.** It appears wherever the left thumb lands in the left
45% of the screen, any angle, dead zone 12% of its throw, and **the magnitude is the speed**: under
55% of the throw is a walk (creeping at the very bottom), over it is a run, with a ring drawn at the
line so the thumb can feel where it is. **There is no run button** — one fewer thing for a thumb to
find, and the owner's "fully free is better for touch". The right side has two round buttons, **JUMP**
and the interact button that was already there, both low and to the right so neither is near
Dev/Settings at the top right; `vx_btn_act`/`vx_btn_jump` are the one place their geometry lives, so
the hit test and the drawing cannot drift apart.

Keyboard: **WASD/arrows** (8 directions, normalised), **Shift** run, **Space** jump, **Enter or Z**
interact. Space used to be interact; it is the jump now.

### Cost

`movement` is its own perf scope, nested inside `tick/logic`. It measures well under 0.05 ms a frame
on the Mac with four actors and the NPCs — the whole system is a few dozen nav-voxel lookups. The nav
build is load time only and logs its own ms.

## Camera, and why perspective

Fixed yaw, never rotating. **Perspective by default**, Octopath-style: pitch 42°, FOV 32°, about 11
blocks of vertical view, which puts the party at roughly a fifth of the screen. The camera stands
**south of the party looking north**, so east is on the right and north at the top — the tile field's
orientation, which is what the walker sheets were drawn for. (Standing north of the party and looking
south mirrors the world; that was the first build's other bug.) Ortho is a Dev toggle and a real
alternative; pitch, FOV and view height are sliders.

A **far fog** toward the horizon colour makes the far map edge fade instead of ending.

The **sky** is a full-screen gradient drawn before the world: a horizon colour up to a deeper zenith,
both palette indices through the colormap, with a slow procedural cloud band in the top half tinted
the lightest step of the plaster ramp (looked up by NAME — no colour is typed in). The fog colour and
the horizon are the same colour by construction.

## Sprites

- Characters are **billboards that lean back** toward the camera by `tilt` x pitch (0.68 by default),
  feet pinned to the ground, so an upright sprite is not foreshortened to 70% by a 42° camera.
- The **existing 9-frame walker sheets** (`story/field/walkers/<id>.png` + `.json`, rows S/side/N,
  columns stand/A/B, side row mirrored for east), decoded, **indexed on the master palette** and
  uploaded `GL_R8` — the same currency the blocks are drawn in, so a sprite and a wall can never
  disagree about a colour. `GL_NEAREST` both ways: the owner's pixels stay pixels.
- **Alpha-tested with depth write**, so they sort against the voxels for free. No sorting pass.
- A **soft blob shadow** under each, and **detail billboards** — the tileset's `decals/*.png` —
  hash-scattered over grass, about one in six cells, with a live density slider.
- **Lamps at night** get an additive soft glow billboard; the bloom does the rest. Off in full day.

## HD-2D post pass

One full-screen composite over three half-resolution passes, all of it behind a master **HD-2D**
toggle with a strength slider each:

1. **Tilt-shift** — the depth texture is read, linearised (perspective or ortho), and the blurred
   half-res copy mixed in by how far the fragment is from the party's own depth. A sharp band follows
   the party.
2. **Bloom** — the blurred copy blurred again and thresholded at 0.66 luma: lamps, water glints, night
   windows, sunlit plaster.
3. **Vignette** and 4. a **gentle grade** (warm highlights, cool shadows).

The pixel detail stays crisp inside the focus band: nothing in the pass smooths a texel, it only mixes
in a blurred copy outside the band.

## Occlusion

A **cutaway**: in the world fragment shader, anything inside a screen-space circle around the party
and nearer to the camera than they are is discarded, with a dithered edge. One `if`, no geometry, no
per-object bookkeeping — the robust option.

## Numbers

Measured on the Mac at 1920x1080 with `glFinish` around the world and post draws:

| | ms/frame |
|---|---|
| world only (`VOX_HD2D=0`) | **2.42** |
| world + HD-2D | **3.48** |

The post pass costs about **1.06 ms** at 1080p. Against the whole-cell build (1.96 / 2.90) the half-size
voxels plus the shadow map cost about **0.46 ms** of world time for 9x the triangles — the extra
shadow fetch set is four taps a fragment and the geometry is vertex-bound, not fill-bound. **On the
phone that split will not hold**: four `sampler2DShadow` taps at 1080p on a mobile GPU is the thing to
measure first, and the `res %` slider is the lever.

halm: 9 chunks, **73 810 triangles**, 25 draw calls, 330 sprites, 305 detail billboards, **2 207 shaped
voxels, 12 buildings, 2 ramps**, 29 ms to mesh, 2.3 ms to shadow, 0 px off-palette.
hart_yard: 15 748 tris, 290 shaped, 2 buildings. west_road: 19 992 tris, 349 shaped, 13 ramps.

**The phone has not been measured.** The device at 192.168.1.217:5555 was offline for this round, so
no `fast_reload.sh` run and no device `SELFCHECK`/frame line. The budget lever is there: the Dev
panel's **res %** slider renders the 3D at a fraction of the drawable and upscales, and it rides the
reload blob. Both `voxfield.cpp` and `star_logic.cpp` were compiled for `aarch64-none-linux-android24`
with the NDK and `-DIMGUI_IMPL_OPENGL_ES3` to prove the GLES 3 path builds.

## Performance

The owner, 2026-09-19: *"It looks amazing! It does feel slow. I need better tools to analyse what is
making it slow."* `src/vxperf.h` is those tools — header-only, included by `voxfield.cpp` alone, so
nothing in `CMakeLists.txt` or `fast_reload.sh` had to change to carry it.

**The first thing it found was a `glFinish()` in the frame path.** It had been left in to make a
CPU-side timer around `vx_render` + `vx_post` mean something, and it cost the phone a third of its
frame rate: on a tile-based GPU it flushes and waits inside every frame, so the CPU and the GPU never
overlap. Removing it took halm from **~47 fps / 21 ms to a vsync-locked 60 fps**. Per-pass GPU cost
now comes from timer queries, which do not stall. **Do not put a `glFinish` back on that line.** The
benchmark is the one caller allowed to stall, and only when the timer queries are unavailable.

### The instrumentation

- **Named scopes**, flat by construction: `tick/logic`, `mesh/upload`, `shadow map`, `sky`,
  `world opaque`, `sprites+detail`, `post: blur`, `post: bloom`, `post: composite`, `ui/dialogue`.
  Each one is timed on the CPU with `SDL_GetPerformanceCounter` and on the GPU with a timer query.
  Water is **not** a scope: water voxels live in the chunk meshes and are shaded by the world program,
  so their cost is inside `world opaque`. Saying so beats a scope that would always read 0.
- **GPU timers**: `GL_EXT_disjoint_timer_query` on GLES, `GL_TIME_ELAPSED` on desktop GL. Every entry
  point is fetched through `SDL_GL_GetProcAddress` (EXT name first, then core), because macOS's
  `gl3.h` does not declare `glGetQueryObjectui64v` and the GLES3 headers declare none of the EXT
  ones. Results are read **three frames late** out of a four-deep ring and the AVAILABLE flag is
  checked before the value is taken, so a query never blocks. `GL_GPU_DISJOINT` is read only when a
  slot actually holds a query and its samples are thrown away. The queries are **armed only when
  somebody is reading them** — `vxp_want_gpu(perf_hud > 0 || bench_on)`, or `VXPERF_GPU=1`. If anything is missing it falls back to CPU only and the HUD says
  `gpu timers unavailable`. **`GL_TIME_ELAPSED` cannot nest**: a scope opened inside another is timed
  on the CPU alone and its GPU column reads `-`.
- **TOTAL is wall clock, tick entry to tick entry** — the honest frame interval including the host's
  swap. That needed no hook in `host.cpp`, so none of this costs a `./deploy.sh`.
- **Counters**: draw calls, triangles after frustum culling, chunks drawn/total, sprites, detail
  billboards, FBO and drawable size, shadow size, and a GPU memory estimate (an estimate, and
  labelled one — GLES has no query for it).
- **Frame pacing**: the display's refresh rate from `SDL_GetCurrentDisplayMode`, the swap interval,
  and a histogram of frame intervals in vsync buckets (1x, 2x, 3x, 4x+, off-grid). A run that is
  missing vsync is visible as weight in the 2x bucket rather than as a worse average.

### The HUD

Dev panel → **Perf HUD: off / compact / full**. It rides the reload blob (`VxSave.perf_hud`), so a hot
reload keeps it up. Compact is fps, frame ms average / 1% worst / max and a 120-sample graph with the
16.7 and 33.3 ms lines on it. Full adds the per-scope CPU and GPU table sorted by cost, the counters,
the refresh rate and swap interval, the vsync buckets and the timer-query note. It is drawn into the
foreground draw list with no windows and one `snprintf` a line.

### The benchmark

Dev panel → **Run benchmark**, or a `perf.flag` in the pref dir (its contents name the map). It saves
the owner's settings, parks the party on a fixed view, and runs **20 configurations** for 30 warm-up
plus 240 measured frames at **two views** (halm: the square and the stream), then puts the settings
back. Configurations: baseline, HD-2D master, DoF, bloom, grade+vignette, shadows, shadow PCF 1 tap,
procedural pattern detail, AO, cutaway, detail sprites, water animation, sky clouds, fog, resolution
scale 100/85/75/66/50 %, and everything off. Output is a `PERF` line per configuration in the log and
`perf_report.md` / `perf_report.csv` in the pref dir, with the device and GL strings, the resolution
and the date, the per-scope table and the counters.

**Wall clock is vsync-bound and cannot separate two configurations that both beat the refresh period.
The column that can is `gpu ms`.** Read that one.

The `u_qpattern`, `u_qpcf`, `u_qwater` uniforms and the `q_*` fields exist **only** so the benchmark
can turn one thing off at a time. All three are uniform branches — one value for the whole draw — and
all three sit at the shipping look unless a benchmark moved them. They are not settings.

```
./perf.sh [map]        the phone: writes perf.flag, waits for `PERF done`, pulls the report, prints it
./perf.sh --desktop    the same benchmark in the desktop build
./perf.sh --watch      tail the periodic `voxfield perf:` lines from the phone
```

The periodic log line (every 300 frames) is now wall-clock ms with the 1% worst and the max, the GPU
total, the CPU total, and the three most expensive scopes, so a `logcat` tail alone says where the
time went.

### What was optimised, and what was measured

Measured on the phone (Adreno 650, 2400x1080, 60 Hz), halm, at the square:

| | GPU ms | wall ms | fps |
|---|---|---|---|
| before | 12.62 | 17.98 | 55.6 |
| after | see `build_desktop/perf/` | | |

1. **The `glFinish` is gone** from the non-capture path. The single biggest win, and it cost nothing.
2. **The sky is drawn after the opaque world, depth-rejected against it.** It used to be drawn first
   with the depth test off, shading all 2.59 M pixels — two octaves of value noise, eight hash
   evaluations each — and then being painted over by the town. The benchmark put `sky clouds off` at
   **-1.71 ms**, which is what that pass was worth. Now `VX_QUAD_VS` emits the quad at the far plane
   (`z = 1.0`) and the sky is drawn with `GL_LEQUAL` and no depth write, so every pixel the world
   already covers is rejected before the fragment shader runs. The pixels that survive are exactly
   the ones that were visible before, shaded by the same code. It is drawn **after the opaque pass and
   before the blended one** — the only order that is also correct, since a lamp glow over open sky
   would be overwritten if the sky came last.
3. **The cutaway `discard` is the first thing in the world fragment shader**, not the last. It used to
   sit after the whole pattern had been evaluated, so every discarded fragment paid for ten hash
   evaluations and a LUT fetch first. Same pixels out.
4. **A shadow strength of 0 now really skips the taps** in both the world and the sprite shader, which
   is what makes the benchmark's `shadows off` row mean anything.
5. **Uniform locations are cached** (`vx_uni`). The render path made about forty
   `glGetUniformLocation` string lookups a frame and they are the same forty every frame. The cache is
   keyed by program id and the name's address and is **reset in `vx_gl_init`**, because a fresh
   program can be handed a recycled id.

6. **The world shader is two programs, and only the chunks that need it get the one with `discard`.**
   A fragment shader that contains `discard` anywhere is one the GPU cannot assume writes depth at the
   rasterized value, so Adreno turns **LRZ — its early-Z / hidden-surface removal — off for the whole
   draw**. The cutaway's `discard` sat under a uniform branch in the one world shader, so the entire
   town was shaded with no early-Z: ten hash evaluations and four shadow taps a fragment, for pixels a
   nearer wall went on to cover. `VX_FS_HEAD` + `VX_FS_BODY` is now compiled twice, once with
   `VX_FS_CUT` between them and once without, and `vx_chunk_needs_cut` picks per chunk: a chunk
   entirely at or behind the party, or whose screen box misses the cutaway circle, has no fragment the
   block could discard and gets the discard-free program. On halm that is **6 of 7 visible chunks at
   the square and 2 of 5 at the stream**. The test is conservative everywhere it is unsure (a corner
   across the near plane → use the cutaway variant). Verified by forcing every chunk to the cutaway
   program and diffing five views: four byte-identical, one with 210 differing bytes of 8.29 M at a
   maximum of 1/255 — post-pass rounding from a different draw order, not a selection error.
7. **The visible chunks are drawn front to back.** An insertion sort over at most `VX_CHUNKS` boxes by
   view-space distance, which is free; without it early-Z has nothing to work with. Measured on the
   overdraw probe: square **1.747 → 1.645** writes per covered pixel, stream **1.939 → 1.910**.
   `VOX_CHUNKSORT=0` (index order) and `=-1` (back to front) exist only so that number can be taken.
8. **The GPU timer queries are only armed when somebody is reading them** (`vxp_want_gpu`, called with
   `perf_hud > 0 || bench_on`; `VXPERF_GPU=1` forces them on). Left on all the time they cost a
   `glGetIntegerv(GL_GPU_DISJOINT_EXT)` and up to `VXP_COUNT` `glGetQueryObjectuiv` a frame, and on
   Adreno a `glGet*` can flush the command stream — the instrumentation would be measuring itself.
   The CPU scopes and the spike recorder stay on always; they are two counter reads.
9. **The sprite vertex data is uploaded once a frame, not once a texture.** The kind-0 pass called
   `glBufferData` per texture — four walker sheets, the atlas and ten decals, fourteen orphan-and-
   upload round trips, each one a driver allocation the GPU may still be reading from. The whole array
   is now built first, uploaded once, and drawn with fourteen `glDrawArrays` offsets into it.
10. **Per-frame odds and ends that were pure overhead**: `SDL_GetCurrentDisplayMode` was called every
    frame for a refresh rate that never changes (now cached for a second); the GPU memory estimate
    looped over every decal every frame for a HUD that was closed (now only when it is open);
    `map.flag` and `perf.flag` were opened 2.5 and 2 times a second on the render thread (now once a
    second each, half a period out of phase so they can never charge the same frame).

None of these changes the look; the captures (`--vox halm`, `--viewh 24`, `--light night`) were
compared before and after. **No default visual quality or resolution scale was changed.** The
resolution rows in the report are there so the owner can make that call with numbers.

### The spike recorder

`p99` on the phone sat at 33-45 ms in almost every configuration — regular missed vsyncs — while the
averages were fine, and no average can tell you what happened in the one frame that went long. So:
when the frame interval exceeds `vxp.spike_ms` (25 ms), one `VXSPIKE` line is logged naming **every
scope's CPU time for that frame**, plus the total those scopes account for and what is left over.
It is rate-limited to one a second and says how many it swallowed. Read it like this:

- scopes add up to roughly the frame → the game did it, and the line names which pass;
- `unaccounted` is nearly the whole frame → the game did **not** do it. The time went somewhere the
  render thread was not running: the scheduler, a GPU queue the driver blocked on, or another process.

`cpu_ms` is this frame's, **zero included**, precisely so a stale number (`mesh/upload`'s 70 ms from
load time) can never be read as a cause forever after.

### Overdraw, and whether a deferred pass would pay

`vx_measure_overdraw` replays the visible chunks **in the order the real pass just drew them** with a
constant-colour probe blended `ONE/ONE` against a fresh depth buffer, then reads back a 1-in-4 row
sample. The byte in a pixel is the number of times the opaque world wrote it; the mean is over the
covered pixels, so the sky's empty half of the screen does not flatter it. It is a full pipeline
stall, so it runs only in the capture path (`VOX_OVERDRAW=1`), once a configuration in the benchmark
(the `overdraw` column), and at most once a second in the Dev panel's **Overdraw view**.

On halm at 1920x1080: **square 1.65, stream 1.91**. So a perfect depth prepass or a deferred pass
could save at most a third to a half of the world's shading — and would pay for it with a G-buffer
write and read of every pixel, which on a tile-based mobile GPU is the expensive half. **It does not
justify a deferred renderer.** The win was never the overdraw count: it was that `discard` had
switched early-Z off altogether, so the GPU was shading every rasterized fragment rather than the
1.65 that survive. Items 6 and 7 above are that fix. Re-measure on the phone before revisiting this.

## Dev panel

Sun / shadow: azimuth and elevation (each re-renders the shadow map), strength, softness, a
**Snapped** toggle and **Sun default**. Then: map buttons and Respawn; Coords, No clip, Cut away;
**Nav view** and the movement sliders (walk, run, jump apex, gravity, radius — see "Movement");
Ortho, pitch, FOV, view blocks, sprite tilt, AO,
detail density, fog; HD-2D with tilt-shift, bloom, vignette, grade and res %; the colormap table
buttons and the ambient slider; **Print cell** (height, top block, walkable, tris, mesh ms, reach).
Above them, **Old 3D field** and **Old tile field**.

## Tooling

```
./capture.sh --vox-selftest                               every map loaded, meshed and checked
./capture.sh --vox-walktest [halm|all]                    the movement bot: walls, paths, 200 jumps
./capture.sh --vox halm --nav 1 [--radius 0.4]            the navmesh overlay, in a capture
./capture.sh --vox halm                                   build_desktop/vox_halm.png, 1920x1080
./capture.sh --vox halm --at 21,21 --name spawn           stand the party on a cell
./capture.sh --vox halm --ortho 1 / --hd2d 0              the toggles, for a side by side
./capture.sh --vox halm --light night:0.30 --name night   the same colormap the phone uses
./capture.sh --vox halm --pitch 40 --fov 32 --viewh 11 --face N --size 2400x1080
./capture.sh --vox halm --cut 0 --fog 0                   the diagnostics
./perf.sh halm                                            the A/B benchmark on the phone, table + report
./perf.sh --desktop halm                                  the same on the Mac
./perf.sh --watch                                         tail the periodic frame lines
```
`run_desktop.sh` runs the same binary; `files/map.flag` still drives it to a map with no taps.

`fast_reload.sh` and `deploy.sh` now ship `story/field/tilesets/<set>/decals/` as well as the atlas,
`tiles.md`, `tmaps`, `walkers`, `swatches` and `palette/`. Everything the voxel field reads is on that
list; a file that is not on it shows on the phone as black.

## The reload blob

`RELOAD_MAGIC` went `STR9` → `STRA` → `STRC` → **`STRD`** (free movement: `VxSave` now carries a
FLOAT position and the five movement tunables; the party always comes back on the ground, never
mid-jump, and a blob written before free movement falls back to the cell). `VxSave` carries the map,
the party's position and facings, the
step count and every look knob (camera, HD-2D, light, res %), and `vx_restore` validates all of it —
a cell that is off the map or no longer standable is dropped rather than stranding the party in a
wall. The look knobs are applied **before** `vx_load_map`, the light table and ambient after, so what
the owner was tuning wins over the map's own `light:` line.

## The self-test

`./capture.sh --vox-selftest` loads halm, hart_yard and west_road in turn, builds and meshes each,
renders its shadow map and prints a `SELFCHECK vox:` line — and a second `SELFCHECK vox: nav` line
with the navmesh's own verdict (walkable voxels, walk-reachable, jump-only regions and their sizes,
how much was widened, `navreach`, build ms) — with the shaped-voxel count, the number of
buildings generated and `reach=ok|FAIL`, then a verdict line. The script greps that line and exits
non-zero. All three pass.

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
