# TILEFIELD_NOTES — what the tile field actually does

`TILES.md` is the decision and the contract. This file is the engine's side of it: what is built,
the formats exactly as parsed, where it departs from TILES.md and why, and the Dev controls.

`src/tilefield.cpp` (~1700 lines) and `src/tilefield.h` are a **new, separate module**. `field.cpp`
(the 2.5D block-out, the navmesh, the painted screens) still compiles and still runs, but nothing new
is built on it: `SCR_FIELD` is the tile field, and the old one is one Dev button away.

## What works

- **640x360-class internal view**, nearest upscale, camera snapped to whole pixels, centred on the
  leader and clamped to the map; a map smaller than the view is centred, so it letterboxes.
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
- **Desktop capture**: `./capture.sh --tiles halm` renders the **whole map** to
  `build_desktop/tiles_halm.png` with no phone involved. That is how a `.tmap` is read as a town.

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

`story/field/walkers/<id>.png`, 128x192, four rows S W E N by four columns (stand, step, stand,
step). Anything else logs and falls back to a procedural figure whose colour comes from the id's
hash, so the leader, the follower and every NPC are already different people with no files at all.
Feet on the tile, head over the tile above.

## Deviations from TILES.md, and why

1. **The view fills the width.** TILES.md says 640x360. The FBO is 1024x360 and the used width is the
   phone's aspect at a fixed 360 height, clamped to 640..1024 — the same trick `field.cpp` uses, so a
   20:9 phone is not pillar-boxed. On a 16:9 screen it is exactly 640x360.
2. **`- over: n` instead of a separate `layer: over` entry per row** (above).
3. **`- fringe:` is a priority number**, not a yes/no (above). Twelve cases from three tiles, as
   TILES.md asks, but two terrains can now meet without either fringing the other.
4. **`- solid:` rows are slash separated** on one line (above).
5. **The capture is the whole map, not a view.** A 640x360 shot of a 48x36 town says nothing about
   whether it reads as a town, which is what the capture is for.
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
