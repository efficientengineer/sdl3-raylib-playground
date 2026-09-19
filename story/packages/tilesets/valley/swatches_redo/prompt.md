# ChatGPT package: swatch redo — slot(s) 2 (grass_dry), 5 (dirt)

## What exists already

- redo of `story/packages/tilesets/valley/swatches`
- only slot(s) 2 (grass_dry), 5 (dirt) are written; the rest of the sheet is left exactly as it is

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/swatches_redo/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached sheet with ONLY the outlined slot(s) redrawn. Canvas: exactly the same size and proportions as the image I attached.

Top-down JRPG field art in the clean 16-bit manner: flat luminous fields of colour, big simple shapes, soft painted edges, sparse deliberate detail. Every shadow is a single cool blue-violet tone, never a gradient and never grey.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

THIS IS A CORRECTION, NOT A NEW SHEET. The image I attached is the sheet you already drew. Every slot in it is finished and approved EXCEPT the one or two that have been cleared to the flat background colour and outlined in white with a number beside them. Return the whole image again with those slots filled in and EVERY OTHER PIXEL of the image identical to what I sent — same slots, same colours, same positions, down to the pixel. Do not redraw, retouch, recolour, shift or re-light anything else.

The slot(s) to redraw, and what was wrong:
  Slot 2 (grass_dry): dry, slightly yellower grass, only a small step from slot 1 — the same green family, a shade paler and warmer, not a different material and nowhere near orange. Somebody should read it as the same field in a dry summer.
    It is still a 4x4-tile ground texture: flat, seamless on all four edges, no border, no lit side, no vignette, no object, calm and low in contrast. It has to sit beside the other slots in this sheet without looking like it came from a different game, so take its palette from them.
  Slot 5 (dirt): packed earth of a village lane: pale, greyish tan, much less saturated, NO orange. Think dust and dry mud walked flat, not clay and not sand.
    It is still a 3x3-tile ground texture: flat, seamless on all four edges, no border, no lit side, no vignette, no object, calm and low in contrast. It has to sit beside the other slots in this sheet without looking like it came from a different game, so take its palette from them.

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, dithering, checkerboard texture, stippling, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, changing any slot that is not outlined, moving a slot, changing the size of the image, adding a border or a caption
````

## 3. Afterwards

- [ ] Only slot(s) 2 (grass_dry), 5 (dirt) changed; every other slot is pixel-identical
- [ ] The new slot sits in the same palette as its neighbours
- [ ] Nothing moved, nothing resized

If it comes back with other slots altered, say so in the same chat and ask again: "Keep every other slot exactly as I sent it."

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/swatches_redo/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/swatches_redo
```

That writes only the file(s) for slot(s) 2 (grass_dry), 5 (dirt). Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
