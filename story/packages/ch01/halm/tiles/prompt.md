# ChatGPT package: halm — 7 tiles

## What exists already

0 of 7 files in this package have been cut already.

- slot 1 `story/field/tiles/cliff.png` — missing
- slot 2 `story/field/tiles/dirt.png` — missing
- slot 3 `story/field/tiles/grass.png` — missing
- slot 4 `story/field/tiles/plank.png` — missing
- slot 5 `story/field/tiles/stone.png` — missing
- slot 6 `story/field/tiles/wall_plaster.png` — missing
- slot 7 `story/field/tiles/wall_timber.png` — missing

## 1. Start a new chat and attach these files, in this order

1. `story/packages/ch01/halm/tiles/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 7 numbered slots filled in. Canvas: landscape, 1536x1024, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: ground and wall textures for a 2.5D field map, one per slot. Each slot is filled edge to edge with its texture, right up to the white border, with no frame, no vignette, no border of its own and no empty corner. Every tile is SEAMLESS: the right edge continues into the left edge and the bottom edge into the top, so a floor tiled with copies of it shows no seam and no repeating landmark. Keep the detail even: no single large feature that the eye can count across a field, no object, no character, no shadow of anything outside the tile. All of these tiles belong to one valley town and share one palette and one pixel size.

A ground tile is seen straight down from directly above. A wall tile is seen level from the front, is the face of a step in the ground, and repeats upward as well as sideways. In game every tile is 64x64 pixels, so keep the pixels large and the shapes simple.

SLOTS:
Slot 1 (cliff), wall tile, 315x315 px in the template: A face of broken valley rock in rough horizontal bands, cracked, with loose scree caught on the ledges. Seen level from the front, the face of a step in the ground, seamless on all four edges.
Slot 2 (dirt), ground tile, 315x315 px in the template: Packed pale earth of a walked lane, dry, with shallow wheel ruts and loose grit. Seen straight down from directly above, seamless on all four edges.
Slot 3 (grass), ground tile, 315x315 px in the template: Short valley grass in uneven clumps with bare earth showing through and a few small stones. Seen straight down from directly above, seamless on all four edges.
Slot 4 (plank), ground tile, 315x315 px in the template: Weathered timber decking boards laid one way, gapped, with nail heads at the joins. Seen straight down from directly above, seamless on all four edges.
Slot 5 (stone), ground tile, 315x315 px in the template: Flat grey paving slabs of uneven size laid close, with grass in the joints. Seen straight down from directly above, seamless on all four edges.
Slot 6 (wall_plaster), wall tile, 315x315 px in the template: Cream plaster over rubble stone, cracked and patched in places, with a low band of bare stone at the bottom. Seen level from the front, the face of a step in the ground, seamless on all four edges.
Slot 7 (wall_timber), wall tile, 315x315 px in the template: Dark timber framing over pale plaster panels, the beams pegged at the joins. Seen level from the front, the face of a step in the ground, seamless on all four edges.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, a border or frame inside a slot, a visible seam at a tile edge, one big feature in the middle of a tile, objects or characters, changing the size of the image
````

## 3. Afterwards

- [ ] All 7 slots filled, every border and number still exactly where it was
- [ ] Nothing drawn in the gutters; the black between the slots is still flat black
- [ ] Each tile fills its slot to the border, with no frame and no empty corner
- [ ] Tiling test: the left edge of a tile would meet its right edge without a seam
- [ ] No text, labels or swatches anywhere

If one tile fails, reply in the same chat: "Redraw only slot 4 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/ch01/halm/tiles/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/tiles
```

That writes the 7 tile file(s) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
