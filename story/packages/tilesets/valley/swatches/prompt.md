# ChatGPT package: valley — the ground swatches (9 terrains)

## What exists already

- `story/field/tilesets/valley/swatches/grass.png` — **exists**
- `story/field/tilesets/valley/swatches/grass_dry.png` — **exists**
- `story/field/tilesets/valley/swatches/crop.png` — **exists**
- `story/field/tilesets/valley/swatches/mud.png` — **exists**
- `story/field/tilesets/valley/swatches/dirt.png` — **exists**
- `story/field/tilesets/valley/swatches/gravel.png` — **exists**
- `story/field/tilesets/valley/swatches/paving.png` — **exists**
- `story/field/tilesets/valley/swatches/bridge_deck.png` — **exists**
- `story/field/tilesets/valley/swatches/water.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/swatches/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 9 numbered slots filled in. Canvas: portrait, 1024x1536, exactly the same size and proportions as the template.

Top-down JRPG field art in the clean 16-bit manner: flat luminous fields of colour, big simple shapes, soft painted edges, sparse deliberate detail. Every shadow is a single cool blue-violet tone, never a gradient and never grey.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THESE ARE: ground textures, one per slot, each one a piece cut from the middle of an endless field of that material. Each slot is filled edge to edge, right up to its white border, and is SEAMLESS on all four edges: the right edge continues into the left and the bottom into the top, so a whole map tiled with copies of it shows no join and no landmark. Draw them FLAT. No border of any kind, no frame, no vignette, no darker or lighter side, no light falling across the slot, no shadow of anything outside it, no grass creeping in at an edge, no object, no path, no water's edge, no single feature a player could point at and remember. Calm and low in contrast: 3 or 4 flat tones of the material, close together, with the detail even across the whole slot. This texture will have flowers, stones and tufts scattered over it by the game, so it must be quiet enough to sit underneath them.

SLOTS:
Slot 1 (grass), 4x4 tiles of ground, 390x390 px in the template: short river-valley turf seen from straight above, even and quiet, worn thin to pale soil in places, no single blade readable and nothing the eye stops on
Slot 2 (grass_dry), 4x4 tiles of ground, 390x390 px in the template: the same turf gone dry and strawy, a shade paler and warmer than the green, still even and quiet; it is laid over the green in big soft blobs, so it must sit almost flat against it
Slot 3 (crop), 3x3 tiles of ground, 291x291 px in the template: a worked field of low young grain in rows, the soil showing between them, seen from straight above
Slot 4 (mud), 3x3 tiles of ground, 291x291 px in the template: churned wet earth at a river edge or a gateway, dark, with shallow standing water in the hollows
Slot 5 (dirt), 3x3 tiles of ground, 291x291 px in the template: packed earth of a village lane, dry and pale, a little gravel worked into it
Slot 6 (gravel), 3x3 tiles of ground, 291x291 px in the template: loose grey river shingle, small stones of mixed size, dry
Slot 7 (paving), 2x2 tiles of ground, 192x192 px in the template: laid stone of the village square, blocks of uneven size, the mortar dark between them
Slot 8 (bridge_deck), 2x2 tiles of ground, 192x192 px in the template: weathered timber decking boards laid across a bridge, gapped, nail heads at the joins
Slot 9 (water), 3x3 tiles of ground, 291x291 px in the template: shallow running stream water, the current read as long soft bands of two close blues

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, dithering, checkerboard texture, stippling, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, a border or frame inside a slot, a lit or shaded side, a vignette, a visible seam at a slot edge, one big feature in the middle of a slot, an object, a path, a shoreline, a character, a shadow, noisy high-contrast texture, drawing outside a slot, moving or covering a slot number, changing the size of the image
````

## 3. Afterwards

- [ ] All 9 slots filled to the border, every number still where it was
- [ ] Each slot is FLAT: no lit side, no vignette, no border, no shadow
- [ ] Calm and even: nothing in a slot the eye goes to first
- [ ] Nothing that reads as an edge of the material — no grass rim, no shoreline, no path
- [ ] Tiling test: the top edge of a slot would meet its bottom edge with no join

If one slot fails, reply in the same chat: "Redraw only slot 2 and keep every other slot and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/swatches/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/swatches
```

That writes the 9 seamless swatch(es) listed above. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
