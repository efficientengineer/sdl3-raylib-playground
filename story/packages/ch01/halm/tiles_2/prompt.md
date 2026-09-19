# ChatGPT package: halm — 6 tiles (2 of 2)

## What exists already

6 of 6 files in this package have been cut already.

- slot 1 `story/field/tiles/fence_top.png` — **exists**
- slot 2 `story/field/tiles/fence_wood.png` — **exists**
- slot 3 `story/field/tiles/hedge.png` — **exists**
- slot 4 `story/field/tiles/hedge_top.png` — **exists**
- slot 5 `story/field/tiles/wall_stone.png` — **exists**
- slot 6 `story/field/tiles/wall_stone_top.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/packages/ch01/halm/tiles_2/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 6 numbered slots filled in. Canvas: landscape, 1536x1024, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: ground and wall textures for a 2.5D field map, one per slot. Each slot is filled edge to edge with its texture, right up to the white border, with no frame, no vignette, no border of its own and no empty corner. Every tile is SEAMLESS: the right edge continues into the left edge and the bottom edge into the top, so a floor tiled with copies of it shows no seam and no repeating landmark. Keep the detail even: no single large feature that the eye can count across a field, no object, no character, no shadow of anything outside the tile. All of these tiles belong to one valley town and share one palette and one pixel size.

A ground tile is seen straight down from directly above. A wall tile is seen level from the front, is the face of a step in the ground, and repeats upward as well as sideways. In game every tile is 64x64 pixels, so keep the pixels large and the shapes simple.

SLOTS:
Slot 1 (fence_top), ground tile, 429x429 px in the template: The top of a timber fence seen straight down from above: the upper rail running the length of the tile with its grain along it, a square post head cut level at intervals, the strip either side of the rail the same flat dark colour as the fence face. Seen straight down from directly above, seamless on all four edges.
Slot 2 (fence_wood), wall tile, 429x429 px in the template: A post-and-rail timber fence seen level from the front, two split rails pegged between square posts, the wood grey and shaken; the gaps between the rails are one flat dark colour, the same on every tile. Seen level from the front, the face of a step in the ground, seamless on all four edges.
Slot 3 (hedge), wall tile, 429x429 px in the template: A clipped hedge face of dense small leaves cut level, dark green with olive lights, a few bare twigs showing through near the bottom. Seen level from the front, the face of a step in the ground, seamless on all four edges.
Slot 4 (hedge_top), ground tile, 429x429 px in the template: The top of a clipped hedge seen straight down from above, dense leaf cover cut level with a slightly uneven ridge along the middle and shadow between the clumps. Seen straight down from directly above, seamless on all four edges.
Slot 5 (wall_stone), wall tile, 429x429 px in the template: A dry stone wall face of stacked grey field stones in uneven courses, no mortar, small chips packed into the gaps and lichen on the weathered faces. Seen level from the front, the face of a step in the ground, seamless on all four edges.
Slot 6 (wall_stone_top), ground tile, 429x429 px in the template: The top of a dry stone wall seen straight down from above: a run of flat grey capstones laid across the width, chipped at the edges, moss in the joints between them. Seen straight down from directly above, seamless on all four edges.

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, a visible seam at a tile edge, one big feature in the middle of a tile, objects or characters, changing the size of the image
````

## 3. Afterwards

- [ ] All 6 slots filled, every border and number still exactly where it was
- [ ] Nothing drawn in the gutters; the black between the slots is still flat black
- [ ] Each tile fills its slot to the border, with no frame and no empty corner
- [ ] Tiling test: the left edge of a tile would meet its right edge without a seam
- [ ] No text, labels or swatches anywhere

If one tile fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/ch01/halm/tiles_2/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/tiles_2
```

That writes the 6 tile file(s) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
