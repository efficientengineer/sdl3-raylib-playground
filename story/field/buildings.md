# Field buildings — one entry per building id

A building is **not a sprite**. The engine builds it out of map geometry (`## buildings` in a
`.map` file gives its footprint, height and rotation) and dresses that geometry with three textures.
This file says what each building is made of; `./story_prompt.py building <id> [<id>...]` turns it
into one ChatGPT template with three slots per building. Nothing here is style: the locked blocks in
`story/STYLE.md` are inserted by the tool and must never be repeated in a description.

The face-texture contract is owned by the engine (`src/FIELD_NOTES.md`, "Face-texture contract"):

| file | size | how it is mapped |
|---|---|---|
| `story/field/buildings/<id>_front.png` | 128x128, opaque | stretched **once** across the whole front wall, so a painted door and windows land exactly where they were drawn whatever the building's width |
| `story/field/buildings/<id>_side.png` | 128x128, opaque, seamless both ways | tiles once per cell across and once per world unit up, on the back, both ends and the gables |
| `story/field/buildings/<id>_roof.png` | 64x64, opaque, seamless | tiles once per cell in both directions on each roof slope |

So the **front** slot is a whole wall — one door, its windows, its beams, drawn where they belong —
and the **side** and **roof** slots are plain material samples with no openings and no landmark: a
patch of wall and a patch of roof that repeat forever without a visible seam.

Format, one `## <id>` per building:

- the first plain line is the **description**: one line, what the building is and what state it is
  in. No style or rendering words, and never ask for writing on anything — image models mangle
  letters, and a painted sign on a wall would be stretched across the whole front.
- `- map: <id>[, <id>...]` — the chapter-one maps that place this building. An id placed on more
  than one map is drawn once, with the first.
- `- wall:` *(optional)* — the material for the side slot, when the description leaves it open.
- `- roof:` *(optional)* — the material for the roof slot, same reason.

Ids come from the `## buildings` section of `story/field/maps/*.map`. Everything else in the world,
including the grain-yard wall, is a prop sprite and belongs in `props.md`.

## house_a
A small valley house of plastered stone under a low timber gable, a plank door standing ajar in the middle of the front wall, one shutter open beside it and one gone from its hinges.
- map: halm
- wall: cream plaster over rubble stone, cracked and patched, a low band of bare stone along the bottom
- roof: dark red clay pantiles in overlapping rows, a few chipped and a few slipped out of line

## house_b
A narrow two-storey house with a steep roof, a barred door at ground level with closed shutters either side, and a small upper window under the gable reached by an outside stair.
- map: halm
- wall: pale plaster panels between dark pegged timber framing
- roof: dark red clay pantiles in overlapping rows, tight and well kept

## guild_hall
A long low stone hall with a deep porch on squat timber posts, wide double doors standing open in the centre of the front wall, a bare plank board fixed to the wall beside them and a small high window at each end.
- map: halm
- wall: coursed grey valley stone with wide pale mortar joints, weathered darker at the base
- roof: heavy grey stone slates in overlapping courses, largest at the eaves

## grain_shed
An open-fronted grain shed of dark boards on a stone base, the whole front wall a wide timber opening under a sagging lintel, with a plank shutter hinged above it and sacking hung in one corner.
- map: halm
- wall: wide dark weathered boards laid upright, gapped, on a low course of rough stone
- roof: bundled straw thatch laid in courses, sagging in the middle, combed flat

## ladder_house
A squat hunter's house with half its roof tiles laid and the rest stacked, a low plank door set off-centre in the front wall, one small square window beside it and a bad chimney at the end.
- map: halm, hart_yard
- wall: cream plaster over rubble stone, patched unevenly, bare stone showing through at one corner
- roof: dark red clay pantiles on the lower half and bare timber battens above, tiles stacked in short piles
