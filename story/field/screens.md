# Field screens — one entry per painted map screen

The main path for the walking-around art (owner's direction, 2026-09-18): **ChatGPT paints the whole
map screen from a description, and the game is fitted to the painting.** One painting, two binary
masks, all three generated in the same chat, one message each.

`./story_prompt.py screen <map> <zone>` reads the entry below and writes
`story/packages/chNN/<map>/screens/<zone>/prompt.md` with three prompts in it — the painting, the
walkable mask, the foreground mask. `./story_prompt.py ingest <that folder>` fits all three to one
size, cleans the masks, and derives `story/field/screens/<map>_<zone>.screen`: the nav polygons, the
occluders, the exits and the walker scale, in screen pixels, for the engine to load.

Nothing here is style: the locked blocks in `story/STYLE.md` are inserted by the tool and must never
be repeated in a description. Never ask for writing on anything — image models mangle letters.

Format, one `## <map>.<zone>` per screen:

- the **description**: one paragraph, what the player is looking at, written for an artist. Say what
  the ground is made of, what stands on it, where the streets run and what closes the frame. Name
  tokens are allowed.
- `- exits: left -> west_road, top -> hart_yard` — which frame edge leads to which map. The painter
  is told to run each of them off the edge as a visible path, and `ingest` warns if the walkable mask
  has no walkable ground touching that edge. Edges are `left`, `right`, `top`, `bottom`.
- `- landmark:` — the one thing the player orients by, which has to be in frame (FIELD.md rule 7).
- `- spawn: x y` *(optional)* — where the player stands on arrival, in screen pixels of the painting.
  Without it the tool uses the centroid of the largest walkable polygon.
- `- size: 1536x1024` *(optional)* — the canvas to ask for. Default 1536x1024, landscape.
- `- scale: near far` *(optional)* — the walker's scale at the bottom row of the image and at the top
  walkable row. Default `1.0 0.55`. Nothing in a painting says how deep it is, so this is the one
  number the owner tunes by eye on the phone.

The maps are the seven of chapter one: `halm`, `hart_yard`, `west_road`, `bridge`, `north_grass`,
`ridge_camp`, `stair_shrine`. A screen is one camera's worth of a map, so a big map has several.

## halm.square

The lopsided well square at the middle of {{HOME_TOWN}}, seen from above and to one side, late
afternoon, the valley town half emptied. The square is not square: an irregular open space of worn
grey paving that spreads wider at the near end and pinches to a lane at the far one, with beaten dirt
showing through where feet have crossed it and grass creeping in at the edges. The round stone well
stands a little off centre with its plank roof on two posts and its bucket hooked at the rim, and the
paving fans out from it in rings that the town grew around. The long low guild hall faces the square
from the right, its deep porch on squat timber posts and its double doors standing open, a bare plank
board fixed to the wall beside them. Plastered houses under low red pantile roofs crowd the left and
the far side, no two of them set at the same angle, their doors open and a shutter or two gone from
the hinges; a two-wheeled handcart stands loaded above the sides with a rolled mattress and tied
bundles where the lane leaves the far corner. A dry-laid stone wall with a flat coping closes the
grain yard at the near right, one stretch of it bellied out and patched with newer stone, with the
dark open front of the grain shed behind it. A broad street leaves the left edge of the frame and
bends out of sight between the houses; a narrower lane climbs the far edge toward the shoulder of the
hill, past a broad valley tree with a heavy rounded crown. Nothing is straight for longer than two
houses, the gaps between the buildings are all different, and the town dissolves at the edges of the
frame into orchard, hedge and shed rather than stopping at a line.

- exits: left -> west_road, top -> hart_yard
- landmark: the lopsided well square, and the road that climbs round the shoulder of the hill
- spawn: 760 880
- size: 1536x1024

## hart_yard.yard

{{MENTOR}}'s yard on the shoulder of the hill above {{HOME_TOWN}}, seen from above and to one side,
the same late afternoon. Rough grass worn to bare earth in a crooked arc where feet have turned, with
six head-high practice posts of scarred timber set round it at uneven spacings, none of them plumb
and the wood chewed pale where it has been hit. The squat hunter's house closes the far side of the
yard at an angle, half its roof tiles laid and the rest stacked in short piles on the battens, a low
plank door set off-centre and one small square window beside it, a ladder standing against the eaves.
A squat water barrel of dark staves brim full stands by the door with a tin cup hooked on the rim. A
fence of three rails pegged between split posts runs down the near side, one rail newer than the
others and set slightly proud, with the ground falling away behind it into the tops of the town's
roofs far below and the valley beyond going blue with distance. A dirt path enters at the bottom of
the frame from that fall and climbs into the yard. The near ground is high and the far ground is low,
so the yard reads as a shelf cut into the hill, and the edges dissolve into rough hedge, a stacked
woodpile and a thin young tree staked at the base.

- exits: bottom -> halm
- landmark: the six posts set in a crooked arc round the ladder house
- size: 1536x1024

## west_road.road

The road west out of {{HOME_TOWN}}, seen from above and to one side, the light going long. A wide
dirt road of two wheel ruts with a grass crown between them comes in from the bottom right of the
frame, bends round a rock too big to move, and runs away to the left edge between low hedges and
open valley grass. Nothing about it is straight: it swings out for the rock, wanders where the ground
is wet, and forks once to a beaten side path that goes off up the slope to nothing in particular. A
stone culvert carries a ditch under the road at the near bend, its arch half fallen and the roadway
above it dropped into the hole, loose blocks lying in the ditch below, so the road narrows to one
side of it. A two-wheeled handcart stands abandoned on the verge with its shafts down, loaded above
the sides. A knee-high weathered milestone stands where the side path leaves, one flat face scrubbed
bare. Broad valley trees with heavy rounded crowns stand in ones and twos along the hedge line, with
a thin young staked tree near the near verge, and a blank plank board nailed across a post leans at
the fork. Beyond the hedges the valley opens out in fields and hedge lines going hazy toward the
hills, and the road's far end is hidden by a bend and a stand of trees rather than by the frame edge.

- exits: right -> halm, left -> bridge
- landmark: the broken culvert at the bend, and the rock the road swings round
- size: 1536x1024
