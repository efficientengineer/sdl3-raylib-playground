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

Chapter one is the valley town of Halm emptying out: doors open, shutters off, carts loaded with
beds, the grain yard still working, and Hart's yard up the hill.

## house_a
A small valley house of plastered stone under a low timber gable, one shutter open and one gone, the plank door standing ajar on an empty room.
- footprint: 3x3

## house_b
A narrow two-storey house with a steep tiled roof and an outside stair to the upper door, its lower shutters closed and barred.
- footprint: 2x3

## guild_hall
A long low stone hall with a deep porch on squat timber posts, wide double doors standing open, and a bare board on the wall beside them.
- footprint: 4x3

## grain_shed
An open-fronted grain shed of dark boards on a stone base, a sagging roof, sacks stacked inside and a scale bench by the door.
- footprint: 3x3

## ladder_house
A squat hunter's house with half its roof tiles laid and the rest stacked, a long ladder leaning against the eaves, a bad chimney.
- footprint: 3x3

## well
A round stone well with a low wall, a plank roof on two posts, a rope on a winding drum and a wooden bucket hooked at the rim.
- footprint: 1x2

## cart
A two-wheeled handcart with its shafts down, loaded above the sides with a rolled mattress and tied bundles.
- footprint: 2x2

## barrel
A squat water barrel of dark staves with two iron hoops, brim full, a tin cup hooked on the rim.
- footprint: 1x1

## practice_post
A head-high post of scarred timber set in the ground, a cross-piece near the top, the wood chewed pale where it has been hit.
- footprint: 1x2

## fence
Three rails pegged between two split posts, one rail newer than the others and set slightly proud.
- footprint: 2x1

## tree_a
A broad valley tree with a thick low trunk and a heavy rounded crown, a few dead branches on one side.
- footprint: 2x3

## tree_b
A thin young tree with a straight pale trunk and a sparse upright crown, staked at the base.
- footprint: 1x2

## sign
A blank plank board nailed across a single post at head height, the wood split at one corner.
- footprint: 1x2

## milestone
A knee-high roadside marker stone, weathered round at the top, one flat face scrubbed bare.
- footprint: 1x1
