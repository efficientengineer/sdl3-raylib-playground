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

Every tile is 64x64 and seamless on all four edges: the right edge continues into the left, the
bottom into the top, so a field of them shows no seam and no repeated landmark.

## grass
Short valley grass in uneven clumps with bare earth showing through and a few small stones.
- kind: ground

## dirt
Packed pale earth of a walked lane, dry, with shallow wheel ruts and loose grit.
- kind: ground

## stone
Flat grey paving slabs of uneven size laid close, with grass in the joints.
- kind: ground

## plank
Weathered timber decking boards laid one way, gapped, with nail heads at the joins.
- kind: ground

## water
Still dark water with slow ripple lines and a few pale highlights.
- kind: ground

## cliff
A face of broken valley rock in rough horizontal bands, cracked, with loose scree caught on the ledges.
- kind: wall

## wall_plaster
Cream plaster over rubble stone, cracked and patched in places, with a low band of bare stone at the bottom.
- kind: wall

## wall_timber
Dark timber framing over pale plaster panels, the beams pegged at the joins.
- kind: wall
