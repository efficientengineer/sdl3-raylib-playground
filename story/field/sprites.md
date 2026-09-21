# Field sprites — the billboards in the voxel world

The world is voxels; the things that move and the things that read as *drawn* are **billboards**:
flat cut-out sprites standing upright in it, always facing the camera. Ground, walls and houses need
no art at all — they are palette colours and shader detail and rule-built houses — so this file is
the whole of the field's generated art besides the walkers.

`./story_prompt.py sprites <id> [<id>...]` reads this file, lays out a magenta template sheet with
one slot per frame, and writes the ChatGPT package. `ingest` cuts it. Nothing here is style: the
locked blocks in `story/STYLE.md` are inserted by the tool and must never be repeated here.

Format, one `## <id>` per sprite:

- the first plain line is the **description**: one line, what the thing is and what state it is in.
  No style or rendering words, and never ask for writing on anything — image models mangle letters.
- `- footprint: WxH` — the sprite's box in map cells: W cells across, H cells tall, at 64 pixels to
  the cell. Everything on a sheet shares that scale, which is how a boss ends up bigger than a sheep.
- `- frames: N` — optional, 2 or 3. A simple idle or a tell, drawn as N frames left to right. Leave
  it out for a still, which is most of them.
- `- frame2:` / `- frame3:` — what MOVES in that frame, and nothing else. Required whenever
  `frames` is more than one: without it the prompt describes frame two in the same words as frame
  one and ChatGPT draws the same picture twice.
- `- loop: loop | pingpong` — optional, how the frames play. Default `pingpong`.
- `- fps: N` — optional, default 3.

**Output contract** (the engine loads these by file-exists and falls back to a placeholder):
`story/field/sprites/<id>.png`, an indexed PNG on the master palette, index 0 transparent, anchored
**bottom-centre** — the bottom edge of the image is where it meets the ground. A still has no
sidecar. A sprite with frames also writes `story/field/sprites/<id>.json`:
`{ id, frames, frame: [w, h], sheet: [w*frames, h], footprint: [W, H], fps, loop }` — read `frame`
from there, never divide. This is **not** a walker sheet: no direction rows, because a billboard
always faces the camera.

Descriptions come from `story/v3/BESTIARY.md`, which is the design; these are the one-line visual
readings of it. Keep this list short — the test for adding one is *does the demo look wrong without
it?*

## lid
A flat slate-grey disc the width of a dinner plate lying on the ground on dozens of short legs, with a hot pink vent in the middle of its back
- footprint: 1x1
- map: halm
- frames: 2
- loop: pingpong
- frame2: it has stood up on its rim, the disc now edge on and upright, the legs bunched under it and the pink vent turned to face the camera

## burr
A knee-high wide low creature the straw and dull-green colour of dry grass, covered all over in hooked seed-burrs, with a wet dark mouth line running the whole width of its front and no eyes
- footprint: 1x1
- map: hill_path
- frames: 2
- loop: pingpong
- frame2: the mouth line has opened the whole width of the front and the body has sunk an inch lower on its legs

## false_lantern
A soft pale ball of light the size of a held lamp with something dark and thin suspended inside it, drawn hanging in the upper part of the slot with nothing under it
- footprint: 1x1
- map: high_pasture
- frames: 3
- loop: pingpong
- fps: 2
- frame2: the light has dimmed right down, small and dull, and the dark thin shape inside it shows clearly
- frame3: the light has flared out much wider and paler and the shape inside is washed out of sight

## fleece
A sheep seen from the front with no face at all, the wool carried all the way round the head, standing square on four legs
- footprint: 1x1
- map: high_pasture

## klee
A shaggy guard animal bigger than a cart, slate blue and bone white, long and low backed on six legs with a heavy head carried down and a mane of pale standing fibres along its neck and shoulders
- footprint: 3x2
- map: high_pasture
- frames: 2
- loop: pingpong
- fps: 2
- frame2: every fibre of the pale mane has lifted and stood out from the neck and shoulders, and the heavy head has come up level

## sleeping_animal
A sheep lying down asleep in flattened grass with its legs folded under it and its head turned in against its own shoulder
- footprint: 1x1
- map: high_pasture

## sheep
A sheep standing awake on four legs with its head down grazing, heavy in the fleece
- footprint: 1x1
- map: high_pasture

## machine_post
A split wooden post as tall as a man mounted on a heavy iron spring in a timber base, scarred and splintered down one side
- footprint: 1x2
- map: hart_yard

## machine_arm
A waist-high timber frame carrying a sprung iron jaw held open at the height of a wrist, with the spring and its release catch bare on the side
- footprint: 1x2
- map: hart_yard

## machine_swing
A heavy timber frame with a long counterweighted arm on a pivot, an iron block hanging from a rope over a pulley at the top, and a scarred wooden post standing under the arm's reach
- footprint: 2x2
- map: hart_yard
- frames: 3
- loop: loop
- fps: 4
- frame2: the iron counterweight has dropped a hand's width down its rope and the long arm has wound back to one side
- frame3: the long arm has swung right across the frame to the other side and the counterweight is back at the top

## job_board
A weathered free-standing timber board on two legs with a shallow roof over it and a scatter of curling paper sheets pinned to its face
- footprint: 2x2
- map: halm
