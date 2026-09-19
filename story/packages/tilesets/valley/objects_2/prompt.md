# ChatGPT package: valley — objects_2 tile sheet (2 of 4)

## What exists already

- `story/field/tilesets/valley/atlas.png` — **exists**
- `story/field/tilesets/valley/cut/objects_2.png` — **cut already**

This sheet carries 6 of the tileset's entries: `ladder_house`, `house_e`, `house_c`, `barn`, `shed`, `stable`

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/objects_2/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 6 numbered slots filled in. Canvas: landscape, 1536x1000, exactly the same size and proportions as the template.

PIXEL GRID — THE MOST IMPORTANT RULE ON THIS SHEET. Everything is drawn at 4x. Every art pixel is a 4x4 block of identical colour, aligned to the slot: the blocks start at the slot's top-left inside corner and run in an even grid across and down it, so a 128-pixel slot is a 32x32 pixel-art tile blown up 4 times. No block is half a colour, no edge falls between blocks, no detail is finer than one block, and no two objects on the sheet use different block sizes. This is pixel art at 4x, not a smooth drawing shrunk down.

Top-down JRPG field art in the clean 16-bit manner: flat luminous fields of colour, big simple shapes, soft painted edges, sparse deliberate detail. Every shadow is a single cool blue-violet tone, never a gradient and never grey.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS SHEET IS: the 'objects_2' tile sheet for the 'valley' tileset of a top-down 16-bit JRPG field map. Every slot is one tile or one multi-tile object, and in game each tile is exactly 32x32 pixels, so a 128-pixel slot is one tile and a 256x384 slot is a 2x3-tile object.

THIS SHEET HOLDS 6 stamp tile(s) in slot(s) 1-6. The rules for each kind:

STAMPS (objects) — SLOT(S) 1-6. STAMPS are objects that stand on the ground: a house, a tree, a barrel. Each one is drawn to fill its slot's width and STANDS ON THE SLOT'S BOTTOM EDGE — its base touches the bottom inside edge of the border, centred left to right — and everything else in the slot is left as the flat background, which the tool keys out. No ground under it (the map draws its own), no cast shadow beyond a contact shadow no more than one tile high tucked under its south face. Every stamp on this sheet is drawn to ONE COMMON SCALE: a tile is a stride, a person would be one tile wide and about one and a half tiles tall, and a door is one tile wide and about 1.3 tiles tall, centred on a tile column so it lines up with the grid.

BACKGROUND: the flat magenta around and between the objects is empty space and is keyed out by the tool. Leave it completely untouched right up to the edge of what you draw: no magenta tint on an object, no soft halo, no gradient, no drop shadow lying on the magenta.

PROJECTION — top-down oblique, the Phantasy Star IV field view, the same for every slot:

- The ground is seen from straight above: a patch of grass, dirt, paving or water is a flat texture with no thickness and no side visible.
- An object standing on the ground is seen from slightly in front of straight above: you see its top or roof AND its south-facing front, and nothing of its north, east or west sides.
- Every vertical edge in the world — a wall corner, a tree trunk, a post — runs straight up the image. Nothing leans, nothing converges, nothing is foreshortened.
- No perspective, no vanishing point, no horizon, no sky. Nothing gets smaller because it is further away: a thing the same size in the world is the same size in pixels wherever it sits.
- One light direction for the whole sheet, from the upper left. Every object's shading and every contact shadow agrees with it.
- The slots are different sizes because the things in them are different sizes. Nothing is scaled to fill a bigger slot: a barrel in a 1-tile slot and a barn in a 4x3 slot are drawn at the same number of pixels to the foot.

SLOTS — 6 of them, grouped by kind:

STAMPS (objects):
Slot 1 (ladder_house), 512x512 px = 4x4 tiles: a two-storey house with a ladder left leaning against the eaves and slates missing off the ridge. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.
Slot 2 (house_e), 512x512 px = 4x4 tiles: the tallest house of the town, three windows on the south face, a chimney at the west gable. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.
Slot 3 (shed), 384x384 px = 3x3 tiles: a plank outbuilding with a shallow board roof and a wide doorway. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.
Slot 4 (stable), 384x384 px = 3x3 tiles: a stable with a split door standing open and straw trodden at the threshold. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.
Slot 5 (house_c), 512x384 px = 4x3 tiles: a wider house with two windows and a bench beside the door. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.
Slot 6 (barn), 512x384 px = 4x3 tiles: a barn with tall boarded doors and a hoist beam under the gable. Its art fills its footprint edge to edge: walls to the left and right edges of its tiles and the front wall down to the bottom edge, with no ground, grass or shadow inside the footprint. Standing on the slot's bottom edge, everything else left as background.

RENDERING: Low resolution pixel art upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, drawn from the attached color palette. Flat tones only: no dithering anywhere, no checkerboard, no stipple, no noise — a large area of ground, water or roof is broken up by drawn detail, by a band of a neighboring tone, or it stays flat. Thin 1-pixel dark outlines around every object that stands on the ground. Muted earthy ground tones — grass, dirt, stone, timber — with saturated accents on roofs, doors, awnings, cloth and water. Flat shading with 2-3 tones per color and one consistent light direction across the whole map.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, dithering, checkerboard texture, stippling, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, pixels not aligned to the 4x4 block grid, anti-aliased or soft edges, a visible seam at a ground tile's edge, background colour showing inside a ground tile, changing the size of the image
````

## 3. Afterwards

- [ ] All 6 slots filled; every border and every number exactly where it was
- [ ] Every art pixel is a 4x4 block on one grid — zoom in and check a diagonal edge is a clean staircase of blocks, not a soft ramp
- [ ] Nothing drawn in the gutters; the background between slots is still flat and untouched
- [ ] Each object (1-6) stands on its slot's bottom edge, with clean background right up against it and no magenta tint on the object itself
- [ ] One light direction, upper left, across the whole sheet
- [ ] No text, labels, numbers of your own, swatches or signature

If one slot fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/objects_2/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/objects_2
```

That writes the 6 tile(s) into `story/field/tilesets/valley/atlas.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
