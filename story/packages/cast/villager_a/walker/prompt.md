# ChatGPT package: villager_a — walk sheet (no cast entry)

## What exists already

- `story/field/walkers/villager_a.png` — **exists**, 1024x1536

## 1. Start a new chat and attach these files, in this order

1. `story/packages/cast/villager_a/walker/template.png`
2. `story/refs/style.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 16 numbered slots filled in. Canvas: portrait, 1024x1536, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS IS: a walking sprite sheet for villager_a in a 2.5D field map, 16 frames of one character, cut out against the flat magenta. The magenta is empty space, not a backdrop: it runs right up to the figure on every side. No ground, no shadow, no scenery, no props that are not part of the costume.

THE GRID, four rows of four frames, read left to right, top to bottom:
Row 1 (slots 1-4), facing S: the character walks toward the camera, seen from the front.
Row 2 (slots 5-8), facing W: walks to the viewer's left, seen in side view from their right side.
Row 3 (slots 9-12), facing E: walks to the viewer's right, seen in side view from their left side. This is row 2 mirrored, and the costume details stay on the correct side of the body.
Row 4 (slots 13-16), facing N: walks away from the camera, seen from behind.

THE COLUMNS, the same four poses in every row: column 1 standing still with both feet together; column 2 mid-stride with the left leg forward and the right arm forward; column 3 the same standing pose as column 1, identical to it; column 4 mid-stride with the right leg forward and the left arm forward. Frames 1 and 3 must match each other exactly, because the game plays them as 1, 2, 3, 4 in a loop.

FRAMING, the same in all 16 frames: the character is drawn at the same size, upright and centred left to right, with the feet on the bottom edge of the frame and a small gap of magenta above the head. The head does not move up or down between frames, so the sheet does not bob when it is played. Each frame is 186x282 pixels here and is squeezed down to 32x48 in game, so keep the pixels large, the silhouette clear and the face simple: a few pixels of eye, no fine detail.

CHARACTER: a middle-aged woman in a faded blue kerchief, a long brown skirt and a grey shawl, a rolled bed roped across her back

CHARACTER DESIGN: 1990s Japanese anime and manga character design, like the cast of a 1993 sci-fantasy JRPG or OVA: youthful heroic young adults, large expressive eyes with simple highlights, small noses and pointed chins, big layered spiky hair with hard-edged shine bands, slim necks, clean readable silhouettes. Sci-fantasy costumes: high collars, bodysuits, long coats, half-capes, oversized shoulder plates, headbands, sashes. Simple clean shapes and large flat areas of color. Clean dark outlines, flat cel shading with only 2-3 tones per color, no gradients.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, ground or shadow under the feet, a different size or costume between frames, the head bobbing between frames, a background inside a frame, a soft blurred or glowing edge where the figure meets the magenta, changing the size of the image
````

## 3. Afterwards

- [ ] All 16 slots filled, every border and number still exactly where it was
- [ ] Rows in the order S, W, E, N; columns stand, step-left, stand, step-right
- [ ] Slots 1 and 3 of each row are the same pose; the head sits at the same height in all 16
- [ ] Feet on the bottom edge, magenta right up to the figure, no shadow and no ground
- [ ] Hair, outfit, colors and marks match the reference sheet in every frame

If one row fails, reply in the same chat: "Redraw only slots 5 to 8 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/cast/villager_a/walker/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/cast/villager_a/walker
```

That writes `story/field/walkers/villager_a.png`, the 4x4 walk sheet. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
