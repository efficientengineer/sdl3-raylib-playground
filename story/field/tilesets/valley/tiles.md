# valley — the tileset of Halm and the hill above it (v2, TILES2/D20)

The engine reads this file (`src/tilefield.cpp`); the art pipeline generates the set's art from it.
One `## <id>` per entry, and an entry is one of three **kinds**:

- **`- kind: terrain`** — ground you walk on. It has **no atlas index**: it is one seamless
  **swatch** (`- swatch: WxH` tiles, stored at 128 px a tile in `swatches/<id>.png`) sampled in world
  space, and the boundary where it meets a lower-priority terrain is a **generated 1-bit mask**, not
  art. `- priority:` (low drawn first, equal never overlays), `- edge_style: ragged|smooth|bank`,
  optional `- border: <terrain-or-colour> <tiles>` for a sand or foam band outside the mask,
  `- drift:` for macro light drift and `- cycle: <name>` to hand the terrain to a palette cycle.
  The old `_edge` / `_corner_out` / `_corner_in` tiles are **gone**: three drawn tiles a terrain that
  had to match each other became nothing to draw at all.
- **`- kind: decal`** — a small loose object the game scatters over the ground by the hundred, cut out
  against magenta with **nothing under it**. `- on: <terrain>[, ...]`, `- density:` (mean per cell at
  full density), `- cluster:` (tiles; the period of the density noise, so they arrive in drifts),
  `- edge_bias: <terrain> <factor>` (more of them where a cell touches that terrain), `- sizes:`,
  `- flip: h`, `- size_tiles:`. Decals are the variety that used to be `grass_tuft` and
  `grass_flower`, without a tile-shaped repeat.
- **everything else** — an atlas tile or **stamp**, unchanged: `- index: n` (or `- index: n WxH`,
  row-major from its top-left cell, 16 columns to the row), `- layer: ground|object|over`,
  `- solid: yes|no|<rows of #/.>`, `- over: n` top rows drawn above the walkers, `- frames: n`.
  New and optional on any entry: `- pass: NESW` (which sides may be walked **through** — a counter,
  a one-way ledge) and `- tag: <word>` (counter, bush, damage… what the entry *is* to the game).

**The owner's rules, and they are load-bearing.** What looks solid is solid, and nothing solid is
small: a building is **at least 2x2 tiles** and its art **fills its footprint edge to edge** — walls
to the left and right edges of its tiles, the front wall down to the bottom edge — so there is no
strip of grass inside a solid tile that looks walkable and is not. `tmap check` warns when a solid
stamp's art leaves more than 15% of a solid tile transparent. Nature stamps may be mirrored on a map
(`## flips`); **buildings may not** — every roof and wall face in the set is lit from the upper left,
and a mirrored building is lit from the wrong side.

Every tile still has a procedural placeholder in the engine keyed on its name, so the maps are
walkable and readable with no art at all.


### Terrains — one swatch each, the edges are computed

## grass
- kind: terrain
- priority: 0
- swatch: 4x4
- edge_style: ragged
- solid: no
- drift: 1.0
- desc: short river-valley turf seen from straight above, even and quiet, worn thin to pale soil in places, no single blade readable and nothing the eye stops on

## grass_dry
- kind: terrain
- priority: 1
- swatch: 4x4
- edge_style: ragged
- solid: no
- drift: 1.0
- desc: the same turf gone dry and strawy, a shade paler and warmer than the green, still even and quiet; it is laid over the green in big soft blobs, so it must sit almost flat against it

## crop
- kind: terrain
- priority: 2
- swatch: 3x3
- edge_style: ragged
- solid: no
- desc: a worked field of low young grain in rows, the soil showing between them, seen from straight above

## mud
- kind: terrain
- priority: 3
- swatch: 3x3
- edge_style: ragged
- solid: no
- desc: churned wet earth at a river edge or a gateway, dark, with shallow standing water in the hollows

## dirt
- kind: terrain
- priority: 4
- swatch: 3x3
- edge_style: ragged
- solid: no
- desc: packed earth of a village lane, dry and pale, a little gravel worked into it

## gravel
- kind: terrain
- priority: 5
- swatch: 3x3
- edge_style: ragged
- solid: no
- desc: loose grey river shingle, small stones of mixed size, dry

## paving
- kind: terrain
- priority: 6
- swatch: 2x2
- edge_style: smooth
- solid: no
- desc: laid stone of the village square, blocks of uneven size, the mortar dark between them

## bridge_deck
- kind: terrain
- priority: 7
- swatch: 2x2
- edge_style: smooth
- solid: no
- desc: weathered timber decking boards laid across a bridge, gapped, nail heads at the joins

## water
- kind: terrain
- priority: 8
- swatch: 3x3
- edge_style: bank
- solid: yes
- cycle: water
# - border: sand 0.085   # the owner chose a HARD bank (Phantasy Star); uncomment for a sand rim
- desc: shallow running stream water, the current read as long soft bands of two close blues

### Decals — scattered by hash, never on a grid

## tuft
- kind: decal
- on: grass, grass_dry
- density: 0.55
- cluster: 4
- edge_bias: water 1.6, mud 1.5
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.55
- desc: a standing tuft of longer grass, three or four blades leaning one way

## tuft_tall
- kind: decal
- on: grass, grass_dry
- density: 0.22
- cluster: 5
- edge_bias: water 2.2
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.8
- desc: a taller clump of coarse grass, a hand's height, leaning one way

## clover
- kind: decal
- on: grass
- density: 0.3
- cluster: 3
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.5
- desc: a small low patch of round clover leaves

## flower_white
- kind: decal
- on: grass
- density: 0.25
- cluster: 5
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.4
- desc: three or four small pale field flowers on thin stems

## flower_red
- kind: decal
- on: grass, grass_dry
- density: 0.14
- cluster: 6
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.4
- desc: two small deep red field flowers on thin stems

## daisy_patch
- kind: decal
- on: grass
- density: 0.12
- cluster: 6
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.7
- desc: a loose scatter of seven or eight tiny white daisies

## pebble
- kind: decal
- on: dirt, gravel, mud
- density: 0.5
- cluster: 3
- edge_bias: water 1.5, paving 1.3
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.35
- desc: one small rounded grey stone

## pebbles
- kind: decal
- on: dirt, gravel, water
- density: 0.35
- cluster: 3
- edge_bias: water 1.8
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.6
- desc: three or four small rounded stones lying close together

## stone_flat
- kind: decal
- on: grass, dirt, gravel
- density: 0.12
- cluster: 5
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.7
- desc: one flat weathered stone half sunk into the ground

## crack
- kind: decal
- on: paving, dirt
- density: 0.3
- cluster: 4
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.6
- desc: a thin dark crack running across the ground, branching once

## crack_moss
- kind: decal
- on: paving
- density: 0.18
- cluster: 4
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.6
- desc: a crack in laid stone with a thin line of moss grown along it

## grass_sprout
- kind: decal
- on: paving, dirt, gravel
- density: 0.2
- cluster: 4
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.35
- desc: a few blades of grass pushing up through a joint

## reed
- kind: decal
- on: water, mud
- density: 0.4
- cluster: 3
- edge_bias: water 2.0
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.85
- desc: three tall thin reeds standing straight up, a dark seed head on each

## reed_clump
- kind: decal
- on: water, mud
- density: 0.2
- cluster: 4
- edge_bias: water 2.0
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 1.0
- desc: a clump of eight or nine reeds of different heights

## lily_pad
- kind: decal
- on: water
- density: 0.3
- cluster: 3
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.5
- desc: two flat round lily pads, one notched

## mushroom
- kind: decal
- on: grass, mud
- density: 0.1
- cluster: 6
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.3
- desc: one small pale mushroom with a domed cap

## mushroom_pair
- kind: decal
- on: grass, mud
- density: 0.07
- cluster: 6
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.45
- desc: two small pale mushrooms of different heights, close together

## leaf_fall
- kind: decal
- on: grass, grass_dry, dirt, paving
- density: 0.3
- cluster: 5
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.35
- desc: two fallen leaves lying flat, dry and curled at one edge

## twig
- kind: decal
- on: grass, dirt, gravel
- density: 0.2
- cluster: 4
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.6
- desc: a short bare twig lying flat with one fork in it

## straw
- kind: decal
- on: dirt, gravel, crop
- density: 0.25
- cluster: 4
- edge_bias: crop 1.8
- sizes: 0.8 1.0 1.25
- flip: h
- size_tiles: 0.55
- desc: a small scatter of loose straw stems lying flat, dropped from a cart

### Tiles and stamps — the atlas, unchanged

## fence_ew
- index: 32
- layer: object
- solid: yes
- desc: three rails of a split fence running east and west, posts at both ends

## fence_ns
- index: 33
- layer: object
- solid: yes
- desc: the same split fence running north and south, seen along its length

## fence_corner
- index: 34
- layer: object
- solid: yes
- desc: the corner post of a split fence with a stub of rail each way

## wall_ew
- index: 35
- layer: object
- solid: yes
- desc: a dry stone wall a little above waist height running east and west, capped with flat stones

## wall_ns
- index: 36
- layer: object
- solid: yes
- desc: the same dry stone wall running north and south

## wall_corner
- index: 37
- layer: object
- solid: yes
- desc: the corner of a dry stone wall, the cap stones turning

## hedge
- index: 38
- layer: object
- solid: yes
- desc: a thick clipped field hedge, dark and uneven along the top

## practice_post
- index: 39
- layer: object
- solid: yes
- desc: a shoulder-high post driven into a yard and split down one side from use

## barrel
- index: 40
- layer: object
- solid: yes
- desc: a water barrel with iron hoops, the lid off and the water standing in it

## crate
- index: 41
- layer: object
- solid: yes
- desc: a nailed wooden crate with a batten across the face

## boulder
- index: 42
- layer: object
- solid: yes
- desc: a single grey field stone too big to shift, moss on its north side

## signpost
- index: 43
- layer: object
- solid: yes
- desc: a plank sign on one leg at a lane end, the board blank

## trough
- index: 44
- layer: object
- solid: yes
- desc: a long stone trough with water standing in it

## grain_sack
- index: 45
- layer: object
- solid: yes
- desc: two full grain sacks tied at the neck, leaning together

## stump
- index: 46
- layer: object
- solid: yes
- desc: a felled tree stump with the axe cuts still on it

## gate_post
- index: 47
- layer: object
- solid: yes
- desc: one squared stone gate post standing where a wall opens

## well
- index: 48 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: the capped village well, stone ring, two posts and a small roof, a new rope on the windlass. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## handcart
- index: 50 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: a two-wheeled handcart standing empty with its shafts down. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## woodpile
- index: 52 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: split logs stacked end-on against a yard wall. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## haystack
- index: 54 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: a small hayrick, roped over the top. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## market_stall
- index: 56 2x2
- layer: object
- solid: ../##
- over: 0
- desc: a trestle under a striped awning, nothing laid out on it. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## tree
- index: 80 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a broad valley tree, dense crown seen from above and slightly in front, one trunk at the foot. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## tree_2
- index: 82 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a second tree, narrower and darker than the first, so two together do not read as a pair. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## tree_bare
- index: 84 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a dead tree, bare limbs, the bark off one side. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## orchard_tree
- index: 86 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a low pruned fruit tree on a short trunk, the crown kept round. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## house_a
- index: 128 3x3
- layer: object
- solid: yes
- over: 1
- desc: a small valley house, plastered walls on a stone footing, red pantile roof, one door and one shuttered window on the south face. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## house_b
- index: 131 3x3
- layer: object
- solid: yes
- over: 1
- desc: a second small house, timbers showing in the plaster, the roof a shade darker. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## shed
- index: 134 3x3
- layer: object
- solid: yes
- over: 1
- desc: a plank outbuilding with a shallow board roof and a wide doorway. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## stable
- index: 137 3x3
- layer: object
- solid: yes
- over: 1
- desc: a stable with a split door standing open and straw trodden at the threshold. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## hut
- index: 140 3x3
- layer: object
- solid: yes
- over: 1
- desc: the smallest house in the town, one room, turf growing on the roof edge. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## house_c
- index: 176 4x3
- layer: object
- solid: yes
- over: 1
- desc: a wider house with two windows and a bench beside the door. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## barn
- index: 180 4x3
- layer: object
- solid: yes
- over: 1
- desc: a barn with tall boarded doors and a hoist beam under the gable. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## grain_shed
- index: 184 4x3
- layer: object
- solid: yes
- over: 1
- desc: a grain store on stone staddles, the boards tight, one small hatch high in the wall. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## house_d
- index: 188 4x3
- layer: object
- solid: yes
- over: 1
- desc: a house with an outside stair to an upper door and washing lines at the side. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## ladder_house
- index: 224 4x4
- layer: object
- solid: yes
- over: 2
- desc: a two-storey house with a ladder left leaning against the eaves and slates missing off the ridge. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## house_e
- index: 228 4x4
- layer: object
- solid: yes
- over: 2
- desc: the tallest house of the town, three windows on the south face, a chimney at the west gable. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.

## guild_hall
- index: 288 8x6
- layer: object
- solid: yes
- over: 2
- desc: the long hall of the carriers' guild, stone to the first floor and timber above, a wide double door at the middle of the south face and a bell cote at the east end. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint.
