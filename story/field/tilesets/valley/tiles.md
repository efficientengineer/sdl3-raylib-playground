# valley — the tileset of Halm and the hill above it

The engine reads this file (src/tilefield.cpp); the art pipeline generates `atlas.png` from it. One
`## <name>` per tile or stamp. `- index: n` is the atlas cell of its top-left tile, 16 columns to the
row, 32 px to the tile; `n WxH` makes it a **stamp** laid out row-major from that cell. `- layer:` is
`ground`, `object` or `over`. `- solid:` is `yes`, `no`, or one row of `#`/`.` per tile row, slashes
between the rows. `- over: n` is how many of a stamp's TOP rows draw above the walkers, so a roof
ridge or a tree crown hides whoever passes behind it. `- fringe: n` says this terrain overlays every
lower-numbered terrain it touches, using its own `_edge`, `_corner_out` and `_corner_in` tiles. Their
**shape** is fixed by the fringe convention in `story/field/tilesets/README.md` and the engine only
rotates them, so the `desc:` on a fringe entry names the material and nothing about the shape.
`- frames: n` animates over that many cells to the right.

**Every tile here has a procedural placeholder in the engine, keyed on its name**, so the maps are
walkable and readable with no `atlas.png` at all. The names below are therefore the art contract: a
sheet that fills in cell 128 fills in `house_a`, and nothing else has to change.

The whole set is one 16-column atlas, 24 rows: 512 x 768.

## grass
- index: 1
- layer: ground
- solid: no
- desc: short river-valley turf seen from straight above, worn to the soil in places, no single blade readable

## grass_tuft
- index: 2
- layer: ground
- solid: no
- desc: the same turf with a few standing tufts of longer grass, scattered off any grid

## grass_flower
- index: 3
- layer: ground
- solid: no
- desc: the same turf with a handful of small pale field flowers

## dirt
- index: 4
- layer: ground
- solid: no
- fringe: 1
- desc: packed earth of a village lane, dry, a little gravel worked into it

## dirt_edge
- index: 16
- layer: ground
- solid: no
- desc: the fringe of a dirt lane: packed earth, dry and a little gravelly, over nothing else

## dirt_corner_out
- index: 17
- layer: ground
- solid: no
- desc: the same packed earth of a dirt lane, nothing else in the tile

## dirt_corner_in
- index: 18
- layer: ground
- solid: no
- desc: the same packed earth of a dirt lane, nothing else in the tile

## dirt_rut
- index: 5
- layer: ground
- solid: no
- fringe: 1
- desc: packed earth with two cart ruts worn down the length of it

## paving
- index: 6
- layer: ground
- solid: no
- fringe: 3
- desc: laid stone of the village square, blocks of uneven size, mortar dark between them

## paving_edge
- index: 19
- layer: ground
- solid: no
- desc: the fringe of laid stone: outer blocks half sunk and broken, over nothing else

## paving_corner_out
- index: 20
- layer: ground
- solid: no
- desc: the same laid stone, its outer blocks broken, nothing else in the tile

## paving_corner_in
- index: 21
- layer: ground
- solid: no
- desc: the same laid stone, its outer blocks broken, nothing else in the tile

## paving_worn
- index: 7
- layer: ground
- solid: no
- fringe: 3
- desc: the same laid stone, hollowed and polished where the town walks over it

## water
- index: 8
- layer: ground
- solid: yes
- frames: 2
- fringe: 4
- desc: shallow running stream water, two frames, the light on it moved between them

## water_edge
- index: 22
- layer: ground
- solid: no
- desc: the fringe of a stream bank: wet stones and mud where the water ends, over nothing else

## water_corner_out
- index: 23
- layer: ground
- solid: no
- desc: the same wet stones and mud of a stream bank, nothing else in the tile

## water_corner_in
- index: 24
- layer: ground
- solid: no
- desc: the same wet stones and mud of a stream bank, nothing else in the tile

## gravel
- index: 10
- layer: ground
- solid: no
- desc: the grey grit of a working yard, raked flat and stained

## crop
- index: 11
- layer: ground
- solid: no
- fringe: 2
- desc: standing barley seen from above, the heads in rough drills

## crop_edge
- index: 25
- layer: ground
- solid: no
- desc: the fringe of a barley plot: the last heads where the drills stop, over nothing else

## crop_corner_out
- index: 26
- layer: ground
- solid: no
- desc: the same standing barley of a worked plot, nothing else in the tile

## crop_corner_in
- index: 27
- layer: ground
- solid: no
- desc: the same standing barley of a worked plot, nothing else in the tile

## mud
- index: 12
- layer: ground
- solid: no
- desc: churned wet ground of a cart yard, hoof and wheel marks standing in it

## bridge_deck
- index: 13
- layer: ground
- solid: no
- fringe: 5
- desc: planks of a footbridge laid across the run of the stream, a rail board at each side

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
- desc: the capped village well, stone ring, two posts and a small roof, a new rope on the windlass

## handcart
- index: 50 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: a two-wheeled handcart standing empty with its shafts down

## woodpile
- index: 52 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: split logs stacked end-on against a yard wall

## haystack
- index: 54 2x2
- layer: object
- solid: ##/##
- over: 0
- desc: a small hayrick, roped over the top

## market_stall
- index: 56 2x2
- layer: object
- solid: ../##
- over: 0
- desc: a trestle under a striped awning, nothing laid out on it

## tree
- index: 80 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a broad valley tree, dense crown seen from above and slightly in front, one trunk at the foot

## tree_2
- index: 82 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a second tree, narrower and darker than the first, so two together do not read as a pair

## tree_bare
- index: 84 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a dead tree, bare limbs, the bark off one side

## orchard_tree
- index: 86 2x3
- layer: object
- solid: ../../##
- over: 2
- desc: a low pruned fruit tree on a short trunk, the crown kept round

## house_a
- index: 128 3x3
- layer: object
- solid: yes
- over: 1
- desc: a small valley house, plastered walls on a stone footing, red pantile roof, one door and one shuttered window on the south face

## house_b
- index: 131 3x3
- layer: object
- solid: yes
- over: 1
- desc: a second small house, timbers showing in the plaster, the roof a shade darker

## shed
- index: 134 3x3
- layer: object
- solid: yes
- over: 1
- desc: a plank outbuilding with a shallow board roof and a wide doorway

## stable
- index: 137 3x3
- layer: object
- solid: yes
- over: 1
- desc: a stable with a split door standing open and straw trodden at the threshold

## hut
- index: 140 3x3
- layer: object
- solid: yes
- over: 1
- desc: the smallest house in the town, one room, turf growing on the roof edge

## house_c
- index: 176 4x3
- layer: object
- solid: yes
- over: 1
- desc: a wider house with two windows and a bench beside the door

## barn
- index: 180 4x3
- layer: object
- solid: yes
- over: 1
- desc: a barn with tall boarded doors and a hoist beam under the gable

## grain_shed
- index: 184 4x3
- layer: object
- solid: yes
- over: 1
- desc: a grain store on stone staddles, the boards tight, one small hatch high in the wall

## house_d
- index: 188 4x3
- layer: object
- solid: yes
- over: 1
- desc: a house with an outside stair to an upper door and washing lines at the side

## ladder_house
- index: 224 4x4
- layer: object
- solid: yes
- over: 2
- desc: a two-storey house with a ladder left leaning against the eaves and slates missing off the ridge

## house_e
- index: 228 4x4
- layer: object
- solid: yes
- over: 2
- desc: the tallest house of the town, three windows on the south face, a chimney at the west gable

## guild_hall
- index: 288 8x6
- layer: object
- solid: yes
- over: 2
- desc: the long hall of the carriers' guild, stone to the first floor and timber above, a wide double door at the middle of the south face and a bell cote at the east end
