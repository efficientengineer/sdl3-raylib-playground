# Field tiles — one entry per tile id

The tile half of the art contract in `FIELD.md`. `./story_prompt.py tiles <id> [<id>...]` reads this
file, lays out a template sheet of square slots, and writes the ChatGPT package. Style is never
written here: the locked blocks in `story/STYLE.md` are inserted by the tool.

Format, one `## <id>` per tile:

- the first plain line is the **description**: one line, what the surface is. No style or rendering
  words, and nothing written on it.
- `- kind: ground` — seen straight down from above; the floor of a cell.
- `- kind: wall` — seen level from the front; the face of a height step. One 64x64 wall tile covers
  one full step (two half-steps) and repeats upward.
- `- map: <id>[, <id>...]` — the chapter-one maps that use this tile. The seven are `halm`,
  `hart_yard`, `west_road`, `bridge`, `north_grass`, `ridge_camp`, `stair_shrine`. Generating a map's
  tiles and props together is how a map gets dressed in one pass.

Every tile is 64x64 and seamless on all four edges: the right edge continues into the left, the
bottom into the top, so a field of them shows no seam and no repeated landmark.

Per map, the set is: **halm** grass, dirt, stone, plank, cliff, wall_plaster, wall_timber,
wall_stone, wall_stone_top, fence_wood, fence_top, hedge, hedge_top · **hart_yard** grass, dirt,
plank, cliff, wall_plaster, wall_timber, wall_stone, wall_stone_top, fence_wood, fence_top, hedge,
hedge_top · **west_road** road, grass, old_road, river_bank, water · **bridge** stone, road,
river_bank, water, grass · **north_grass** grass_tall, grass, old_road, scree, cliff ·
**ridge_camp** scree, grass, cliff · **stair_shrine** grass, dirt, stone, wall_plaster, stair_stone.

**Walls, fences and hedges are extruded boxes**, not sprites: the engine raises a box along the line
and textures its face with the `- kind: wall` tile and its top with the matching `- kind: ground`
one, so those two have to look like the same object seen from the front and from above. A tile is
opaque, so a gap a person could see through — between two fence rails — is painted as one flat dark
colour rather than left clear.

## grass
Short valley grass in uneven clumps with bare earth showing through and a few small stones.
- kind: ground
- map: halm, hart_yard, west_road, bridge, north_grass, ridge_camp, stair_shrine

## dirt
Packed pale earth of a walked lane, dry, with shallow wheel ruts and loose grit.
- kind: ground
- map: halm, hart_yard, stair_shrine

## stone
Flat grey paving slabs of uneven size laid close, with grass in the joints.
- kind: ground
- map: halm, bridge, stair_shrine

## plank
Weathered timber decking boards laid one way, gapped, with nail heads at the joins.
- kind: ground
- map: halm, hart_yard

## water
Still dark water with slow ripple lines and a few pale highlights.
- kind: ground
- map: west_road, bridge

## road
The wide packed surface of a carting road, pale grit rolled hard, with deep paired wheel ruts and a scatter of dropped straw.
- kind: ground
- map: west_road, bridge

## grass_tall
Shoulder-high grass seen straight down onto the heads, dense and all leaning one way, with narrow dark gaps between the clumps.
- kind: ground
- map: north_grass

## river_bank
Wet grey shingle and river mud at a water's edge, rounded stones half sunk, dried weed caught in the hollows.
- kind: ground
- map: west_road, bridge

## scree
Loose broken hill stone in flat shards over dry earth, tipped at every angle, nothing growing in it.
- kind: ground
- map: north_grass, ridge_camp

## old_road
Pale cut paving slabs of one size showing through thin grass, joints tight and edges still square, sunk a little below the turf.
- kind: ground
- map: west_road, north_grass

## cliff
A face of broken valley rock in rough horizontal bands, cracked, with loose scree caught on the ledges.
- kind: wall
- map: halm, hart_yard, north_grass, ridge_camp

## wall_plaster
Cream plaster over rubble stone, cracked and patched in places, with a low band of bare stone at the bottom.
- kind: wall
- map: halm, hart_yard, stair_shrine

## wall_timber
Dark timber framing over pale plaster panels, the beams pegged at the joins.
- kind: wall
- map: halm, hart_yard

## stair_stone
Enormous pale grey cut blocks in even courses with hairline joints, unweathered, no moss and no crack anywhere in them.
- kind: wall
- map: stair_shrine

## wall_stone
A dry stone wall face of stacked grey field stones in uneven courses, no mortar, small chips packed into the gaps and lichen on the weathered faces.
- kind: wall
- map: halm, hart_yard

## wall_stone_top
The top of a dry stone wall seen straight down from above: a run of flat grey capstones laid across the width, chipped at the edges, moss in the joints between them.
- kind: ground
- map: halm, hart_yard

## fence_wood
A post-and-rail timber fence seen level from the front, two split rails pegged between square posts, the wood grey and shaken; the gaps between the rails are one flat dark colour, the same on every tile.
- kind: wall
- map: halm, hart_yard

## fence_top
The top of a timber fence seen straight down from above: the upper rail running the length of the tile with its grain along it, a square post head cut level at intervals, the strip either side of the rail the same flat dark colour as the fence face.
- kind: ground
- map: halm, hart_yard

## hedge
A clipped hedge face of dense small leaves cut level, dark green with olive lights, a few bare twigs showing through near the bottom.
- kind: wall
- map: halm, hart_yard

## hedge_top
The top of a clipped hedge seen straight down from above, dense leaf cover cut level with a slightly uneven ridge along the middle and shadow between the clumps.
- kind: ground
- map: halm, hart_yard
