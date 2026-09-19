# TILES2 — a more robust tile system (proposal)

Answering the owner's ask: *"We need a more robust tile system. We should be able to flip and mirror
tiles so we can do more transitions. We should look up state of the art tile system practices and
apply them."* — plus the follow-up: *"Can we do overlay tile props for variety? What if our grass tile
was very toned down, not much noise, but we could overlay flowers, rocks, whatever for sparse detail
that doesn't repeat every tile?"*

This is a research + design document. Nothing in `src/`, `story_prompt.py` or `TILES.md` was changed.
The prototype that produced the pictures is `tools/tiles2/` (python stdlib, `out/` gitignored).

**Recommendation in one line:** keep the 32-px logical cell and the `.tmap` format, and replace
*drawn transition tiles* with a **dual-grid renderer driven by procedural 1-bit masks over
world-space seamless swatches**, dressed with **hash-scattered decals** and a **macro light drift**.
ChatGPT then draws **one swatch per terrain and no transition tiles at all** — from 4 images per
terrain today (ground + 3 fringes, which must match each other exactly) to **1**, and the engine's
per-terrain art risk goes to zero.

---

## 0. The pictures (pushed to `/sdcard/Pictures/tiles2/`)

Same 20x12 test map every time: a lake, a winding dirt path, a lopsided paved square, grass/dirt/water
and grass/dirt/paving meeting at single corners.

| file on the phone | what it is |
| --- | --- |
| `tiles2_old.png` | **today's system**: one repeated tile per terrain + the three rotated fringe tiles |
| `tiles2_new.png` | **dual grid + masks + world-space swatches + decals** |
| `tiles2_new_detail.png`, `tiles2_old_detail.png` | the same 7x4 tiles of each, at 2x |
| `tiles2_s5_all.png` | the full overlay stack: calm swatch + decals + macro drift + a second grass |
| `tiles2_stack_strip.png` | five panels: base → +decals → +macro drift → +second grass → all |
| `tiles2_s5_detail.png` | 8x5 tiles of the stack at 2x |

`old.png` is the honest comparison and it is the argument: the terrain silhouettes are 32-px
staircases, every cell of dirt is the same drawing, and the fringe overlays only soften the step —
they cannot round a corner, because a corner is a whole tile. `new.png` has no straight edges, no
visible cell boundary and no repeat at the tile scale, from **one** drawing per terrain.

---

## 1. Findings (research)

Everything in this section was read from the sources cited. Where a topic was on the brief and is
**not** here, it says so rather than being filled in from memory.

### 1.1 The families, and what each costs the artist

| family | tiles to draw | what must match | note |
| --- | --- | --- | --- |
| 4-bit edge (cardinal neighbours) | **16** | tile edges | cannot express an inner/outer corner difference — it never looks at a diagonal |
| 8-bit "blob" | **47** distinct shapes, packed as **48** (6x8, tile 255 duplicated) or **49** (7x7) | edges *and* corners, pixel for pixel | the 47/48 confusion is exactly this: 47 shapes, 48 slots |
| corner Wang, 2 terrains | **16** (c⁴, c = 2) | the four corner quadrants | per *pair* of terrains — combinatorial in the number of terrains |
| edge Wang, 3 colours | **81** | edges | never rotated or reflected, by definition |
| quarter-tile (RPG Maker's family) | **5** half-size with rotation, 14–20 without | quarter boundaries | |
| **dual grid / marching squares** | **16**, or **6 with rotation** | nothing between tiles — each drawn shape is one terrain over transparency | |

The blob reduction from 2⁸ = 256 to 47 is because a diagonal bit only means anything when both
cardinal neighbours beside it are also set. cr31's bit weights are the numbering everything uses
(N 1, NE 2, E 4, SE 8, S 16, SW 32, W 64, NW 128), and on that numbering **a 90° clockwise rotation
is `index * 4`, minus 256 if it overflows** — worth knowing if we ever do want drawn tiles.

- Wang tiles, and the "never rotated or reflected" definition: http://www.cr31.co.uk/stagecast/wang/intro.html
  (mirror: https://www.boristhebrave.com/permanent/24/06/cr31/stagecast/wang/intro.html)
- The 47-tile blob set, the bit weights, the rotation-by-multiplication trick:
  https://www.boristhebrave.com/permanent/24/06/cr31/stagecast/wang/blob.html
- The 6x8/7x7 minimum packings, found by exhaustive search:
  https://www.boristhebrave.com/permanent/24/06/cr31/stagecast/wang/blob_g.html
- 4-bit vs 8-bit, and the terrain-priority extension: https://www.redblobgames.com/articles/autotile/claude/
- Tiled has an open request (#1873) for a 47-blob authoring helper, citing that same cr31 page:
  https://github.com/mapeditor/tiled/issues/1873

### 1.2 The dual grid

Two grids offset by **half a tile in both axes**: the data grid holds the terrain per cell (gameplay
and the editor touch only this), and the display grid is shifted half a tile, so **every display tile
sits on the intersection of exactly four data cells** and is chosen by a 4-bit *corner* mask — 16
cases, four neighbour lookups instead of nine. A `W x H` data map needs a `(W+1) x (H+1)` display map.

- Oskar Stålberg proposed it publicly on 13 Oct 2021: https://x.com/OskSta/status/1448248658865049605
- Boris the Brave's framing — marching squares stores terrain on *vertices* (the dual grid) where
  quarter-tiles store it on cells: https://www.boristhebrave.com/2023/05/31/quarter-tile-autotiling/
- ThinMatrix uses it in *Homegrown*: six tile shapes, four lookups instead of nine —
  https://www.youtube.com/watch?v=buKQjkad2I0 (the moment: https://youtu.be/buKQjkad2I0?t=234)
- jess::codes, "Draw fewer tiles – by using a Dual-Grid system!" (2024):
  https://www.youtube.com/watch?v=jEWFSv3ivTg, Unity
  https://github.com/jess-hammer/dual-grid-tilemap-system-unity, Godot
  https://github.com/jess-hammer/dual-grid-tilemap-system-godot. Her README's claims: **"a maximum of
  only 16 tiles are required in the tileset, as opposed to 47"**, "perfectly rounded corners", tiles
  unambiguous against the world grid, and 4 neighbour checks instead of 8.
- Excalibur.js, with the geometry spelled out — a **-8, -8** offset for 16 px tiles, a 10x10 world map
  against an 11x11 mesh map, and **all 16 cases from 5 shapes plus a base tile**, by rotation:
  https://excaliburjs.com/blog/Dual%20Tilemap%20Autotiling%20Technique/
- skner's Unity package separates a *Data Tilemap* from a *Render Tilemap*: https://github.com/skner-dev/DualGrid
- Godot implementations: https://github.com/pablogila/TileMapDual,
  https://github.com/jamesplease/dual-grid-tilemap-system-godot-gdscript,
  https://forum.godotengine.org/t/dual-grid-tile-system-implementation/115685

**The enumeration, which is the number the owner asked for.** The 16 corner masks fall into exactly
**six** classes under 90° rotation: empty (1), outer corner (4), straight edge (4), **diagonal/saddle
(2)**, inner corner (4), solid (1). 1+4+4+2+4+1 = 16. Boris tabulates it as 6 with rotation, 16
without: https://www.boristhebrave.com/2023/05/31/quarter-tile-autotiling/

**And the finding that contradicts the ask as posed: mirroring buys nothing for transitions.** Every
one of those six classes is already closed under reflection — each already contains all of its own
rotations, and reflecting any member lands back inside the class. Rotation alone gives all 16 cases.
So "flip and mirror the tiles" is the right instinct about *variety* but not the lever for
*transitions*; flips earn their keep on decals and on nature stamps instead (§3.5, §3.7).

**Multi-terrain.** The plain dual grid is binary. Both primary sources give the same fix and it is the
one this proposal takes: **assign each terrain a priority, treat a neighbour as matching if its
priority is equal or higher, give each terrain its own overlay, and draw low priority first**
(https://www.redblobgames.com/articles/autotile/claude/). Boris notes this is precisely where
marching squares beats quarter-tiles, which have no clean multi-terrain extension
(https://www.boristhebrave.com/2023/05/31/quarter-tile-autotiling/). Layered overlays are what make
the mask idea work: **n terrains cost n fills plus one shared mask set, not n(n-1)/2 drawn transition
sets.** Our `- fringe: n` priority is already this idea, applied to the wrong renderer.

**Two caveats from the sources, both of which we have to design around:**
1. **The saddle is genuinely ambiguous** — two diagonally opposite corners of terrain A can be drawn
   as connected or as separate, and the convention must be fixed or the visuals and any derived
   topology disagree. Ours: the `diag` shape's two quarter-discs **touch at the centre** (connected),
   chosen because a pinch reads as a path and a gap reads as an accident.
2. **The dual grid cannot draw a curve of radius larger than half a tile.** True, and it is why our
   masks add windowed noise: the raggedness supplies the large-scale irregularity that the geometry
   cannot. A deliberately *smooth* sweeping bay would need authored art.

### 1.3 Tiled's transform flags — verified

A Tiled GID is 32 bits with four transform flags in the top nibble: horizontal `0x80000000`,
vertical `0x40000000`, anti-diagonal `0x20000000`, hex-120° `0x10000000`; clear all four to get the
tile id. The anti-diagonal flag combined with H/V is how 90°/270° rotations are encoded — which is
exactly where a rotated dual-grid shape, or our stamp mirror flag, would live in a `.tmj`.
https://doc.mapeditor.org/en/stable/reference/global-tile-ids/

### 1.4 What is **not** researched here

The brief also asked for RPG Maker's A2 block layout and quarter-tile composition; Tiled Wang set
types and the `.tmj` structure in detail; Godot 4 terrain modes; LDtk auto-layers; mask/"splat"
blending as practised in Wesnoth/AoE2/OpenRA; stochastic and hex tiling, hashed decal scatter and
density fields as published; the mirrored-shaded-art light-direction literature; texture
bleeding/extrusion and texture arrays; and chunked batching, animated tiles, sub-tile collision and
y-sorting in PSIV/Chrono Trigger. **Those sub-searches had not returned when this was written, and
nothing in them is asserted here.** The design below does not depend on them: every claim it makes is
either cited above or demonstrated in the prototype. The one place it would change the document is
§3.4 — the exact anti-repetition technique for the world-space swatch — where the prototype's hash
scatter is doing the job empirically and a literature pass could only improve the constants.

---

## 2. Evaluation against our constraints

| | (a) ChatGPT draws, per terrain | (b) flat cel art + hard-alpha indexed | (c) engine complexity | (d) authoring (agents; Tiled) | (e) look: rounded, no grid, 3-way corners | (f) grid walking / collision |
| --- | --- | --- | --- | --- | --- | --- |
| **Today: ground + 3 rotated fringes** | 4 images that must match each other at the pixel *and* be self-consistent under 90° rotation | fine | already built (~120 lines) | unchanged | **poor**: tile-shaped silhouettes; a 3-way corner shows whichever priority wins, as a step | unchanged |
| 4-bit edge autotile (16) | 16 matched tiles | fine | small | unchanged | still cell-shaped, no inner corners | unchanged |
| 8-bit blob (47/48) | 47 matched tiles | fine | medium | unchanged | good | unchanged |
| RPG Maker A2 quarter-tile | 1 small block, but every quarter must align | fine | medium | needs a new art contract | good | unchanged |
| Corner Wang (16, 2-terrain) | 16 matched tiles per *pair* of terrains — combinatorial | fine | medium | unchanged | good | unchanged |
| **Dual grid, drawn tiles (16, or 6 with rotation)** | 6 matched tiles, all of one terrain over transparency | fine | small | unchanged | very good | unchanged |
| **Dual grid + procedural masks + world swatch (recommended)** | **1** swatch, no matching problem at all | **best**: mask is 1 bit, swatch is opaque; nothing to key | medium (one extra sampler, ~250 lines net) | unchanged + a Tiled path | **best**: any shape, any raggedness, 3-way corners fall out of priority | unchanged |
| Runtime alpha-blend "splat" of two swatches | 1 swatch | **bad**: blending indices is meaningless; blending *colours* after lookup leaves muddy half-tones that are off-palette-ramp | medium | unchanged | soft, painterly — not Phantasy Star | unchanged |

The two rows worth arguing about:

- **Dual grid with drawn tiles vs with procedural masks.** Drawn is what the community does, because
  their artists are people. Ours is a model that is reliably good at *one seamless texture* and
  reliably bad at *a set of six shapes that have to tile against each other*. Every failure in the
  current `valley` atlas is in the second category. Masks move the matching problem out of the art
  entirely: the shapes are computed, so they are exact by construction, and the art is the one thing
  the model does well. **Keep the drawn-tile path as an escape hatch** (a terrain may supply
  `mask_<shape>.png`), but do not plan on it.
- **Masks vs splat blending.** Rejected. Our pipeline is indexed with hard alpha (`PALETTE.md`): a
  blend between two terrains produces colours that are on no material ramp, which is exactly what the
  light tables need. A 1-bit mask keeps every pixel on a ramp. The *raggedness* is what sells the
  transition, not the softness.

---

## 3. The recommended design, in contract form

### 3.1 The dual grid

Terrain ids stay on the **world cells** — one terrain per cell, exactly as `## ground` writes them
today, which is what keeps collision and grid walking untouched. The **display grid is offset half a
tile**: display cell `(i, j)` covers world space `[i-0.5, j-0.5] .. [i+0.5, j+0.5]` and its four
corners are the world cells `(i-1,j-1) NW`, `(i,j-1) NE`, `(i,j) SE`, `(i-1,j) SW`. A `W x H` map
draws `(W+1) x (H+1)` display cells, half-tile-clipped at the border.

For each terrain in **priority order** (low first), the four corners give 4 bits — "is this corner my
terrain" — and those 16 patterns map onto **five shapes and a rotation**. Bit 1 = NW, 2 = NE, 4 = SE,
8 = SW; rotation is clockwise 90° steps.

| bits | corners set | shape | rot |
| --- | --- | --- | --- |
| 0 | — | *nothing drawn* | — |
| 1 | NW | `corner` | 0 |
| 2 | NE | `corner` | 1 |
| 4 | SE | `corner` | 2 |
| 8 | SW | `corner` | 3 |
| 3 | NW+NE | `edge` | 0 |
| 6 | NE+SE | `edge` | 1 |
| 12 | SE+SW | `edge` | 2 |
| 9 | SW+NW | `edge` | 3 |
| 5 | NW+SE | `diag` | 0 |
| 10 | NE+SW | `diag` | 1 |
| 11 | NW+NE+SW | `inv` | 0 |
| 7 | NW+NE+SE | `inv` | 1 |
| 14 | NE+SE+SW | `inv` | 2 |
| 13 | SE+SW+NW | `inv` | 3 |
| 15 | all | `full` | 0 |

That is the whole table; `tools/tiles2/render.py` builds it from four canonical patterns by rotating
the bit word (`b = ((b<<1)|(b>>3)) & 15`) and asserts it covers all 16.

**On flipping and mirroring, plainly:** the owner asked for flip and mirror, and the honest finding
is that for the *transition* problem **rotation alone is sufficient and mirroring adds nothing** —
each of the five shapes is symmetric about a diagonal or an axis, so its mirror is one of its own
rotations. Mirroring earns its keep in two other places, and we should take it there: **decal
variants** (a mirrored tuft is a free second tuft) and **non-architectural stamps**. `diag` (bits 5
and 10) is the case today's fringe system cannot express at all.

### 3.2 The five shapes, and the one rule that makes them compose

Each shape is a signed distance field over the unit display tile:

```
full    d = +1
edge    d = 0.5 - y                                   (terrain in the north half)
corner  d = 0.5 - |(x, y)|                            (quarter disc at NW, radius 0.5)
diag    d = max(0.5 - |(x,y)|, 0.5 - |(x-1,y-1)|)     (two quarter discs, touching at the centre)
inv     d = |(x-1, y-1)| - 0.5                        (everything but a quarter-disc bite at SE)
```

Radius **0.5 is not a taste decision**. It is what makes every boundary cross a tile edge **exactly at
that edge's midpoint**, which is the invariant that lets any shape meet any other shape, in any
rotation, in any variant, with no break. Write it into the contract:

> **The boundary of a mask crosses each tile edge at the midpoint of that edge, perpendicular, and
> nowhere else.**

The raggedness is then a displacement of `d` by two octaves of value noise, **windowed to zero within
0.16 of the tile border**, so the pinned midpoints survive whatever the noise does. Amplitude and
frequency come from the terrain's `edge_style`:

| `edge_style` | amp / freq (octave 1) | amp / freq (octave 2) | for |
| --- | --- | --- | --- |
| `ragged` | 0.105 / 5.5 | 0.045 / 13 | grass, dirt, crop, mud |
| `smooth` | 0.022 / 3 | 0.010 / 7 | paving, plank, tile floor |
| `shore` | 0.060 / 3.5 | 0.022 / 9 | water, and anything with a `border:` band |

Three **variants** per (shape, style) chosen by `hash(i, j, terrain) % 3`, so a long straight bank
never repeats its wobble. 5 shapes x 3 styles x 3 variants x 4 rotations = 180 masks, but rotation is
free in the shader (swizzle the UV), so the stored set is **45 masks per tileset**, generated by the
tool, 128x128 each, 1 bit — `story/field/tilesets/<set>/masks.png`, about 90 KB as an indexed PNG.

### 3.3 Border bands (shorelines)

A terrain may declare `- border: sand 0.085` — a band of a named colour (or a second swatch) drawn in
the *outside* of the mask, `0 > d > -0.085`, in its own pass before the terrain. That is the sand rim
around the lake in `new.png`, and it is what makes water read as water rather than as a blue hole. No
extra art.

### 3.4 Swatches — world-space, not per-cell

Each terrain has **one** seamless swatch, `- swatch: 3x3` tiles (grass, dirt, water) or `2x2`
(paving), stored at the same 128 px per tile as the atlas, sampled in **world space**:
`uv = world_pixel mod swatch_size`. Two consequences worth stating:

- The drawing is no longer cut at cell boundaries, so there is no cell-shaped repeat and nothing to
  match between neighbouring cells.
- The cutter's existing seam healing applies to the swatch instead of the tile. The prototype's
  healer (`tools/tiles2/assets.py`) rolls by half so the wrap is continuous by construction, flattens
  the low-frequency lighting **with a wrapped blur** (a clamped blur leaves a bright rim that the roll
  puts in the middle of the tile, where it reads as a pale cross at every repeat — that cost an hour),
  and hides the remaining cross with a **minimum-error cut** rather than a cross-fade. The old
  cross-fade is visible in `tools/tiles2/out/` history as a pale ribbon at every repeat; do not go
  back to it.
- **Residual honesty:** at 3x3 tiles the repeat is still findable in a still frame if you look for it
  (see `tiles2_s5_detail.png`). Decals and the macro drift are what kill it, and they are cheaper than
  a bigger swatch.

### 3.5 The overlay stack (the owner's follow-up)

The strip `tiles2_stack_strip.png` is base → +decals → +macro drift → +second grass → all.

1. **Calm swatch.** The ground texture is toned down — every pixel's colour pulled ~70 % of the way to
   the swatch mean — so it carries material and light and nothing else. This is the single biggest
   improvement in the whole document, and it costs nothing: it is a line in the ChatGPT prompt
   (*"even, calm, low contrast, no hero details, nothing a player could point at"*) plus a `- calm:`
   check in the cutter. A busy swatch fights the decals and makes the repeat obvious; a calm one
   hides its own repeat.
2. **Decals.** Small cut-out sprites scattered by `hash(cell_x, cell_y, salt)`: sub-tile jitter, one of
   three sizes, a mirror flag, and a pool chosen by the cell's terrain *and its neighbours* — reeds and
   pebbles bias to a cell that touches water, pebbles to a cell that touches a path or paving, nothing
   at all on paving. Density comes from a **low-frequency value-noise field** (period ~4 tiles) so
   they arrive in drifts and patches rather than at an even 0.7 per cell. They straddle cell
   boundaries freely, never collide, and draw under the walkers.
3. **Macro tone drift.** One very-low-frequency noise (period ~7 tiles) shifts the light level by
   -2..+2 shade steps in big soft patches. In our pipeline this is **not art and not a colour
   computation**: it is an offset into the colormap's light rows, `lv += drift(world)`, three lines in
   the fragment shader, and it composes with time of day and lamps for free.
4. **A second grass-family terrain** (dry grass / clover) laid in large organic blobs through the same
   dual-grid masks, at a priority just above grass. One more swatch; no new code at all. This is what
   makes the open field stop being a green sheet.
5. **Motion, later** (described, not prototyped): a 2-frame sway on tuft decals at 2–3 fps with a
   per-decal phase from its hash; water sparkle by palette cycling a few colormap columns
   (`story/palette/cycles.md` already does this); **drifting cloud shadows** as a second, slowly
   translating macro-drift field — the same three shader lines with a scrolling offset, which is a
   remarkable amount of life for no art; and **contact shadows under stamps** drawn from the shade
   table (an ellipse quad at light level -3) instead of painted into the sprite, so a tree's shadow
   turns with the time of day and a stamp's art keeps its hard alpha.

### 3.6 `tiles.md` keys

Existing keys are unchanged. New ones, all optional:

```
## grass
- terrain: yes              # this entry is a terrain layer, not an atlas tile
- priority: 0               # replaces `fringe:`; who overlays whom, low first. Equal = never overlay
- edge_style: ragged        # ragged | smooth | shore
- swatch: 3x3               # tiles; the cutter asks for one image of 3x3 * 128 px
- calm: yes                 # the cutter checks the returned swatch's contrast and warns
- desc: ...

## water
- terrain: yes
- priority: 4
- edge_style: shore
- border: sand 0.085        # a band outside the mask: a palette colour name, or a swatch id
- solid: yes
- frames: 2

## tuft
- decal: yes
- on: grass, grass_dry      # terrains it may sit on
- density: 0.6              # mean per cell at full density-field
- cluster: 4                # tiles; the period of the density noise
- edge_bias: water 2.0, dirt 1.4    # multiplier when the cell touches that terrain
- sizes: 0.75 1.0 1.35
- mirror: yes
- desc: a standing tuft of longer grass, three or four blades
```

### 3.7 `.tmap` changes

- `## ground` — unchanged, one char per cell. **This is the whole point**: an agent still edits a text
  grid and the renderer does the rest.
- `## objects` — unchanged, plus an optional **mirror flag**: a stamp placed as `h` (lower case of its
  legend char) is drawn mirrored. **Nature only.** A building must not be mirrored: every roof, door
  shadow and wall face in the set is lit from the same side, and a mirrored building is lit from the
  wrong one — the standard failure of mirrored shaded art. The tool rejects a mirror flag on an entry
  whose `desc` is a building, and warns on any stamp with an `over:` row.
- `## decals` — **new, optional**: `x y id [flip]`, one per line, for a decal the author wants
  *there* (the flowers on a grave, the rubble by the broken culvert). Hash-scattered decals are
  suppressed within one tile of an authored one.
- Everything else — `## meta`, `## legend`, `## triggers`, lamps, light — unchanged.

### 3.8 How the engine draws it

One extra texture unit and one extra vertex attribute; the existing palette shader does the rest.

- **Textures**: `atlas.png` (stamps and props, unchanged), `swatches.png` (all terrains' swatches
  packed, indexed R8, the same palette), `masks.png` (45 masks, R8, values 0 or 255).
- **Per display cell per terrain**: one quad. UVs into `swatches.png` come from the **world**
  position (so the swatch is continuous across cells); UVs into `masks.png` come from the mask's slot
  with the rotation applied to the corner order — rotation is a UV swizzle, mirroring a UV negation,
  so **no mask is ever stored twice**.
- **Fragment**: `alpha = mask sampled with the same four-tap smoothing the atlas already uses; if
  alpha < 0.5 discard; else output the swatch's colormap colour.` The four-tap blend on the mask is
  what makes a 1-bit edge read soft on screen, exactly as it does for the atlas today.
- **Cost**: worst case ~ (21 x 13) display cells x ~2 terrains present ≈ 550 quads a frame, against
  the ~2600 the ground + fringe pass already pushes. It is cheaper than what we do now.
- **Decals**: per 16x16-cell **chunk**, the hash scatter is evaluated once at map load into a prebuilt
  quad list; the draw is a memcpy of that list into the vertex buffer. Rebuilt only on map change.
- **Collision**: untouched. It reads `solid` off the world cell, and the world cell grid has not
  moved. This is the property that makes the dual grid safe to adopt: it is a **rendering** change.

---

## 4. Tiled interoperability

Worth a `tmj` **export** now and an **import** later, not a format change.

- `tmap → .tmj`: `## ground` becomes one tile layer (the terrain id + 1 as the GID), `## objects`
  another, `## triggers` an object layer with the kind and args as properties, `## decals` a second
  object layer. Tiled's flip bits live in the top three bits of the 32-bit GID (horizontal, vertical,
  anti-diagonal), which is where the stamp mirror flag would go.
- `.tmj → tmap` is the half that matters to the owner and is the harder half (Tiled will happily write
  things our format cannot express). Recommend: write the export first, look at Halm in Tiled, and
  decide then.
- **Do not** adopt Tiled's Wang/terrain sets. They are a drawn-tile system; our transitions are
  computed, and a Tiled terrain set would describe art we do not have.

---

## 5. Migration

**Art.** Per terrain, one swatch replaces four tiles. The `valley` set has five terrains that fringe
(dirt, dirt_rut, paving, paving_worn, water) plus grass: **20 tiles become 6 swatches**, and the six
`*_edge/_corner_out/_corner_in` entries leave `tiles.md`. Add one decal sheet per biome — ~20 cut-outs
on magenta, which is a template sheet the tool already knows how to lay out and cut.

**Maps.** No change to any of the three `.tmap` files. `- fringe: n` becomes `- priority: n` with the
same numbers; a one-line rename in `tiles.md` and a compatibility read in the parser.

**Engine** (`src/tilefield.cpp`, ~2650 lines):

| change | lines |
| --- | --- |
| delete `gen_fringe`, `draw_fringes` and the fringe lookup in `load_tileset` | **-150** |
| swatch + mask texture load, and the `terrain/priority/edge_style/swatch/border` keys | +120 |
| dual-grid draw pass (the 16-case table, the quad, the border band) | +130 |
| mask sampling + macro drift in the shader | +20 |
| decal chunk build + draw | +110 |
| `## decals` parsing, stamp mirror flag | +40 |
| **net** | **+270** |

**Tool** (`story_prompt.py`): mask generation (~120 lines, and it is the prototype's `bake()` verbatim),
swatch healing (already exists, moved to run on swatches), decal sheet template + cut (the template
path already exists), `tileset` prompt changes. The prototype's `assets.py`/`render.py`/`render2.py`
are the working reference for all of it.

**Order.** (1) masks + swatches + dual grid, keeping the old fringe path behind a flag so the three
maps stay playable; (2) calm the swatches; (3) decals; (4) macro drift; (5) second grass terrain;
(6) `tmj` export.

---

## 6. The new ChatGPT asks

Per terrain, **one image**: a `3x3`-tile (384x384) or `2x2` (256x256) **seamless swatch**, drawn edge
to edge, no border, no gradient, no hero detail, even lighting, calm and low contrast. That is the
entire transition art budget.

Per biome, **one decal sheet**: ~20 numbered magenta slots — tufts, flowers, pebbles, cracks, reeds,
fallen leaves — each an isolated object filling its slot, no ground under it.

Unchanged and restated, because they are the rules that have been earned: a building is **at least
2x2 tiles and its art fills its footprint edge to edge**; walk sheets are **9 frames** (S, side, N x
stand, step-A, step-B) and are unaffected by any of this; the style is the flat, calm, cel-shaded look
of `story/sheets/tests/clean_style_test_v1.png`; everything is palettised to the 256-colour master.

**Two hard art rules the prototype earned:**
- A swatch with a **grassy border or a lit side** is a bug, not a style — the low-frequency lighting
  has to be flattened out before it can tile, and flattening it throws away what the model drew. Ask
  for it flat.
- A **decal must be an object on nothing**, not an object on a patch of ground. The prototype had to
  key its decals out of a ground drawing, and the keying is the only part of it that is fragile.

---

## 7. Risks and open questions

**Risks.**
1. **The pinched midpoints.** Every mask boundary crosses a tile edge at its midpoint, so a very long
   straight coastline has a faint rhythm at the 32-px period. Three variants and the noise hide it at
   game scale (look at the lake in `tiles2_new.png`); a fourth variant and a slightly wider window are
   the lever if it shows up on the phone.
2. **Swatch repeat** is still visible in a still frame at 3x3 tiles. Mitigated by calm + decals +
   drift; the fallback is a 4x4 swatch, which costs nothing but the model's pixel budget.
3. **Two terrains of the same priority** cannot transition to each other (they meet with a hard cell
   edge). That is already true today and is what lets `paving`/`paving_worn` sit side by side; it now
   also means grass variants must be a *terrain* with a priority, not a variant tile.
4. **The mask escape hatch** (a hand-drawn `mask_edge.png`) will be tempting and should stay shut
   until something demands it.

**Open questions for the owner.**
1. **How dry is the second grass?** The prototype's dry grass is deliberately obvious
   (`tiles2_s5_all.png`); tell me to halve it or double it.
2. **Decal density.** The prototype averages ~0.7 decals a cell in a drift and near zero between.
   Too sparse, about right, or too busy on the phone?
3. **Does the sand band belong on our water** at all, or is a hard bank more Phantasy Star?
4. **Tiled**: do you actually want to edit maps on the Mac, or is the text format enough? The export
   is a day; the import is a week and a source of format drift.
5. **Cloud shadows** — yes or no? They are three shader lines and they change the feel of an open map
   more than anything else in this document.
