# ChatGPT package: west_road — 2 props

## What exists already

0 of 2 files in this package have been cut already.

- slot 1 `story/field/props/culvert.png` — missing
- slot 2 `story/field/props/milestone.png` — missing

## 1. Start a new chat and attach these files, in this order

1. `story/packages/ch01/west_road/props/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 2 numbered slots filled in. Canvas: landscape, 1536x1024, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: single objects for a 2.5D field map, one object per slot, each cut out against the flat magenta. The magenta is not a backdrop, it is empty space: it runs right up to the edge of the object on every side. No ground, no grass, no paving, no base plate, no cast shadow, no glow, no scenery and no second object in a slot.

CAMERA, the same for every slot: a front three-quarter view from slightly above, looking about fifty degrees down, as if all of these objects stood in one town seen from one fixed camera. The object sits upright, centred left to right, and touches the bottom edge of its slot, because that line is where it meets the ground in game.

SCALE: a slot is a grid of map cells, 64 pixels to the cell in game, and each slot below says how many cells it is. Objects share one scale across the sheet: a four-cell hall is four times the width of a one-cell barrel and is drawn with the same size of pixel.

SLOTS:
Slot 1 (culvert), 3 x 2 cells, 1031x685 px in the template: A stone culvert mouth under a road, the arch half fallen and the roadway above it collapsed into the hole, loose blocks in the ditch below. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 2 (milestone), 1 x 1 cells, 339x339 px in the template: A knee-high roadside marker stone, weathered round at the top, one flat face scrubbed bare. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, ground or grass or paving under an object, a cast shadow on the magenta, a base plate or pedestal, a scene or background inside a slot, two objects in one slot, a soft blurred or glowing edge where an object meets the magenta, changing the size of the image
````

## 3. Afterwards

- [ ] All 2 slots filled, every border and number still exactly where it was
- [ ] The magenta is untouched outside the slots, and comes right up to each object
- [ ] The edge of every object is hard against the magenta, not soft, blurred or glowing
- [ ] No ground, shadow, base plate or scenery under or behind an object
- [ ] Every object stands on the bottom edge of its slot and shares one camera angle
- [ ] One scale across the sheet: the big buildings really are bigger than the barrel

If one prop fails, reply in the same chat: "Redraw only slot 2 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/ch01/west_road/props/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/west_road/props
```

That writes the 2 prop sprite(s) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
