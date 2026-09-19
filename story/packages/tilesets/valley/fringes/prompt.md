# ChatGPT package: valley — fringes tile sheet

## What exists already

- `story/field/tilesets/valley/atlas.png` — not started yet
- `story/field/tilesets/valley/cut/fringes.png` — not cut yet

This sheet carries 12 of the tileset's entries: `dirt_edge`, `dirt_corner_out`, `dirt_corner_in`, `paving_edge`, `paving_corner_out`, `paving_corner_in`, `water_edge`, `water_corner_out`, `water_corner_in`, `crop_edge`, `crop_corner_out`, `crop_corner_in`

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/fringes/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 12 numbered slots filled in. Canvas: landscape, 1536x1024, exactly the same size and proportions as the template.

16-bit Sega Genesis era pixel art, early 1990s JRPG field map, Phantasy Star IV look.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS SHEET IS: the 'fringes' tile sheet for the 'valley' tileset of a top-down 16-bit JRPG field map. Every slot is one tile or one multi-tile object, and in game each tile is exactly 32x32 pixels.

PIXEL GRID — the most important rule on this sheet. Everything is drawn at 4x. Every art pixel is a 4x4 block of identical colour, aligned to the slot: the blocks start at the slot's top-left inside corner and run in an even grid across and down it, so a 128-pixel slot is a 32x32 pixel-art tile blown up 4 times. No block is half a colour, no edge falls between blocks, no detail is finer than one block, and no two objects on the sheet use different block sizes.

PROJECTION — top-down oblique, the Phantasy Star IV field view, the same for every slot:

- The ground is seen from straight above: a patch of grass, dirt, paving or water is a flat texture with no thickness and no side visible.
- An object standing on the ground is seen from slightly in front of straight above: you see its top or roof AND its south-facing front, and nothing of its north, east or west sides.
- Every vertical edge in the world — a wall corner, a tree trunk, a post — runs straight up the image. Nothing leans, nothing converges, nothing is foreshortened.
- No perspective, no vanishing point, no horizon, no sky. Nothing gets smaller because it is further away: a thing the same size in the world is the same size in pixels wherever it sits.
- One light direction for the whole sheet, from the upper left. Every object's shading and every contact shadow agrees with it.

BACKGROUND: the flat magenta between and around the objects is empty space and is keyed out by the tool. Leave it completely untouched right up to the edge of what you draw: no magenta tint on an object, no soft halo, no gradient, no drop shadow lying on the magenta. Do not paint ground under an object — the map draws its own ground underneath.

FRINGE TILES (the ids ending in _edge, _corner_out and _corner_in) are not whole tiles: each one is a ragged scrap of its own terrain drawn over the empty background, which the game lays on top of the neighbouring ground so the boundary between two terrains is not a straight line. Draw only the terrain, never the neighbour, and let the background show through the rest:
  - _edge: a band of the terrain along the NORTH edge of the slot, solid and full width at the very top, reaching about 10 of the tile's 32 pixels down at its deepest, with a ragged, organic, uneven southern boundary — clumps and gaps, never a straight line and never a repeating scallop. The bottom two thirds of the slot is background.
  - _corner_out: the same terrain filling only the NORTH-EAST corner, reaching about 10 pixels in from the north edge and 10 pixels in from the east edge, a ragged quarter-round. Everything else is background.
  - _corner_in: the exact inverse — the terrain fills the whole slot EXCEPT a ragged bite out of the SOUTH-WEST corner about 10 pixels deep from the south edge and 10 pixels in from the west edge. Only that bite is background.
A slot's own description below gives the MATERIAL only — what the terrain is made of. Where it also mentions an edge or a corner, the three shapes above win: draw the shape described here, in that material.
The north / north-east / south-west wording is fixed: the game rotates each of these three by 90, 180 and 270 degrees to cover the other sides and corners, so all three must be drawn at the orientation described and the terrain must meet the slot's edges at full strength wherever it touches them.

SLOTS — 12 of them:
Slot 1 (dirt_edge), 128x128 px, 1x1 tile: the fringe of a dirt lane: packed earth, dry and a little gravelly, over nothing else. Drawn as the _edge fringe shape described above, in this material.
Slot 2 (dirt_corner_out), 128x128 px, 1x1 tile: the same packed earth of a dirt lane, nothing else in the tile. Drawn as the _corner_out fringe shape described above, in this material.
Slot 3 (dirt_corner_in), 128x128 px, 1x1 tile: the same packed earth of a dirt lane, nothing else in the tile. Drawn as the _corner_in fringe shape described above, in this material.
Slot 4 (paving_edge), 128x128 px, 1x1 tile: the fringe of laid stone: outer blocks half sunk and broken, over nothing else. Drawn as the _edge fringe shape described above, in this material.
Slot 5 (paving_corner_out), 128x128 px, 1x1 tile: the same laid stone, its outer blocks broken, nothing else in the tile. Drawn as the _corner_out fringe shape described above, in this material.
Slot 6 (paving_corner_in), 128x128 px, 1x1 tile: the same laid stone, its outer blocks broken, nothing else in the tile. Drawn as the _corner_in fringe shape described above, in this material.
Slot 7 (water_edge), 128x128 px, 1x1 tile: the fringe of a stream bank: wet stones and mud where the water ends, over nothing else. Drawn as the _edge fringe shape described above, in this material.
Slot 8 (water_corner_out), 128x128 px, 1x1 tile: the same wet stones and mud of a stream bank, nothing else in the tile. Drawn as the _corner_out fringe shape described above, in this material.
Slot 9 (water_corner_in), 128x128 px, 1x1 tile: the same wet stones and mud of a stream bank, nothing else in the tile. Drawn as the _corner_in fringe shape described above, in this material.
Slot 10 (crop_edge), 128x128 px, 1x1 tile: the fringe of a barley plot: the last heads where the drills stop, over nothing else. Drawn as the _edge fringe shape described above, in this material.
Slot 11 (crop_corner_out), 128x128 px, 1x1 tile: the same standing barley of a worked plot, nothing else in the tile. Drawn as the _corner_out fringe shape described above, in this material.
Slot 12 (crop_corner_in), 128x128 px, 1x1 tile: the same standing barley of a worked plot, nothing else in the tile. Drawn as the _corner_in fringe shape described above, in this material.

RENDERING: Low resolution pixel art upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors. Checkerboard dithering only on large flat areas of ground, water and roof, never as noisy texture on small objects. Thin 1-pixel dark outlines around every object that stands on the ground. Muted earthy ground tones — grass, dirt, stone, timber — with saturated accents on roofs, doors, awnings, cloth and water. Flat shading with 2-3 tones per color and one consistent light direction across the whole map.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, pixels not aligned to the 4x4 block grid, anti-aliased or soft edges, a visible seam at a ground tile's edge, changing the size of the image
````

## 3. Afterwards

- [ ] All 12 slots filled; every border and every number exactly where it was
- [ ] Every art pixel is a 4x4 block on one grid — zoom in and check a diagonal edge is a clean staircase of blocks, not a soft ramp
- [ ] Nothing drawn in the gutters; the background between slots is still flat and untouched
- [ ] Each object stands on its slot's bottom edge, with clean magenta right up against it and no magenta tint on the object itself
- [ ] One light direction, upper left, across the whole sheet
- [ ] No text, labels, numbers of your own, swatches or signature

If one slot fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/fringes/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/fringes
```

That writes the 12 tile(s) into `story/field/tilesets/valley/atlas.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
