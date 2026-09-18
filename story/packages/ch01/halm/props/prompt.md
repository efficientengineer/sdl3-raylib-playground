# ChatGPT package: halm — 9 props

## What exists already

9 of 9 files in this package have been cut already.

- slot 1 `story/field/props/barrel.png` — **exists**
- slot 2 `story/field/props/cart.png` — **exists**
- slot 3 `story/field/props/fence.png` — **exists**
- slot 4 `story/field/props/scale_bench.png` — **exists**
- slot 5 `story/field/props/sign.png` — **exists**
- slot 6 `story/field/props/tree_a.png` — **exists**
- slot 7 `story/field/props/tree_b.png` — **exists**
- slot 8 `story/field/props/well.png` — **exists**
- slot 9 `story/field/props/yard_wall.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/packages/ch01/halm/props/template.png`
2. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 9 numbered slots filled in. Canvas: portrait, 1024x1536, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: single objects for a 2.5D field map, one object per slot, each cut out against the flat magenta. The magenta is not a backdrop, it is empty space: it runs right up to the edge of the object on every side. No ground, no grass, no paving, no base plate, no cast shadow, no glow, no scenery and no second object in a slot.

CAMERA, the same for every slot: a front three-quarter view from slightly above, looking about fifty degrees down, as if all of these objects stood in one town seen from one fixed camera. The object sits upright, centred left to right, and touches the bottom edge of its slot, because that line is where it meets the ground in game.

SCALE: a slot is a grid of map cells, 64 pixels to the cell in game, and each slot below says how many cells it is. Objects share one scale across the sheet: a four-cell hall is four times the width of a one-cell barrel and is drawn with the same size of pixel.

SLOTS:
Slot 1 (barrel), 1 x 1 cells, 158x158 px in the template: A squat water barrel of dark staves with two iron hoops, brim full, a tin cup hooked on the rim. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 2 (cart), 2 x 2 cells, 323x323 px in the template: A two-wheeled handcart with its shafts down, loaded above the sides with a rolled mattress and tied bundles. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 3 (fence), 2 x 1 cells, 323x158 px in the template: Three rails pegged between two split posts, one rail newer than the others and set slightly proud. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 4 (scale_bench), 2 x 1 cells, 323x158 px in the template: A low plank bench with a pair of iron grain scales bolted to one end, a stack of small lead weights beside the pan and one weight lying loose. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 5 (sign), 1 x 2 cells, 158x323 px in the template: A blank plank board nailed across a single post at head height, the wood split at one corner. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 6 (tree_a), 2 x 3 cells, 323x488 px in the template: A broad valley tree with a thick low trunk and a heavy rounded crown, a few dead branches on one side. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 7 (tree_b), 1 x 2 cells, 158x323 px in the template: A thin young tree with a straight pale trunk and a sparse upright crown, staked at the base. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 8 (well), 1 x 2 cells, 158x323 px in the template: A round stone well with a low wall, a plank roof on two posts, a rope on a winding drum and a wooden bucket hooked at the rim. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.
Slot 9 (yard_wall), 3 x 2 cells, 488x323 px in the template: A shoulder-high grain-yard wall of dry-laid stone with a flat coping course, one stretch bellied out and patched with newer stone. Front three-quarter view from above, standing on the bottom edge of the slot, magenta on every other side.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, ground or grass or paving under an object, a cast shadow on the magenta, a base plate or pedestal, a scene or background inside a slot, two objects in one slot, a soft blurred or glowing edge where an object meets the magenta, changing the size of the image
````

## 3. Afterwards

- [ ] All 9 slots filled, every border and number still exactly where it was
- [ ] The magenta is untouched outside the slots, and comes right up to each object
- [ ] The edge of every object is hard against the magenta, not soft, blurred or glowing
- [ ] No ground, shadow, base plate or scenery under or behind an object
- [ ] Every object stands on the bottom edge of its slot and shares one camera angle
- [ ] One scale across the sheet: the big buildings really are bigger than the barrel

If one prop fails, reply in the same chat: "Redraw only slot 2 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/ch01/halm/props/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/ch01/halm/props
```

That writes the 9 prop sprite(s) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
