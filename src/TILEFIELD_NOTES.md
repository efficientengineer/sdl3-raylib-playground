# TILEFIELD_NOTES — what the tile field actually does

`TILES.md` is the decision and the contract. This file is the engine's side of it: what is built,
the formats exactly as parsed, where it departs from TILES.md and why, and the Dev controls.

`src/tilefield.cpp` (~1700 lines) and `src/tilefield.h` are a **new, separate module**. `field.cpp`
(the 2.5D block-out, the navmesh, the painted screens) still compiles and still runs, but nothing new
is built on it: `SCR_FIELD` is the tile field, and the old one is one Dev button away.

## What works

- **640x360-class logical view rendered at the screen's own resolution.** The world is 32 logical px to
  the tile and the camera is centred on the leader, snapped to whole logical pixels and clamped to the
  map (a map smaller than the view is centred, so it letterboxes) — none of that changed. What changed
  (2026-09-19, owner: *"I don't want the cutter to reduce resolution"*) is that there is no low-res
  buffer any more: `sc = min(w/vw, h/360)` device pixels per logical pixel, the FBO is the view times
  `sc` (2400x1080 on the phone), and every quad is emitted in **device** pixels with its edges rounded
  through one `dev()` helper, so two tiles that share a logical edge share a device edge exactly and
  there are no cracks. `u_res` is the device size; the geometry is otherwise untouched.
- **256 colours, one byte a pixel, and the light is a table lookup (PALETTE.md / D19).** The atlas and
  every walker sheet are decoded as RGBA, mapped back to palette indices and uploaded `GL_R8`,
  `GL_NEAREST`; the RGBA is freed. Halm's atlas is 24 MB RGBA and 6 MB indexed, each walker 6 MB and
  1.5 MB, so the four sheets plus the atlas went from 48 MB of texture to 12 MB. Every file logs one
  line with its size saving and its colour count, and a file with colours the master palette has not
  logs `N px off-palette (art not converted yet)` and takes the nearest master colour — not-yet-cut
  art still runs, it is only quantised at load. The colour a fragment ends up with comes from the
  **colormap** (`story/palette/colormap.png`, 256 wide, 192 rows = 6 tables x 32 levels): the shader
  fetches four neighbouring indices, looks each up in the row for that fragment's light, and blends
  the four colours bilinearly, premultiplied. Nothing in the engine computes colour — except the CPU
  fallback tables, which exist only so the field still lights if the palette tool has not run.
- **Linear filtering, and the two things that keeps honest.** Atlas cells are 128 px and walker frames
  256x384, drawn at about 0.75x, so both textures are `GL_LINEAR` min and mag — nearest at a non-integer
  ratio is what made the old 32-px art mush. No mipmaps: at 0.75x they buy nothing and mip level 1
  averages across cell borders. Every UV is inset **half a texel**, which is what stops a linear tap at
  a cell boundary from pulling in the neighbouring tile, and a stamp draws its contiguous rows as **one
  quad** (object rows one, `over` rows one) rather than a quad per row, so it has no internal seams.
- **`atlas.json`'s `"cell"`** is the atlas pixels per tile: 128 for the art, 32 when the file is absent,
  which is the placeholder-only path. Placeholders are still generated at 32 px and nearest-upscaled
  into the cell, so half an art atlas and half stubs still works. `GL_MAX_TEXTURE_SIZE` is logged at
  init and an atlas or walker sheet over it is halved (loudly) rather than silently failing to upload.
- **120 fps on the phone** at 2400x1080 with the 24 MB atlas — the fill rate is a non-issue; the log
  line `tilefield: 600 frames in ...` says so every ten seconds.
- **Layers, in this order**: ground → fringe overlays → object stamps → everyone who walks, sorted by
  the row their feet are on → the `over` rows of stamps. One atlas texture, one dynamic VBO, one
  draw call per texture change (ground, fringes and stamps are all the atlas, so it is about four
  calls a frame plus one per walker sheet). Only the visible cells are pushed.
- **Fringes without an autotile explosion.** Three tiles per terrain, rotated by the engine: four
  edges, four outer corners, four inner corners. The shapes follow
  `story/field/tilesets/README.md` exactly — `_edge` is a band along the north, `_corner_out` is the
  north-east corner only, `_corner_in` is everything except a bite out of the south-west — and the
  placeholder generator draws those same three shapes.
- **Placeholders for everything.** With no `atlas.png` at all, all 61 tiles of the valley set are
  generated on the CPU into the atlas at load, keyed on the tile's **name**. Water gets its two-frame
  shimmer. A supplied `atlas.png` is pasted in first and only the cells that are still empty are
  generated, so half a sheet of real art is a normal working state.
- **Grid walking**: four directions, one tile a step, 0.16 s walking and 0.10 s running, input read at
  the end of each step so a held direction glides. A press shorter than 0.07 s turns in place. A
  blocked tile turns you to face it and does not step. Followers snake one tile behind and never
  block anything.
- **NPCs** hold their tile, turn to face you when you talk to them, and `wander r` takes a random
  step every one to three seconds inside r tiles of home, never onto the party, a solid tile or a
  trigger tile.
- **Interaction**: a "!" floats over the leader when the tile faced (or an NPC on it) has something;
  the press opens the box through `field_text.h`, so the words stay in `story/field/text.md`.
  `exit`, `trap`, `zone` and `scene` fire on entering a tile; a `door` fires on the interact press
  **or** on stepping into it facing north. A map change is an eight-frame fade out and in.
- **Hot reload keeps your place**: map, tile, facing, the follower trail, the step count and the debug
  toggles all survive, and everything read back is validated (a tile off the map, or now solid, is
  dropped rather than stranding you in a wall).
- **`map.flag`** works for tmaps exactly as it does for the old field: write a map name into
  `files/map.flag` and the phone goes there with no taps.
- **Desktop capture**: `./capture.sh --tiles hart_yard` renders the player's own view at the phone's
  resolution — **1920x1080**, the party included and trailed out behind the leader so everyone's art is
  visible — to `build_desktop/tiles_<map>.png`, with no phone involved. `--whole` is the old whole-map
  shot (one device pixel per logical pixel), which is how a `.tmap` is read as a town; `--no-walkers`
  empties it; a bare number is the scale. The env behind them is `TILE_CAPTURE`, `TILE_CAPTURE_WHOLE`
  and `TILE_CAPTURE_NO_WALKERS`.
- **`map.flag` now works from the title too**: a fresh start after a force-stop enters the field and
  goes to the named map, so the phone can be driven to a map from the Mac with no taps at all.

## Formats, as implemented

### `story/field/tilesets/<set>/tiles.md`

One `## <name>` per tile or stamp. Keys parsed: `- index: n` or `- index: n WxH`, `- layer:
ground|object|over`, `- solid:`, `- over: n`, `- frames: n`, `- fringe: n`. Anything else (`- desc:`,
`- sheet:`) is read by the art pipeline and ignored here. The atlas is 16 columns of 32x32 and is
sized from the largest `index + h` the file asks for.

- `- solid:` is `yes`, `no`, or the stamp's rows of `#`/`.` **slash separated on one line**
  (`###/###/#.#`). TILES.md writes the rows on their own lines; one line keeps the parser to ten
  lines and reads no worse.
- `- over: n` is how many of a stamp's **top** rows draw above the walkers. TILES.md says the top
  rows of a tall stamp are `layer: over`; with a stamp being one entry, a count is the natural
  encoding of the same thing. `layer: over` on a whole entry still works and means all of it.
- `- fringe: n` is a **priority**, not a flag: a terrain overlays every lower-numbered terrain it
  touches. Equal numbers never fringe each other, which is how `paving` and `paving_worn` sit side by
  side without a seam. A priority with no fringe tiles at all is legal and means "nothing overlays
  me" — that is what `bridge_deck` uses to keep the stream off the planks.
- `- frames: n` are the n cells to the right of the index, cycled at 3 fps.

### `story/field/tmaps/<map>.tmap`

```
## meta        name / tileset / size W H / spawn X Y F / music
## legend      one char, a space, a tile or stamp name
## ground      `size` rows, one char a tile, ground layer only
## objects     `size` rows, '.' is nothing, a stamp's top-left char and '+' for the rest
## triggers    x y w h kind args
```

Trigger kinds: `message <text-id>`, `door <map> <x> <y> <F>`, `exit <map> <x> <y> <F>`,
`zone <table>`, `trap <text-id>`, `scene <cutscene-id>`, and
`npc <walker> <F> <text-id> [wander r]`, which is not a trigger at all — it places an NPC on `x y`.

Loud validation, all of it in the log, none of it fatal: an unknown legend char, a legend name the
tileset has not, a ground tile used in `## objects`, a stamp that runs off the map or overlaps
another (skipped, naming what it hit), a section whose row count disagrees with `size`, a spawn on a
solid tile (moved to the nearest free one, and said so). A missing or broken map file falls back to a
tiny built-in one rather than crashing.

### Walkers

`story/field/walkers/<id>.png`, a 4x4 grid: rows S W E N, columns stand, step-left, stand, step-right.
The contract size is **1024x1536** (frames of 256x384); the engine reads the frame as **sheet/4 by
sheet/4 whatever the sheet is**, so 128x192 still works and only looks softer, and only a size that
cannot be a 4x4 grid is rejected. Every walker logs one line at load — id, whether the file was found,
sheet size, frame size, how transparent it is — because a sheet that keyed nothing out renders as a
black rectangle and used to do it silently. Drawn one tile wide and one and a half tall, centred on the
tile with its feet on the tile's bottom edge, whatever the sheet's resolution.

**Layout is a sidecar, or the 4x4 default.** `story/field/walkers/<id>.json` may say
`{"rows":["S","W","N"], "cols":["stand","a","b"], "frame":[128,192], "mirror_side":true}`: any row set,
any column set, an explicit frame size, and `mirror_side` draws **E from the W row with flipped UVs**.
With no sidecar the sheet is the old 4x4 (rows S W E N, columns stand, step-left, stand, step-right)
and nothing changed. The three-column cycle shows the stepping frame then the stand, alternating `a`
and `b` between steps, so two tiles of walking still play all of it.

**The ids are the story's names.** `PARTY_ART` is `falke`, `ottilie`, `party_c`, `party_d`: the leader
was asked for as `hero`, there is no `hero.png`, and the placeholder figure was therefore drawn over
real art that was sitting on the phone the whole time. A renamed character is a renamed walker file.

**The walk cycle** is `walk_col(parity, k, moving)`: standing still is column 0, and a step shows two
columns — 1 then 2 on one step, 3 then 0 on the next, `parity` flipping per step. Two tiles of walking
therefore play all four frames and the feet alternate, PS4 style. Before this a step showed only column
1 or only column 3, so half of every sheet was never seen. Followers and NPCs run the same function;
NPCs carry their own `parity`.

## Light, as implemented (PALETTE.md's engine half)

- **A map's ambient is `light: <table> <level>` in `## meta`**, default `day 1.0`. `halm`, `hart_yard`
  and `west_road` all carry it.
- **Point lights are `lamp: x y radius level [flicker]` meta lines**, in tiles. They are NOT triggers:
  `story_prompt.py`'s `TRIGGER_KINDS` knows six kinds and rejects a seventh, and this side does not
  edit the tool, while meta keys it does not recognise it ignores — so `tmap check` passes as it
  stands. A `light` TRIGGER (`x y 1 1 light <radius> <level> [flicker]`) is parsed too, for the day
  the tool learns the word. Halm has two lamps at the well and two at the guild-hall door.
- **Per fragment**: `level = max(ambient, Σ level·(1−d/r)²)` over up to 16 lights, in logical view
  pixels; the fractional level blends the two adjacent colormap rows. A point-light pool is looked up
  in its own table — `lamp`/`lantern` if the colormap has one, otherwise `day`, i.e. full-strength
  true colour — and mixed in by how far the point light is above ambient, which is what makes a pool
  warm inside a blue night.
- **A walker is lit at its feet**: one level and one warm weight for the whole sprite, worked out on
  the CPU with the same arithmetic, so a character brightens as a whole rather than in a gradient up
  the body. That is the `lit`/`warm` pair on every vertex; `lit < 0` means "per fragment".
- **The atlas region is a vertex attribute** and the shader clamps its four taps to it. That REPLACED
  the half-texel UV inset for the indexed path: the inset moved the sample point, the clamp makes
  reaching into the neighbouring cell impossible.
- **The colormap is v2** (256x256 RGBA, index 0 already clear, eight tables at the `row0`s
  `colormap.json` gives). The loader never computes a row from `table_index * 32`, takes the pool
  table from `"point_light_table"` (`lamp`), and logs loudly if index 0 ever comes back opaque.
- **Palette cycling** comes from `story/palette/cycles.md` (`water: 115 116 117 118 @ 6`) and is a
  `glTexSubImage2D` of those columns across every colormap row, once per cycle step.
- **Dev row**: a button per table (day/dusk/night/flash/poison/stone), an ambient slider, a lantern
  toggle with a radius slider, and `1 tap (no blend)`, which is the measuring instrument below. All
  of it survives a hot reload (`TfSave`), applied after the map's own `light:` line.
- **Cost, measured on the phone** at 2400x1080 with `glFinish` round the world draw: **8.35 ms/frame
  with the 4-tap blend, 8.29 ms with one tap** — 0.7%, i.e. the twelve-to-sixteen fetches are free
  here and no scale-based fallback is needed. Frame rate is a hard 59.7-60.1 fps because
  `SDL_GL_SetSwapInterval(1)` and the phone was in a 60 Hz mode; the ms number is the honest one.
- **Desktop shots**: `./capture.sh --tiles halm --light night:0.35 --lantern 5` →
  `build_desktop/tiles_halm_night_lantern.png` (env: `TILE_LIGHT=<table>:<level>`, `TILE_LANTERN=<r>`).

## What ships to the phone (2026-09-19: app data went 69 MB → 0.1 MB)

The owner's app was 78 MB of storage: an 8.7 MB APK and 69 MB of app data — 45 stale
`libgame_logic.so.<pid>.<gen>` copies, the parked 3D field's art, and panels from retired scenes.
Three changes make that permanent:

- **`host.cpp` unlinks each reload copy the moment it dlopens it** (the mapping outlives the name) and
  sweeps any `libgame_logic.so.*.*` left by an older build at startup. Copies can no longer pile up.
- **`fast_reload.sh` prunes to a ship list.** Everything it pushes is recorded, and afterwards
  anything in `files/cutscenes` or `files/field` it would not ship is deleted, along with stale .so
  copies (dead pid, or not the live pid's newest gen) and the leftover `files/com.playground/`. It
  prints what it pruned and `files/ <before> KB -> <after> KB`. Panels come from the names in the
  **current `src/cutscene_data.h`**, not from all of `story/panels/`.
- **The parked kinds are not pushed or bundled** — `screens, views, maps, props, tiles, buildings,
  edges`. `./fast_reload.sh --with-parked` restores them for a session on the old 3D field; without
  them that field logs "the 3D field is parked" and uses its built-in fallback map.
- **`deploy.sh` rebuilds the assets from scratch** to the same list (panels named by
  `cutscene_data.h`, portraits, `field/tmaps`, `field/walkers`, one `tilesets/<set>/`
  {atlas.png, atlas.json, tiles.md}, `palette/`) and prints the APK and assets sizes. It used to
  `rsync` all of `story/field/`, debug images included.

After all three, with the indexed art: **APK 12 MB, assets 5.7 MB, `files/` 112 KB on a fresh
install** (6.3 MB once `fast_reload.sh` has pushed the working copies of the art).

## The dual grid (TILES2 / D20) — terrains, masks, decals

`TILES.md`'s "Terrains, masks and decals" is the contract; this is the engine's side.

- **Terrain ids stay on the world cells** and collision, walking and the maps are untouched. The
  **display grid is offset half a tile**: display cell `(i,j)` covers `[i-0.5,j-0.5]..[i+0.5,j+0.5]`
  and its corners are world cells NW `(i-1,j-1)`, NE `(i,j-1)`, SE `(i,j)`, SW `(i-1,j)`. Four bits
  (1 NW, 2 NE, 4 SE, 8 SW — *is this corner my terrain*) index the 16-case table, which is **read from
  `masks.json`**, not retyped. Variant is `h32(i, j, terrain) % 3` with the tool's FNV-1a, so the
  phone picks the same variant as `tmap preview` and the two pictures match.
- **Terrains draw in ascending priority**, and `terr[0]` is the base: it floods the map, and an
  off-map cell counts as it, exactly as the tool's preview does. The base costs **one quad a frame**
  (a screen-filling quad through a `full` mask slot) however big the map is.
- **A terrain is a world-space swatch through a 1-bit mask.** `u_indexed == 2` in the shader: the
  quad's UV is the mask (already rotated by the vertex data — rotation is a UV swizzle, no mask is
  stored twice), and the swatch is sampled at `mod((world px) * 4, swatch size)`, so there is nothing
  to match between cells and no cut in the drawing. The 4-tap colormap blend is kept, with its
  neighbour fetches **wrapped** at the swatch size instead of clamped to an atlas cell.
- **`masks.png` is indexed** (2 inside, 1 outside); the loader thresholds on luminance and uploads
  `GL_R8`. It is sampled LINEAR and thresholded with a one-texel `smoothstep(0.42, 0.58)` — a hard
  test on a nearest sample crawls along every boundary at the 1.33x minification the field runs at.
  *(Deviation from "1-bit": the boundary is one texel soft. It reads as a hard Phantasy Star edge and
  does not shimmer when the camera moves.)*
- **Swatches**: `swatches/<id>.png` when the cutter has made one, otherwise a flat palette colour per
  terrain with fine low-contrast mottling — the right *shape* with no art at all, which is what the
  maps run on today. The hue table is in `tf_load_swatch`.
- **Decals** are scattered with the tool's arithmetic to the letter (density x edge_bias x a
  `vnoise` density field of period `cluster`, `h32(x, y, id)` for the roll, sub-tile jitter, one of
  three sizes, h-flip), never on a solid tile or under a stamp. With no decal art in the atlas yet
  **nothing is drawn** — the count is logged instead. Dev: a density slider (rebuilds the scatter)
  and a **Print decals** button.
- **Static geometry.** The ground and the decals are built **once per map load** into one VBO
  (halm: 913 ground quads, 5478 verts, 0.23 MB) and the camera moves them in the vertex shader
  (`u_pscale`/`u_poff`), so nothing is rebuilt per frame. Only the visible display rows are drawn:
  one `glDrawArrays` per terrain (nine on halm) plus the base quad. Stamps and walkers stay in the
  dynamic batch, which is what makes animated tiles free — no geometry to rebuild.
- **`## flips`** mirrors a stamp placement by its top-left cell (nature only; `tmap check` enforces
  that a building never carries `- flip: h`). **`## decals`** hand-placements are parsed and stored.
- **`pass: NESW`** is honoured when walking: the letters present are the sides that may be walked
  **through**, so a counter is solid but open on one side. A tile with no `pass:` line behaves exactly
  as it always did. **`tag:`** is carried and shown by **Print tile**.
- **`cycle: water`** hands the terrain to the palette cycle in `story/palette/cycles.md` — the
  colormap columns already animate, so a cycling terrain costs no atlas cells and no extra draw.
- The old **fringe path is gone**: `gen_fringe`, `draw_fringes`, `fr_edge/fr_out/fr_in` and the
  `fringe:` key are deleted.

## Weather: macro drift and cloud shadows (light LEVELS, not colour)

Both are per-fragment offsets to the colormap row, computed from the WORLD position, so they stay put
as the camera moves and carry over unchanged to any later renderer.

- **Drift** — one octave of value noise, period **12 tiles**, shifts the level by up to **±1.5 rows**.
  **The dual-grid ground only** (terrain quads push `lit = -3`; stamps and decals push `-1`), so a
  house does not ripple with the field it stands in. A terrain's own `- drift:` can switch it off. Per map: `drift: <strength>` in `## meta`, default 1.0, 0 = off.
- **Clouds** — two octaves, period **25 tiles**, scrolling about **0.3 tiles/s**, soft-thresholded
  (`smoothstep(0.42, 0.68)`) and darkening by up to **4 rows**. Affects ground, stamps and walkers
  alike. Per map: `clouds: on|off|<strength>`; with no line it is **on under `day` and `dusk`** and off
  under any other table, so an interior gets none.
- One octave of value noise is a field of same-sized blobs; the second octave is what makes it read as
  cloud. The thresholds were set from a measurement, not by eye: against a `0:0` capture, about half
  the ground is more than 5% darker, the darkest about 18%, median 7% — gentle sun and shade.
- Walkers take the cloud per fragment rather than one value at the feet (unlike the lantern light).
  A sprite is one tile and the cloud period is 25, so the variation across a body is under a fifth of a
  row; keeping it in the shader avoids a second copy of the noise in C that could drift out of step.
- Dev: **Clouds** and **Drift** checkboxes with strength sliders; both ride the reload blob. On the
  Mac: `./capture.sh --tiles halm --weather <drift>:<clouds>` (env `TILE_WEATHER`).

## The dialogue box (`src/dialogue.h`), shared with the cutscene player

One header, two users: `star_logic.cpp`'s cutscene box and this file's field message box.

- **Pagination.** A line longer than the box is split into pages at word boundaries, preferring a
  sentence end in the last quarter of a page. Before this a long line ran out of the bottom of the box
  and could not be read at all — the tap went straight to the next line.
- **The tap ladder**: typing → finish the page; page complete with more pages → turn the page; last
  page → next line (or clear the field box). The typewriter runs per page.
- **The marker** blinks in its own corner of the content rect: one white ▼ when the tap moves on, two
  yellow ▼▼ when this line has more pages, so it is obvious whether a sentence is about to be lost.
- **The content rect** is the box minus one uniform pad (0.6 of a line) on all four sides. The speaker
  name sits at the top of it and the text under the name; a box with no name (Narrator, an examine
  line) centres its text block vertically. The text width also leaves the marker its gutter.
- The landscape cutscene box is **0.325 of the stage tall** (it was 0.22), which is a name plus three
  lines at the current text size — the size the real lines actually need.
- Panel reveal and mood change happen in `star_goto`, i.e. on a line's **first page only**, because
  turning a page never goes through it. The page index rides the reload blob (`STR9`).
- **ImGui 1.92 binds its own sampler object over every texture's own filter settings.** Setting
  `GL_LINEAR_MIPMAP_LINEAR` on a panel changed not one pixel until the draw was bracketed with a
  callback that binds a mipmapping sampler and puts ImGui's back (`mip_on`/`mip_off` in
  `star_logic.cpp`). Panels and portraits are full-resolution paintings minified into their boxes — a
  480x771 portrait into about 200x304 — so they want mipmaps; the font atlas, which has no mip levels,
  must keep ImGui's sampler, which is why the bracket is per image and not global.

## Deviations from TILES.md, and why

1. **The view fills the width.** TILES.md says 640x360. The LOGICAL view is the phone's aspect at a
   fixed 360 height, clamped to 640..1024 wide — the same trick `field.cpp` uses, so a 20:9 phone is not
   pillar-boxed. On a 16:9 screen it is exactly 640x360. The FBO behind it is that times the device
   scale.
2. **`- over: n` instead of a separate `layer: over` entry per row** (above).
3. **`- fringe:` is a priority number**, not a yes/no (above). Twelve cases from three tiles, as
   TILES.md asks, but two terrains can now meet without either fringing the other.
4. **`- solid:` rows are slash separated** on one line (above).
5. **The capture has two shapes.** The default is the player's view at 1920x1080, which is how the art
   is judged at the size it will be seen; `--whole` is the whole map, which is how the town's shape is
   judged. A 640x360 shot of a 48x36 town answers neither question on its own.
6. **Party of four in the save, two in play.** `TF_PARTY` is 4 and the snake is written for four; the
   maps start with the two the intro leaves us with.

## Dev controls

The Dev panel's field row, when the tile field is on screen:

- the map, the leader's tile, facing, step count and map size, live;
- a button per tmap (`halm`, `hart_yard`, `west_road`) and **Respawn** (reloads the current map);
- **Collision** — red wash on every solid tile;
- **Triggers** — every trigger rectangle, coloured by kind (messages cyan, exits green, the rest amber);
- **Coords** — the map, tile, facing and step count drawn in the corner of the view itself;
- **No clip** — walk through everything;
- **Print tile** — puts the tile under you, and whether it is solid, into the Dev messages.

Beside them, at the top of the panel: **Old 3D field** toggles `SCR_FIELD` back to `field.cpp` (and
back again). Everything else on the phone is unchanged.

Touch: floating stick on the left half — resolved to the dominant axis with a hysteresis band so a
thumb near a diagonal does not flicker — and the right half taps to interact, holds to run. On the
desktop: arrows or WASD, Space/Enter/Z to interact, Shift to run.

## The maps

`halm.tmap` (48x36) is the town: the square grown round the well, lopsided and ragged, the guild hall
long down its east side, lanes three tiles wide that bend and pinch, the carters' empty yard in the
north-west with its one handcart, the walled grain yard in the south-east, the crooked stream across
the north with two footbridges, ten houses of five different stamp sizes, trees in ones and twos, an
orchard south-west, barley north-east. West edge to `west_road`, the north lane over the bridge to
`hart_yard`. `hart_yard.tmap` (24x20) is the walled yard above it: the ladder house as an L, six
practice posts in a crooked arc, the water barrel, the gate on the south. `west_road.tmap` (40x18) is
the road out, bending, pinched to one cart's width at the washed-out culvert where a plank is gone;
its west end is a `TODO` message, not an exit, because the bridge beyond it is not built.
