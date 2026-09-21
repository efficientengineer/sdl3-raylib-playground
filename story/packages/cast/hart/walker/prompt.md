# ChatGPT package: Hart — walk sheet

## What exists already

- `story/field/walkers/hart.png` — missing

## 1. Start a new chat and attach these files, in this order

1. `story/packages/cast/hart/walker/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/refs/hart.png`
4. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 9 numbered slots filled in. Canvas: portrait, 1024x1536, the same size as the template.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: CHARACTER reference for Hart. The walker is this character: keep the face, hair, outfit and colours identical to this image in every frame. Use it for the design only; ignore its background and its three-panel layout.
Image 4: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS IS: walking sprite sheets for Hart in a top-down field map, cut out against the flat magenta. The magenta is empty space, not a backdrop: it runs right up to the figure on every side. No ground, no shadow, no scenery, no props that are not part of the costume.

SLOTS 1-9 ARE HART. Hart, a stocky broad-shouldered man of sixty with a straight back and a stiff left leg, iron-grey hair cropped short and pushed about, deep-set brown eyes and a clean-shaven square jaw, wire spectacles pushed up on his forehead, a hinged brace of dark leather and iron strapped over his left knee, a scorched leather apron with a folding rule and two pencils in the chest pocket over a faded slate-blue work coat with the sleeves rolled to the elbow, big worked hands with one finger bandaged, patched canvas trousers, heavy laced boots, a plain hunter's knife on the belt.
  Slots 1-3, facing S: walks toward the camera, seen from the front.
  Slots 4-6, facing side: walks to the viewer's LEFT, seen in full side view from their right side. There is only one side row: the game mirrors it for the other direction, so draw the character so that mirroring it would still be right — nothing that belongs on one particular side of the body.
  Slots 7-9, facing N: walks away from the camera, seen from behind.

THE THREE COLUMNS, the same in every row: column 1 is standing still with both feet together and arms at rest; column 2 is mid-stride with the LEFT leg forward and the right arm forward; column 3 is mid-stride with the RIGHT leg forward and the left arm forward. The game plays them 1, 2, 1, 3 in a loop, so column 1 has to read as the resting pose between the two strides.

FRAMING, the same in every frame on the sheet: the character is drawn at the same size, upright and centred left to right, with the feet on the bottom edge of the frame and a small gap of magenta above the head. The head does not move up or down between frames, so the sheet does not bob when it is played. Each frame is 282x426 pixels here and is stored at 128x192, so keep the pixels large, the silhouette clear and the face simple: a few pixels of eye, no fine detail.

Each character must match their own attached reference sheet exactly: same face, hair, outfit, colours and marks, in every one of their frames. Two characters on this sheet must not borrow each other's clothes or hair.

CHARACTER DESIGN: 1990s Japanese anime and manga character design, like the cast of a 1993 sci-fantasy JRPG or OVA: youthful heroic young adults, large expressive eyes with simple highlights, small noses and pointed chins, big layered spiky hair with hard-edged shine bands, slim necks, clean readable silhouettes. Sci-fantasy costumes: high collars, bodysuits, long coats, half-capes, headbands, sashes, work clothes and travelling clothes. Armour only when the character's description asks for it. Simple clean shapes and large flat areas of color. Clean dark outlines, flat cel shading with only 2-3 tones per color, no gradients.

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, even panel grid, panels filling the whole frame, deep perspective, centered full-figure composition, text, letters, captions, speech bubbles, watermark, signature, drawing outside a slot, moving or covering a slot number, ground or shadow under the feet, a different size or costume between frames, the head bobbing between frames, a background inside a frame, a soft blurred or glowing edge where the figure meets the magenta, changing the size of the image
````

## 3. Afterwards

- [ ] All 9 slots filled, every border and number still exactly where it was
- [ ] Columns stand, step-A, step-B; column 1 is the resting pose in every row
- [ ] The head sits at the same height in every frame of a character
- [ ] Feet on the bottom edge, magenta right up to the figure, no shadow and no ground
- [ ] Hair, outfit, colours and marks match each character's own reference sheet

If one row fails, reply in the same chat: "Redraw only slots 4 to 6 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/cast/hart/walker/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/cast/hart/walker
```

That writes `story/field/walkers/hart.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
