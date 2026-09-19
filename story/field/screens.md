# Field screens — one entry per painted map

The main path for the walking-around art. Owner's direction, 2026-09-19: **the field is Phantasy
Star IV.** A screen is a **top-down 16-bit JRPG map**, painted whole by ChatGPT from the entry below,
and the game is fitted to the painting rather than the other way round. The angled three-quarter
experiment is dropped: with an angled camera everything hides ground behind it, and in top-down
nothing does.

One chat, one message per image:

1. the map → `returned.png`
2. the walkable mask, green on black, traced over it → `returned_walk.png`
3. *(only when the entry says `- overhead: yes`)* the overhead mask, white where the player walks
   **underneath** something → `returned_over.png`

`./story_prompt.py screen <map> <zone>` writes the package;
`./story_prompt.py ingest <that folder>` fits everything to one size, cleans the mask, and derives
`story/field/screens/<map>_<zone>.screen` — the size, the nav polygons, the doorways, the exits, the
spawn and the walker height, all in screen pixels, which is what the engine loads.

Nothing here is style: the locked blocks in `story/STYLE.md` are inserted by the tool and must never
be repeated in a description. Never ask for writing on anything — image models mangle letters.

Format, one `## <map>.<zone>` per screen:

- the **description**: one paragraph, the place as it is seen from straight above, written for an
  artist. Say what the ground is made of, what stands on it, where the lanes run, where the map
  thins out at its edges. Name tokens are allowed.
- `- exits: left -> west_road, top -> hart_yard` — which frame edge leads to which map. The painter
  is told to run each of them off that edge as a road, and `ingest` warns if no walkable ground
  reaches it. Edges are `left`, `right`, `top`, `bottom`.
- `- landmark:` — the one thing the player orients by, which has to be on the map (FIELD.md rule 7).
- `- spawn: x y` *(optional)* — where the player stands on arrival, in painting pixels. Without it
  the tool uses the middle of the largest walkable polygon; a spawn that is not on walkable ground
  is reported and ignored.
- `- walker: 64` *(optional)* — how tall a person is drawn, in painting pixels. This is the scale of
  the whole map: a door comes out about 74 px, and every path is at least three people wide. Default
  64 at 1536x1024.
- `- size: 1536x1024` *(optional)* — the canvas to ask for. Default 1536x1024, landscape.
- `- overhead: yes` *(optional)* — this map has something the player walks **under** (an archway top,
  a bridge deck, an overhanging balcony), so the package asks for the third mask and the tool writes
  `<map>_<zone>_over.png` for the engine to draw after the walker.

The maps are the seven of chapter one: `halm`, `hart_yard`, `west_road`, `bridge`, `north_grass`,
`ridge_camp`, `stair_shrine`. A screen is one map's worth of world, whole, in one picture.

## halm.town

The whole valley town of {{HOME_TOWN}} on one map, seen from straight above, late afternoon, half
emptied. It grew round its well, so the middle of the map is a lopsided open square of worn grey
paving — wider at its south end, pinching to a lane at its north — with the round stone well a little
off centre, its plank roof on two posts and its bucket at the rim, and the paving laid in rings that
spread out from it and give way to beaten dirt and creeping grass. The long low guild hall stands on
the east side of the square with its deep porch on squat timber posts, its double doors open and a
bare plank board fixed to the wall beside them; it is the biggest roof on the map. Plastered houses
under red pantile roofs crowd the square and the lanes off it, staggered along the lanes at all
different spacings and in clearly different sizes, some set forward to the path and some back behind
a yard wall, none of them in a row. Their doors stand open and a shutter or two is off its hinges.
Nine lanes leave the square and none of them is straight: they bend, fork, pinch to a gap and widen
again, and two of them end in a yard. The grain yard fills the south-east corner behind a dry-laid
stone wall with a flat coping, one stretch of it bellied out and patched with newer stone, with the
dark open front of the grain shed inside it and a plank bench of grain scales by the door. North-west
of the square the carters' yard stands empty: bare rutted earth, one two-wheeled handcart left with
its shafts down and loaded above the sides, and nothing else. A stream comes in at the north-east,
runs a crooked line south-west across the top of the town under two plank footbridges, and leaves at
the west edge. A wide dirt road leaves the west edge of the map toward the valley; a narrower path
climbs the north edge toward the shoulder of the hill. Broad valley trees with heavy rounded crowns
stand in ones and twos through the town, and the edges of the map dissolve into orchard rows, hedges,
sheds, garden walls and open field rather than stopping at a line.

- exits: left -> west_road, top -> hart_yard
- landmark: the lopsided well square, with the guild hall along one side of it
- walker: 40
- spawn: 750 570
- size: 1536x1024

## hart_yard.yard

{{MENTOR}}'s yard on the shoulder of the hill above {{HOME_TOWN}}, seen from straight above, the same
late afternoon. Rough grass worn through to bare earth in a crooked arc across the middle, with six
head-high practice posts of scarred timber set round it at uneven spacings, none of them in line, the
wood chewed pale where it has been hit. The squat hunter's house sits at the north of the yard with
half its roof tiles laid and the rest stacked in short piles on the bare battens, a low plank door
set off-centre in its front wall, one small square window beside it and a ladder leaning against the
eaves. A squat water barrel of dark staves stands by the door with a tin cup on the rim. A fence of
three rails pegged between split posts runs along the south side of the yard, one rail newer than the
others, with a gap where the path comes through. A woodpile, a chopping block and a thin young tree
staked at its base stand about the yard at odd spacings, and a stony dirt path enters at the bottom
edge of the map and bends up through the fence gap into the yard. The edges go to rough hedge, heather
and outcrops of pale hill stone, and at the south-west corner the ground drops away in a bank of rock
and gorse.

- exits: bottom -> halm
- landmark: the six posts set in a crooked arc round the ladder house
- walker: 64
- size: 1536x1024

## west_road.road

The road west out of {{HOME_TOWN}}, seen from straight above, the light going long. A wide dirt road
of two wheel ruts with a grass crown between them comes in at the right edge of the map, swings out
round a rock too big to move, wanders where the ground is wet, and leaves at the left edge between
low hedges and open valley grass. It forks once to a beaten side path that climbs north to a stand of
trees and stops there. A stone culvert carries a ditch under the road at the near bend, its arch half
fallen and the roadway above it dropped into the hole with loose blocks in the ditch below, so the
road narrows to one side of it and a person has to go round. A two-wheeled handcart stands abandoned
on the verge with its shafts down. A knee-high weathered milestone stands where the side path leaves,
and a blank plank board nailed across a post leans beside it. Broad valley trees with heavy rounded
crowns stand in ones and twos along the hedge lines, with a thin young staked tree near the verge,
and the fields either side of the road are irregular: different sizes, hedged at odd angles, one of
them ploughed in curving ridges, one gone to weeds.

- exits: right -> halm, left -> bridge
- landmark: the broken culvert at the bend, and the rock the road swings round
- walker: 64
- size: 1536x1024
