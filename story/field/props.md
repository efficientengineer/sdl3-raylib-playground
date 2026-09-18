# Field props — one entry per sprite id

The prop half of the art contract in `FIELD.md`. `./story_prompt.py props <id> [<id>...]` reads this
file, lays out a template sheet with one slot per prop, and writes the ChatGPT package. Nothing here
is style: the locked blocks in `story/STYLE.md` are inserted by the tool and must never be repeated
in a description.

Format, one `## <id>` per prop:

- the first plain line is the **description**: one line, what the object is and what state it is in.
  No style or rendering words, and never ask for writing on anything — image models mangle letters.
- `- footprint: WxH` — the sprite's box in map cells: W cells across the ground, H cells tall,
  at 64 pixels to the cell. A 3x3 house is three cells wide and three cells tall, and every prop on
  a sheet shares that scale, which is how a well ends up smaller than a hall.
- `- map: <id>[, <id>...]` — the chapter-one maps that place this prop. The seven are `halm`,
  `hart_yard`, `west_road`, `bridge`, `north_grass`, `ridge_camp`, `stair_shrine`. This is what lets
  the art run be ordered by map instead of by sprite: generate a map's props and tiles together and
  that map can be dressed in one pass.

Chapter one is the valley town of {{HOME_TOWN}} emptying out (doors open, shutters off, carts loaded
with beds, the grain yard still working, {{MENTOR}}'s yard up the hill), then two days west: the
crowded road and its broken culvert, the river bridge, the empty grass north of it, a camp on the
ridge, and the shrine on the grass under {{STAIR}}.

**Buildings are not props.** A house, the guild hall, the grain shed and the ladder house are map
geometry with three face textures on them, not sprites: they live in `buildings.md` and come off a
`./story_prompt.py building` sheet. What stays here is everything a person could walk around — the
well, the carts, the fences, the trees, the grain-yard wall.

**{{STAIR}} itself is not a prop.** The bottom step is a staircase wider than a town ending thirty
feet above an empty field: at 64 pixels to the cell it is hundreds of cells across and taller than
the camera. It has to be built as map geometry — a run of the `stair_stone` wall tile for the face of
the step, with the mass above it carried by the map's backdrop — and the only sprites under it are
the ones listed here (`stone_shelf`, `water_jar`, `step_rope`, `sitter`). Do not put it on a prop
sheet; a 64-pixel staircase is the one thing that will make the scale of the field read wrong.

## well
A round stone well with a low wall, a plank roof on two posts, a rope on a winding drum and a wooden bucket hooked at the rim.
- footprint: 1x2
- map: halm, stair_shrine

## cart
A two-wheeled handcart with its shafts down, loaded above the sides with a rolled mattress and tied bundles.
- footprint: 2x2
- map: halm, west_road, bridge

## barrel
A squat water barrel of dark staves with two iron hoops, brim full, a tin cup hooked on the rim.
- footprint: 1x1
- map: halm, hart_yard

## practice_post
A head-high post of scarred timber set in the ground, a cross-piece near the top, the wood chewed pale where it has been hit.
- footprint: 1x2
- map: hart_yard

## fence
Three rails pegged between two split posts, one rail newer than the others and set slightly proud.
- footprint: 2x1
- map: halm, hart_yard

## tree_a
A broad valley tree with a thick low trunk and a heavy rounded crown, a few dead branches on one side.
- footprint: 2x3
- map: halm, hart_yard, west_road

## tree_b
A thin young tree with a straight pale trunk and a sparse upright crown, staked at the base.
- footprint: 1x2
- map: halm, west_road, north_grass

## sign
A blank plank board nailed across a single post at head height, the wood split at one corner.
- footprint: 1x2
- map: halm, west_road

## milestone
A knee-high roadside marker stone, weathered round at the top, one flat face scrubbed bare.
- footprint: 1x1
- map: west_road

## scale_bench
A low plank bench with a pair of iron grain scales bolted to one end, a stack of small lead weights beside the pan and one weight lying loose.
- footprint: 2x1
- map: halm

## yard_wall
A shoulder-high grain-yard wall of dry-laid stone with a flat coping course, one stretch bellied out and patched with newer stone.
- footprint: 3x2
- map: halm

## culvert
A stone culvert mouth under a road, the arch half fallen and the roadway above it collapsed into the hole, loose blocks in the ditch below.
- footprint: 3x2
- map: west_road

## bridge_rail
A run of low bridge parapet in cut stone with a flat coping, one section rebuilt in newer blocks and a rubbed hollow where hands go.
- footprint: 2x1
- map: bridge

## cave_mouth
A low dark opening in broken hillside rock, the lip shored with one fallen slab, a rope tied off round a boulder and hanging into it.
- footprint: 3x2
- map: north_grass

## slide_rubble
A fan of fresh hillside rubble spilled across a path, raw earth at its top edge, boulders and snapped roots piled at the bottom.
- footprint: 3x2
- map: north_grass

## campfire
A small fire of stacked sticks in a ring of hand-sized stones, embers under it, a blackened pot pushed to one side.
- footprint: 1x1
- map: ridge_camp, stair_shrine

## bedroll
A rolled-out blanket bed on flat ground with a folded pack at the head end and a pair of boots set beside it.
- footprint: 2x1
- map: ridge_camp, stair_shrine

## ridge_rock
A wind-scoured outcrop of pale hill stone standing clear of the grass, flat on top, undercut on the windward side.
- footprint: 3x2
- map: ridge_camp, north_grass

## shrine_house
A low single-storey house of pale stone with a flat weighted roof, a cloth hung in the doorway, and a plank door pinned with small papers inside.
- footprint: 3x2
- map: stair_shrine

## stone_shelf
A long shelf of pale cut stone standing clear of the ground on two squat legs, the top worn hollow in the middle and swept clean.
- footprint: 4x1
- map: stair_shrine

## water_jar
A sealed round water jar the size of a bucket in dull red clay, a cloth cover tied down over the neck with cord.
- footprint: 1x1
- map: stair_shrine

## step_rope
A thick rope hanging straight down out of the dark, knotted every arm's length, the bottom end swinging clear of the ground.
- footprint: 1x3
- map: stair_shrine

## sitter
A knee-high pale grey figure folded down on itself with four short arms tucked in, facing outward, one thin green line running down its spine.
- footprint: 1x1
- map: stair_shrine

## rubbing_stall
A plank trestle stall under a stretched cloth awning, flat sheets of paper weighted with pebbles, a jar of charcoal stubs at one end.
- footprint: 2x2
- map: stair_shrine
