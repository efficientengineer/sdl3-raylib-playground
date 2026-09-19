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

- **One block is one walk cell.** `blk[y][z][x]`, `uint8`, up to 96 x 96 cells and 24 blocks high.
  X is east, **Z is south** (the `.tmap` row index), Y is up.
- **Chunked 16x16 columns, meshed once at load** into one static VBO each — halm is 9 chunks,
  ~7 850 triangles, 2.6 ms to mesh. Hidden faces are culled; there is no greedy meshing (it buys
  little at this size and is a class of bug this round did not need).
- **Winding is derived, never typed.** Each quad's first two edges are crossed and the result checked
  against the face's own normal; if it disagrees the quad is flipped. The first build had every top
  face wound the wrong way and the ground vanished under back-face culling — this is why that can no
  longer happen for any face.
- **Light is baked, per the project rule.** Each vertex carries the face's sun term times a marched
  column shadow (`vx_sun_shadow`, twelve steps along a fixed sun), the classic 0-3 neighbour AO, and
  the lamp level at that corner. The **ambient level and the AO strength stay live** because they are
  applied in the shader to numbers that were baked; nothing is re-traced per frame.

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

- **Ground terrain id → the column's top block**; below it dirt then stone.
- **Water cuts one block down.** The bed is `waterbed`, the water block sits at `land - 1` and its top
  face is emitted **0.8 of a block high**, so the bank shows real depth and the neighbouring land's
  side faces are drawn. Water draws only its top face, animated at 6 fps with hash glints the bloom
  catches.
- **Building stamps are extruded**: walls three high in plaster/timber/plank/stone chosen by the stamp
  id, a **stepped gable** whose ridge runs along the longer side, a door block two high on the south
  face at the x of the map's own door or message trigger, and window blocks either side of it. A door
  is 2 blocks; the party is 1.6.
- **Tree stamps** are a trunk of two or three and a squat ragged crown.
- **Fences, walls and hedges** are one block high; posts two.
- **Everything else stays a billboard** of its own atlas cells, standing on the map. That is the safe
  default — the well, the handcart, the market stall, barrels, crates, the trough. Nobody has to model
  anything for a map to work.
- **Triggers, spawn, NPCs and `lamp:` meta carry over unchanged**, as does `light: <table> <level>`.

### Terrain height, and the flood fill that keeps it honest

Grass, dry grass and crop get a deterministic two-octave height of 0..2 extra blocks. Everything else
is **forced flat**: paths, paving, water, every stamp footprint, every trigger rectangle, and one cell
of margin round all of it. Then eight passes clamp every neighbour to within one block, so a step is
never more than one. Finally **two flood fills from spawn** — one under the `.tmap`'s own rules, one
under the voxel rules — are compared, and any cell the tile map can reach but the voxel world cannot
is flattened and the check rerun, up to six rounds. The result is in the `SELFCHECK vox:` line as
`reach=ok` or `reach=FAIL`. On halm it passes first time.

## Camera, and why perspective

Fixed yaw, never rotating. **Perspective by default**, Octopath-style: pitch 42°, FOV 32°, about 11
blocks of vertical view, which puts the party at roughly a fifth of the screen. The camera stands
**south of the party looking north**, so east is on the right and north at the top — the tile field's
orientation, which is what the walker sheets were drawn for. (Standing north of the party and looking
south mirrors the world; that was the first build's other bug.) Ortho is a Dev toggle and a real
alternative; pitch, FOV and view height are sliders.

A **far fog** toward the sky colour (itself a palette index through the colormap) makes the map edge
fade instead of ending.

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
| world only (`VOX_HD2D=0`) | **1.96** |
| world + HD-2D | **2.90** |

So the post pass costs about **0.94 ms** at 1080p. halm: 9 chunks, 7 850 triangles, 25 draw calls,
329 sprites, 304 detail billboards, 2.6 ms to mesh, 0 px off-palette.

**The phone has not been measured.** The device at 192.168.1.217:5555 was offline for this round, so
no `fast_reload.sh` run and no device `SELFCHECK`/frame line. The budget lever is there: the Dev
panel's **res %** slider renders the 3D at a fraction of the drawable and upscales, and it rides the
reload blob. Both `voxfield.cpp` and `star_logic.cpp` were compiled for `aarch64-none-linux-android24`
with the NDK and `-DIMGUI_IMPL_OPENGL_ES3` to prove the GLES 3 path builds.

## Dev panel

Map buttons and Respawn; Coords, No clip, Cut away; Ortho, pitch, FOV, view blocks, sprite tilt, AO,
detail density, fog; HD-2D with tilt-shift, bloom, vignette, grade and res %; the colormap table
buttons and the ambient slider; **Print cell** (height, top block, walkable, tris, mesh ms, reach).
Above them, **Old 3D field** and **Old tile field**.

## Tooling

```
./capture.sh --vox halm                                   build_desktop/vox_halm.png, 1920x1080
./capture.sh --vox halm --at 21,21 --name spawn           stand the party on a cell
./capture.sh --vox halm --ortho 1 / --hd2d 0              the toggles, for a side by side
./capture.sh --vox halm --light night:0.30 --name night   the same colormap the phone uses
./capture.sh --vox halm --pitch 40 --fov 32 --viewh 11 --face N --size 2400x1080
./capture.sh --vox halm --cut 0 --fog 0                   the diagnostics
```
`run_desktop.sh` runs the same binary; `files/map.flag` still drives it to a map with no taps.

`fast_reload.sh` and `deploy.sh` now ship `story/field/tilesets/<set>/decals/` as well as the atlas,
`tiles.md`, `tmaps`, `walkers`, `swatches` and `palette/`. Everything the voxel field reads is on that
list; a file that is not on it shows on the phone as black.

## The reload blob

`RELOAD_MAGIC` went `STR9` → **`STRA`**. `VxSave` carries the map, the party's cells and facings, the
step count and every look knob (camera, HD-2D, light, res %), and `vx_restore` validates all of it —
a cell that is off the map or no longer standable is dropped rather than stranding the party in a
wall. The look knobs are applied **before** `vx_load_map`, the light table and ambient after, so what
the owner was tuning wins over the map's own `light:` line.

## Known issues, in the order they matter

1. **Not on the phone.** No device numbers, no device SELFCHECK. First job next round.
2. **Trees read as a trunk under a green mass.** The crown is a ragged ellipsoid of leaf blocks and at
   this camera height it is chunky. A two-block canopy with an overhang, or a billboard crown, would
   read better.
3. **Windows are flat blue rectangles.** They want a frame block or a shutter pattern.
4. **Ground boundaries are square.** A voxel world has no dual grid; grass meets paving on a cell edge.
   That is the honest cost of the pivot and may be exactly what the owner wants.
5. **Particles (motes, fireflies) are not implemented.** `motes` rides the save blob and the Dev panel
   has no control yet.
6. **Sprite shadows are blobs**, not squashed copies of the sprite.
7. **No frustum culling**: every chunk is drawn. At 9 chunks that is free; a 96x96 map is 36.
8. **`map.flag` is polled in two places** (star_logic's title poll and `vx_tick`), so a flagged map can
   load two or three times at startup. Harmless, wasteful, worth tidying.
