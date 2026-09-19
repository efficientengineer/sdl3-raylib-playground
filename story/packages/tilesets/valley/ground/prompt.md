# ChatGPT package: valley — ground tile sheet

## What exists already

- `story/field/tilesets/valley/atlas.png` — not started yet
- `story/field/tilesets/valley/cut/ground.png` — not cut yet

This sheet carries 12 of the tileset's entries: `grass`, `grass_tuft`, `grass_flower`, `dirt`, `dirt_rut`, `paving`, `paving_worn`, `water`, `gravel`, `crop`, `mud`, `bridge_deck`

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/ground/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 12 numbered slots filled in. Canvas: landscape, 1536x1024, exactly the same size and proportions as the template.

16-bit Sega Genesis era pixel art, early 1990s JRPG field map, Phantasy Star IV look.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS SHEET IS: the 'ground' tile sheet for the 'valley' tileset of a top-down 16-bit JRPG field map. Every slot is one tile or one multi-tile object, and in game each tile is exactly 32x32 pixels.

PIXEL GRID — the most important rule on this sheet. Everything is drawn at 4x. Every art pixel is a 4x4 block of identical colour, aligned to the slot: the blocks start at the slot's top-left inside corner and run in an even grid across and down it, so a 128-pixel slot is a 32x32 pixel-art tile blown up 4 times. No block is half a colour, no edge falls between blocks, no detail is finer than one block, and no two objects on the sheet use different block sizes.

PROJECTION — top-down oblique, the Phantasy Star IV field view, the same for every slot:

- The ground is seen from straight above: a patch of grass, dirt, paving or water is a flat texture with no thickness and no side visible.
- An object standing on the ground is seen from slightly in front of straight above: you see its top or roof AND its south-facing front, and nothing of its north, east or west sides.
- Every vertical edge in the world — a wall corner, a tree trunk, a post — runs straight up the image. Nothing leans, nothing converges, nothing is foreshortened.
- No perspective, no vanishing point, no horizon, no sky. Nothing gets smaller because it is further away: a thing the same size in the world is the same size in pixels wherever it sits.
- One light direction for the whole sheet, from the upper left. Every object's shading and every contact shadow agrees with it.

GROUND TILES: every slot is filled edge to edge with its texture, right up to the white border, opaque, with no frame, no vignette, no empty corner and no background showing. Each one is SEAMLESS on all four edges: the right edge continues into the left edge and the bottom edge into the top, so a floor tiled with copies of it shows no seam. Keep the detail even and small — no single large feature the eye can count across a field, no object standing on the ground, no character, no shadow cast by anything outside the tile.

SLOTS — 12 of them:
Slot 1 (grass), 128x128 px, 1x1 tile: short river-valley turf seen from straight above, worn to the soil in places, no single blade readable. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 2 (grass_tuft), 128x128 px, 1x1 tile: the same turf with a few standing tufts of longer grass, scattered off any grid. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 3 (grass_flower), 128x128 px, 1x1 tile: the same turf with a handful of small pale field flowers. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 4 (dirt), 128x128 px, 1x1 tile: packed earth of a village lane, dry, a little gravel worked into it. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 5 (dirt_rut), 128x128 px, 1x1 tile: packed earth with two cart ruts worn down the length of it. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 6 (paving), 128x128 px, 1x1 tile: laid stone of the village square, blocks of uneven size, mortar dark between them. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 7 (paving_worn), 128x128 px, 1x1 tile: the same laid stone, hollowed and polished where the town walks over it. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 8 (water), 256x128 px, 2x1 tiles: shallow running stream water, two frames, the light on it moved between them. 2 animation frames of the same tile side by side, left to right, each one 1x1 tile, the same subject with only the moving part changed, the motion looping from the last frame back to the first, and every frame obeying the rules below on its own. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 9 (gravel), 128x128 px, 1x1 tile: the grey grit of a working yard, raked flat and stained. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 10 (crop), 128x128 px, 1x1 tile: standing barley seen from above, the heads in rough drills. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 11 (mud), 128x128 px, 1x1 tile: churned wet ground of a cart yard, hoof and wheel marks standing in it. Seen straight down from directly above, seamless on all four edges, opaque.
Slot 12 (bridge_deck), 128x128 px, 1x1 tile: planks of a footbridge laid across the run of the stream, a rail board at each side. Seen straight down from directly above, seamless on all four edges, opaque.

RENDERING: Low resolution pixel art upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors. Checkerboard dithering only on large flat areas of ground, water and roof, never as noisy texture on small objects. Thin 1-pixel dark outlines around every object that stands on the ground. Muted earthy ground tones — grass, dirt, stone, timber — with saturated accents on roofs, doors, awnings, cloth and water. Flat shading with 2-3 tones per color and one consistent light direction across the whole map.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, pixels not aligned to the 4x4 block grid, anti-aliased or soft edges, a visible seam at a ground tile's edge, changing the size of the image
````

## 3. Afterwards

- [ ] All 12 slots filled; every border and every number exactly where it was
- [ ] Every art pixel is a 4x4 block on one grid — zoom in and check a diagonal edge is a clean staircase of blocks, not a soft ramp
- [ ] Nothing drawn in the gutters; the background between slots is still flat and untouched
- [ ] Each tile fills its slot to the border, and its left edge would meet its right edge without a seam
- [ ] One light direction, upper left, across the whole sheet
- [ ] No text, labels, numbers of your own, swatches or signature

If one slot fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/ground/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/ground
```

That writes the 12 tile(s) into `story/field/tilesets/valley/atlas.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
