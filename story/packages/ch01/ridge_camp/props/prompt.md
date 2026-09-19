# ChatGPT package: ridge_camp — 3 props

## What exists already

3 of 3 files in this package have been cut already.

- slot 1 `story/field/props/bedroll.png` — **exists**
- slot 2 `story/field/props/campfire.png` — **exists**
- slot 3 `story/field/props/ridge_rock.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/packages/ch01/ridge_camp/props/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 3 numbered slots filled in. Canvas: portrait, 1024x1536, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: single objects for a 2.5D field map, one object per slot, each cut out against the flat magenta. The magenta is not a backdrop, it is empty space: it runs right up to the edge of the object on every side. No ground, no grass, no paving, no base plate, no cast shadow, no glow, no scenery and no second object in a slot.

CAMERA, the same for every slot: a front three-quarter view from slightly above, looking about fifty degrees down, as if all of these objects stood in one town seen from one fixed camera. The object sits upright, centred left to right, and touches the bottom edge of its slot, because that line is where it meets the ground in game.

SCALE: a slot is a grid of map cells, 64 pixels to the cell in game, and each slot below says how many cells it is. Objects share one scale across the sheet: a four-cell hall is four times the width of a one-cell barrel and is drawn with the same size of pixel.

SLOTS:
Slot 1 (bedroll), 2 x 1 cells, 607x300 px in the template: A rolled-out blanket bed on flat ground with a folded pack at the head end and a pair of boots set beside it. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 2 (campfire), 1 x 1 cells, 300x300 px in the template: A small fire of stacked sticks in a ring of hand-sized stones, embers under it, a blackened pot pushed to one side. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 3 (ridge_rock), 3 x 2 cells, 913x607 px in the template: A wind-scoured outcrop of pale hill stone standing clear of the grass, flat on top, undercut on the windward side. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, ground or grass or paving under an object, a cast shadow on the magenta, a base plate or pedestal, a scene or background inside a slot, two objects in one slot, a soft blurred or glowing edge where an object meets the magenta, changing the size of the image
````

## 3. Afterwards

- [ ] All 3 slots filled, every border and number still exactly where it was
- [ ] The magenta is untouched outside the slots, and comes right up to each object
- [ ] The edge of every object is hard against the magenta, not soft, blurred or glowing
- [ ] No ground, shadow, base plate or scenery under or behind an object
- [ ] Every object stands on the bottom edge of its slot and shares one camera angle
- [ ] One scale across the sheet: the big buildings really are bigger than the barrel

If one prop fails, reply in the same chat: "Redraw only slot 2 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/ch01/ridge_camp/props/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/ridge_camp/props
```

That writes the 3 prop sprite(s) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
