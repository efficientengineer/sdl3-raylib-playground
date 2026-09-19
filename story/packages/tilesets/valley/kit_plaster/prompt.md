# ChatGPT package: valley — the 'plaster' building kit (28 pieces)

## What exists already

- `plaster` kit, 28 pieces at atlas index 384-411

## 1. Start a new chat and attach these files, in this order

1. `story/packages/tilesets/valley/kit_plaster/template.png`
2. `story/sheets/tests/clean_style_test_v1.png`
3. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: the attached template with all 6 numbered blocks filled in. Canvas: 920x1136, exactly the same size and proportions as the template.

Top-down JRPG field art in the clean 16-bit manner: flat luminous fields of colour, big simple shapes, soft painted edges, sparse deliberate detail. Every shadow is a single cool blue-violet tone, never a gradient and never grey.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: TEMPLATE. Redraw this exact image with every numbered slot filled in and everything else left untouched. It is the canvas, not a reference.
Image 2: STYLE reference. STYLE reference — this is our own approved art, so match it closely: the flat luminous colour, the big simple shapes, the soft painted edges, the sparse deliberate detail and the single cool blue-violet shadow tone. Its colours come from the palette image attached last. Do not copy its layout, its objects or its composition, and do not copy any dithering or stipple.
Image 3: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

TEMPLATE RULES: Return the whole template at the same size and proportions as the image I attached. Every white slot border and every slot number stays exactly where it is, the same size and the same place, down to the pixel. Draw only inside the slots. Everything outside a slot is left as it is: the margins, the gutters between the slots, and the background behind the numbers. Nothing crosses a border, nothing leans into a neighbouring slot, nothing is added between the slots. No text, no labels, no captions, no arrows, no colour swatches, no signature.

WHAT THIS IS: a BUILDING KIT — a set of square tiles the game assembles into houses of any size. It is not a picture of a house. Each numbered block is a strip or a grid of 128x128 tiles drawn TOUCHING, with no gap and no line between them, because the game cuts them apart and puts them back together in a different order.

THE STYLE OF THIS KIT: cream lime-plaster walls over a grey fieldstone foundation, dark timber framing on the upper storey, deep red clay pantiles on the roof, dark oak doors and shutters, a warm and well-kept village house

HOW THE GAME ASSEMBLES THEM — a 5-wide, 2-storey house with a 3-row roof:

        column:   0      1      2      3      4
        row 0   roof_tl roof_t roof_t roof_t roof_tr
        row 1   roof_l  roof_m roof_m roof_m roof_r
        row 2   roof_el roof_e roof_e roof_e roof_er
        row 3   wall_up_l wall_up_m window wall_up_m wall_up_r
        row 4   wall_gr_l wall_gr_m door  wall_gr_m wall_gr_r

So these edges must match, exactly, pixel for pixel:
- The RIGHT edge of `roof_tl` must continue into the LEFT edge of `roof_t`; `roof_t`'s own left and right edges must match EACH OTHER, because it repeats; and its right edge must continue into `roof_tr`'s left edge. The same for the `roof_l / roof_m / roof_r` row and the `roof_el / roof_e / roof_er` row.
- The BOTTOM edge of the top roof row must continue into the TOP edge of the middle row, and the middle row's bottom into the eave row's top. The middle row repeats downward, so its own top and bottom edges must match each other.
- Every wall strip works the same way across: `_l` then any number of `_m` then `_r`, and `_m`'s left and right edges must match each other.
- An OPENING replaces a wall middle, so its left and right edges must match the wall middle's edges of the same storey. A door and a shopfront sit in the GROUND storey, a window in either.
- The wall's top edge meets the eave row's bottom edge; the ground storey's bottom edge is where the building meets the ground.

EVERY TILE FILLS ITS CELL EDGE TO EDGE — no margin, no rounded corner, no drop shadow outside the tile — EXCEPT where the shape of the building genuinely leaves sky: the roof corners, the verges, the gable ends and the extras. There the rest of the cell is the flat background colour and nothing else.

BLOCKS:
Block 1 (roof), 3x3 tiles of 128x128 px — the roof, as a 9-slice: the middle column repeats across a wide house and the middle row repeats down a tall one:
    cell (0,0) roof_tl: the roof's top-left corner: the ridge end, with the pitch falling away right and down
    cell (1,0) roof_t: the roof's top edge: the ridge running across, the pitch falling away below it
    cell (2,0) roof_tr: the roof's top-right corner: the ridge end, the pitch falling away left and down
    cell (0,1) roof_l: the roof's left edge: the pitch, with the verge board down the left side
    cell (1,1) roof_m: the middle of the roof: nothing but pitch, and it repeats in both directions
    cell (2,1) roof_r: the roof's right edge: the pitch, with the verge board down the right side
    cell (0,2) roof_el: the roof's bottom-left corner: the eave and the gutter end, the wall starting below
    cell (1,2) roof_e: the roof's bottom edge: the eave overhanging, its shadow on the wall below
    cell (2,2) roof_er: the roof's bottom-right corner: the eave and the gutter end
Block 2 (gable), 2x1 tiles of 128x128 px — the two gable ends, for a roof seen end-on:
    cell (0,0) gable_l: a gable end seen from the left: the triangle of wall under the roof's slope
    cell (1,0) gable_r: a gable end seen from the right: the triangle of wall under the roof's slope
Block 3 (wall_up), 3x1 tiles of 128x128 px — an upper storey: left end, middle, right end:
    cell (0,0) wall_up_l: an upper storey's left end: the corner of the wall, the quoin or the corner post
    cell (1,0) wall_up_m: an upper storey's middle: blank wall, and it repeats across any width
    cell (2,0) wall_up_r: an upper storey's right end: the corner of the wall
Block 4 (wall_gr), 3x1 tiles of 128x128 px — the ground storey, with the foundation along its bottom edge:
    cell (0,0) wall_gr_l: the ground storey's left end: the corner, with the foundation along the bottom edge
    cell (1,0) wall_gr_m: the ground storey's middle: blank wall over the foundation, and it repeats
    cell (2,0) wall_gr_r: the ground storey's right end: the corner, with the foundation along the bottom edge
Block 5 (openings), 6x1 tiles of 128x128 px — each one REPLACES a wall middle, so it must join the wall either side of it exactly:
    cell (0,0) door: a single door in the ground storey's wall, closed, its frame and step drawn
    cell (1,0) door_dl: the left half of a double door, closed
    cell (2,0) door_dr: the right half of a double door, closed, matching the left half exactly
    cell (3,0) window: a window in the wall: frame, glazing bars and a sill
    cell (4,0) window_shut: the same window with its shutters closed over it
    cell (5,0) shopfront: a shop's open front in the ground storey: a wide counter opening under a lintel
Block 6 (extras), 5x1 tiles of 128x128 px — laid over a roof or wall tile, so each one is mostly empty:
    cell (0,0) chimney: a brick or stone chimney stack with its pot, standing on the roof, the rest empty
    cell (1,0) dormer: a small dormer window sitting in the roof pitch, the rest empty
    cell (2,0) sign_bracket: an iron bracket on the wall with a small blank hanging sign, the rest empty
    cell (3,0) lantern: a lantern on a bracket by a door, lit, the rest empty
    cell (4,0) ivy: a patch of ivy growing up a wall, ragged at its edges, the rest empty

RENDERING: Flat luminous colour, drawn from the palette image attached last. Big simple shapes read first; detail is sparse and deliberate, placed where it means something and absent everywhere else. Shading is 2 or 3 flat steps of one colour with one light direction from the upper left, and every shadow is a single cool blue-violet tone. Edges are soft-painted, not hard-aliased, and clean: no dithering anywhere, no checkerboard, no stipple, no noise, no gradient, no texture for its own sake. Warm greens and earths against that cool shadow, with saturated accents on roofs, cloth, water and flowers.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, dithering, checkerboard texture, stippling, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, a gap or a line between the tiles of one block, a drop shadow or a glow outside a tile, a whole house drawn in one block, perspective, a vanishing point, ground or grass under the building, people, drawing outside a block, moving or covering a block number, changing the size of the image
````

## 3. Afterwards

- [ ] All 6 blocks filled, every border and number still exactly where it was
- [ ] The tiles inside a block TOUCH: no gutter, no line, no border between them
- [ ] A middle tile's left and right edges match each other (it has to repeat)
- [ ] The roof's three rows stack: bottom of one continues into the top of the next
- [ ] An opening's left and right edges match the wall middle of its own storey
- [ ] One light direction, upper left, across every piece

If one block fails, reply in the same chat: "Redraw only block 3 and keep every other block and the whole template exactly as it is. <what was wrong>".

Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/tilesets/valley/kit_plaster/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/tilesets/valley/kit_plaster
```

That writes the 28 kit pieces into `story/field/tilesets/valley/atlas.png`. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.
