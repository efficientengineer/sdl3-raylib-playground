# ChatGPT package: valley — nature tile sheet

## What exists already

- `story/field/tilesets/valley/atlas.png` — not started yet
- `story/field/tilesets/valley/cut/nature.png` — not cut yet

This sheet carries 7 of the tileset's entries: `hedge`, `boulder`, `stump`, `tree`, `tree_2`, `tree_bare`, `orchard_tree`

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/nature/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 7 numbered slots filled in. Canvas: landscape, 1536x1024, exactly the same size and proportions as the template.

16-bit Sega Genesis era pixel art, early 1990s JRPG field map, Phantasy Star IV look.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS SHEET IS: the 'nature' tile sheet for the 'valley' tileset of a top-down 16-bit JRPG field map. Every slot is one tile or one multi-tile object, and in game each tile is exactly 32x32 pixels.

PIXEL GRID — the most important rule on this sheet. Everything is drawn at 4x. Every art pixel is a 4x4 block of identical colour, aligned to the slot: the blocks start at the slot's top-left inside corner and run in an even grid across and down it, so a 128-pixel slot is a 32x32 pixel-art tile blown up 4 times. No block is half a colour, no edge falls between blocks, no detail is finer than one block, and no two objects on the sheet use different block sizes.

PROJECTION — top-down oblique, the Phantasy Star IV field view, the same for every slot:

- The ground is seen from straight above: a patch of grass, dirt, paving or water is a flat texture with no thickness and no side visible.
- An object standing on the ground is seen from slightly in front of straight above: you see its top or roof AND its south-facing front, and nothing of its north, east or west sides.
- Every vertical edge in the world — a wall corner, a tree trunk, a post — runs straight up the image. Nothing leans, nothing converges, nothing is foreshortened.
- No perspective, no vanishing point, no horizon, no sky. Nothing gets smaller because it is further away: a thing the same size in the world is the same size in pixels wherever it sits.
- One light direction for the whole sheet, from the upper left. Every object's shading and every contact shadow agrees with it.

BACKGROUND: the flat magenta between and around the objects is empty space and is keyed out by the tool. Leave it completely untouched right up to the edge of what you draw: no magenta tint on an object, no soft halo, no gradient, no drop shadow lying on the magenta. Do not paint ground under an object — the map draws its own ground underneath.

MULTI-TILE OBJECTS: a slot taller or wider than one tile holds one object, drawn to fill its slot's footprint and STANDING ON THE SLOT'S BOTTOM EDGE — its base touches the bottom inside edge of the border, centred left to right. No ground under it and no cast shadow beyond a contact shadow no more than one tile high tucked under its south face. A door is one tile wide and about 1.3 tiles tall, centred on a tile column so it lines up with the grid. Every slot on this sheet is drawn to ONE common scale: a tile is a stride, a person would be one tile wide and one and a half tiles tall.

SLOTS — 7 of them:
Slot 1 (hedge), 128x128 px, 1x1 tile: a thick clipped field hedge, dark and uneven along the top. Standing on the slot's bottom edge, everything else left as background.
Slot 2 (boulder), 128x128 px, 1x1 tile: a single grey field stone too big to shift, moss on its north side. Standing on the slot's bottom edge, everything else left as background.
Slot 3 (stump), 128x128 px, 1x1 tile: a felled tree stump with the axe cuts still on it. Standing on the slot's bottom edge, everything else left as background.
Slot 4 (tree), 256x384 px, 2x3 tiles: a broad valley tree, dense crown seen from above and slightly in front, one trunk at the foot. Standing on the slot's bottom edge, everything else left as background.
Slot 5 (tree_2), 256x384 px, 2x3 tiles: a second tree, narrower and darker than the first, so two together do not read as a pair. Standing on the slot's bottom edge, everything else left as background.
Slot 6 (tree_bare), 256x384 px, 2x3 tiles: a dead tree, bare limbs, the bark off one side. Standing on the slot's bottom edge, everything else left as background.
Slot 7 (orchard_tree), 256x384 px, 2x3 tiles: a low pruned fruit tree on a short trunk, the crown kept round. Standing on the slot's bottom edge, everything else left as background.

RENDERING: Low resolution pixel art upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors. Checkerboard dithering only on large flat areas of ground, water and roof, never as noisy texture on small objects. Thin 1-pixel dark outlines around every object that stands on the ground. Muted earthy ground tones — grass, dirt, stone, timber — with saturated accents on roofs, doors, awnings, cloth and water. Flat shading with 2-3 tones per color and one consistent light direction across the whole map.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, pixels not aligned to the 4x4 block grid, anti-aliased or soft edges, a visible seam at a ground tile's edge, changing the size of the image
````

## 3. Afterwards

- [ ] All 7 slots filled; every border and every number exactly where it was
- [ ] Every art pixel is a 4x4 block on one grid — zoom in and check a diagonal edge is a clean staircase of blocks, not a soft ramp
- [ ] Nothing drawn in the gutters; the background between slots is still flat and untouched
- [ ] Each object stands on its slot's bottom edge, with clean magenta right up against it and no magenta tint on the object itself
- [ ] One light direction, upper left, across the whole sheet
- [ ] No text, labels, numbers of your own, swatches or signature

If one slot fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/nature/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/nature
```

That writes the 7 tile(s) into `story/field/tilesets/valley/atlas.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
