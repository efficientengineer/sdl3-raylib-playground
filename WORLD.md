# WORLD — the voxel field (D22/D23)

The walking-around game. Replaces `FIELD.md` (2.5D block-out and painted views) and `TILES.md` (the
Sega tile field), both retired on 2026-09-21 with everything that read them; `story/DECISIONS.md`
D24 records it, and the `legacy-final` git tag holds the originals. What survives from them is what
is still true: the map design rules, the `.tmap` text format, and the asset sizes.

The engine is `src/voxfield.cpp`. The contract in `src/` is that side's; this file is what the story
and art pipeline has to agree with.

---

## 1. What the world is made of

**Voxels, plus billboards.** Terrain and buildings are geometry the engine builds — from palette
ramps, shader detail and rule-based house assembly — and **need no generated art at all.** That is
the whole point of the change: the two previous field designs each needed a folder of drawn tiles
per map before anyone could walk anywhere, and both stalled there.

What is drawn is what moves or what has to read as a *character*:

| what | where it lives | how it is made |
|---|---|---|
| walk sprites | `story/field/walkers/<id>.png` (+ `.json`) | `./story_prompt.py walker <Name>` |
| field sprites (billboards) | `story/field/sprites/<id>.png` (+ `.json`) | `./story_prompt.py sprites <id>…` |
| the colours everything is lit in | `story/palette/**` | `./story_prompt.py palette build` |
| terrain ids, scatter decals | `story/field/tilesets/valley/**` | **frozen** — already drawn |
| the maps | `story/field/tmaps/<map>.tmap` | written by hand |
| what the world says | `story/field/text.md` → `src/field_text.h` | written by hand, exported |

`story/field/tilesets/valley/` is a survivor of the tile era: the voxel field still reads its
`tiles.md` for terrain ids, its `atlas.png` for billboard props, and its `decals/` for the things it
hash-scatters over the ground. Its packages under `story/packages/tilesets/` are **frozen** — drawn,
done, never on the to-do list, and kept only so the archived returns in `story/sheets/` could be cut
again if the palette were ever refitted.

### Sizes (from the 2026-09-19 study; the study itself is gone, its answer is not)

- **Store art at the largest size it is ever displayed at, and no further above it.** A rung that
  lands exactly on the display size loses nothing the display path was not already discarding.
- **Walker frame: 128x192** (4x the 32x48 logical frame) — measured as exactly the phone's display
  size at 1440p. A sheet is nine frames, 384x576: rows S, side, N x columns stand, step-A, step-B,
  and the engine mirrors the side row. 256x384 was twice what was needed in each axis.
- **Portrait: about 288 tall**, aspect kept.
- **Atlas tile / stamp: 128 px per tile.** 64 is visibly mushy.
- **Panel: about 16.9 px per percent of stage width.** A wide panel wants ~881x401, so a six-panel
  sheet gives each about 250k px — a scene with a wide establishing shot should be one sheet a page.
- **Never snap to the art-pixel grid** for anything with a face or fine detail. ChatGPT's fat pixels
  are not on a clean lattice, and one-pixel-per-art-pixel versions break eyes, mouths and thin ties.

---

## 2. Field sprites — the billboards

One flat cut-out standing upright in the world, always facing the camera, **anchored bottom-centre**:
the bottom edge of the image is where it meets the ground, and it is centred on its cell at 64 px to
the cell.

- Data file: **`story/field/sprites.md`** — one `## <id>`, a one-line visual description, a
  `- footprint: WxH` in map cells, and optionally `- frames: N` (2 or 3) with a `- frame2:`/`- frame3:`
  line saying what *moves*, plus `- loop:` and `- fps:`.
- Output: **`story/field/sprites/<id>.png`**, an indexed PNG on the master palette, index 0
  transparent. A still has no sidecar. A multi-frame sprite is one horizontal strip and also writes
  **`story/field/sprites/<id>.json`**:
  `{ id, frames, frame: [w,h], sheet: [w*frames,h], footprint: [W,H], fps, loop, anchor }`.
  **Read `frame` from the json; never divide the sheet.**
- This is **not** a walker sheet: no direction rows, because a billboard never turns.
- The frames of one sprite are trimmed to **one shared box**, so they stay registered. Trimming each
  frame to its own silhouette — which is what the old prop cut did — makes a creature that opens its
  mouth a pixel wider in frame two, and the flip reads as the whole animal twitching sideways.
- The engine loads them by file-exists and draws a placeholder otherwise, so a map may place a
  sprite before its art is drawn.
- **The engine never reads `sprites.md`.** It binds a sprite by the id on the trigger line and loads
  `story/field/sprites/<id>.png`, caching a miss. So `sprites.md` is authoritative for the *artist* —
  it is what sizes the slot and writes the prompt — and the engine stays id-driven. The consequence
  is that a typo in a sprite id is not a crash and not a black square: it is a silent placeholder,
  and `tmap check` is the only thing that catches it.

---

## 3. Maps — `story/field/tmaps/<map>.tmap`

Plain text, written and read by hand. `./story_prompt.py tmap check <map>|--all` validates it (and
runs inside `check --all`); `tmap preview <map>` renders it on the Mac.

> **`tmap check` is mandatory before a map ships, and it is the only gate there is.** The engine
> validates none of these ids: an unknown `fight`, `pickup` or `sprite` id still *runs*, firing the
> event with whatever string is on the line, and a wrong `message` or `npc` text id shows as silence
> rather than as an error. Nothing downstream will fail loudly. A map that has not been through
> `tmap check` is not finished.

```
## meta
name: halm
tileset: valley
size: 48 36
spawn: 24 20 S
music: hope
light: day 1.0            # a colormap table and a level 0..1 (PALETTE.md)
lamp: 12 9 6 0.8 flicker  # x y radius level [flicker] — a point light, looked up in the `lamp` table
base: 4                   # the map floor in voxels. Optional, default 4
landmark: the lopsided square

## legend              # one char -> a tile or stamp id from the tileset's tiles.md
. grass
, dirt
# paving
H house_a              # a stamp: the char marks its TOP-LEFT tile, the rest of the footprint is '+'

## ground              # `size` rows, one char per cell, ground layer only
## objects             # `size` rows, '.' = nothing; stamps by top-left char, '+' for the rest
## height              # optional. `size` rows, one char per cell:
                       #   0-9 then a-z = BASE 36, the cell's surface height in VOXELS above the
                       #                  map base (0..35). A voxel is half a walk cell.
                       #   '.' or a space = unauthored: procedural, exactly as before
                       #   '~' = a bend: the mean of its authored orthogonal neighbours
                       # A map with no '## height' section at all behaves as it always did. An
                       # authored cell is exempt from the engine's two-voxel neighbour clamp, so a
                       # cliff between two authored cells is legal and is NOT a validation error.
                       # Stamps and trigger boxes flatten to their top-left cell's height.
## decals              # x y <id> [flip] — placed by hand where the scatter cannot be trusted
## flips               # x y — the top-left cell of a stamp placement drawn MIRRORED
## triggers            # x z w d <kind> [args]
```

### Trigger kinds

| kind | args | what it does |
|---|---|---|
| `exit` | `<map> <x> <y> <facing>` | leave the map |
| `door` | `<map> <x> <y> <facing>` | the same, on the cell in front of a door |
| `message` | `<text_id>` | an examine line from `story/field/text.md` |
| `npc` | `<walker> <facing> <text_id> [wander r]` | somebody to talk to |
| `zone` | `<table>` | an encounter table |
| `trap` | `<id>` | |
| `light` | `<radius> <level> [flicker]` | a point light written as a box |
| `scene` | `<scene_id>` | play a cutscene. Must be a scene some chapter list in `playlist.md` names |
| `fight` | `<encounter_id>` | start a battle, on walk-in. Also draws that id as a sprite |
| `pickup` | `<item_id> <text_id>` | a takeable, on interact, once |
| `goal` | `<Words_with_underscores>` | the on-screen goal line, six words at most |
| `sprite` | `<sprite_id>` | stand a billboard here |

Any trigger line, `npc` included, may carry a bare trailing **`nightsight`**: hidden and
non-interactive until the party has night sight.

Collision comes from the tiles (`solid`), the height ledges and the NPCs. Examine text and NPC lines
live in `story/field/text.md` by id and are exported to `src/field_text.h`; the display name of an
NPC is story too, so it is a `- name:` line there and takes name tokens like everything else.

---

## 4. Map design rules

Binding for every map. Shape tells the player who made a place.

1. **Shape follows the maker.** People *grow* places; the ancients *built* them. A human settlement
   is organic: it started from one reason (a well, a ford, a crossroads, shelter under a hill) and
   spread along the easiest walking lines, so nothing in it is straight for longer than two or three
   houses. An ancients' place is geometric: gridded, symmetric, repeated, straight, at a scale no
   person needs. The contrast is a feeling the game relies on. **Grids are for the ancients only.**
2. **Start from the reason.** Mark the origin point first (Halm: the well). The oldest, densest,
   most crooked buildings cluster round it; newer and sparser toward the edges.
3. **Paths before buildings.** Draw where people walk — origin to each exit, to the water, to the
   fields — as bending desire lines. Then put buildings along them, fronts to the path. Streets fork
   at odd angles, pinch where they are old, widen into irregular open space, some dead-end in a yard.
4. **Terrain decides.** Water and height come before the plan. Streams are never straight and never
   parallel to a street. On a slope, paths curve or switchback along the contour; straight ramps and
   steps exist only where something was built to make them.
5. **Nothing uniform.** Building sizes, rotations (no two neighbours parallel unless they share a
   wall), gaps, plot shapes, the distance between trees. If two things are the same distance apart
   as two other things, move one.
6. **Ragged edges.** A town dissolves into orchards, walls, hedges, sheds and fields; it never stops
   at a clean line. The map border hides behind terrain, trees or a bend.
7. **One landmark always in view.** The player orients by the well, the hall, the hill. Name it in
   `## meta` as `landmark:`.
8. **Small and dense.** Six to twelve buildings on a town map, every one with a reason to walk to it
   — a door, an NPC, a find. A big empty map is a mistake, not scope.
9. **One memorable thing per map.** The lopsided square, the stream through the middle, the road
   that climbs round the hill.
10. **Ancient places are the inverse of all of the above.** Perfect grids, mirror symmetry, identical
    spacing, right angles, dead-flat floors, straight lines that run out of view, seams too tight to
    see, materials that do not weather, doors sized for something taller. Use every one of those
    tells there and nowhere else. Where people live on top of ancient work, the organic grows over
    the grid and the grid shows through in patches.
11. **Paths are wide.** Streets 3-4 cells across, lanes never under 2, and a deliberate pinch is
    still comfortable to walk. (Owner: the first pass was half this and felt cramped.)
12. **Roads between places** bend with the land, have a reason for every bend — a rock, a wet patch,
    an old wall — and offer side turnings that go somewhere small.
13. **The graph-paper test.** Look at the map from above. If it reads as graph paper and the place
    is not ancient, it is wrong. Redo it before anything else.

Checklist before hand-off: origin point named · desire lines drawn before buildings · no straight
street longer than three houses · no two neighbouring buildings parallel · stream and slope paths
curved · edges ragged and the border hidden · landmark in every view · every building has a reason ·
`landmark:` set · graph-paper test passed.
