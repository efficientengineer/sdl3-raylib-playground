# ChatGPT package: Garbe — reference sheet

## What exists already

- reference sheet `story/refs/garbe.png` — **exists**
- dialogue portrait `story/portraits/garbe.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/refs/style.png`
2. `story/palette/master_swatch.png`

## 2. Paste this prompt exactly

````
Create ONE image: a character reference sheet for Garbe. Canvas: landscape, 1536x1024.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.
Image 2: the game's COLOUR PALETTE, one material ramp a row: use only these colours.

SHEET: A character reference sheet on a solid pure black background, three separate white-bordered panels in one row. Left panel: full-body standing pose, head to boots, three-quarter view. Middle panel: head-and-shoulders portrait in three-quarter view against a flat neutral mid-grey background. Right panel: the same head in strict profile. The character is identical in all three panels: same face, hair, outfit, and colors. The character carries nothing and holds nothing: no weapon of any kind anywhere on the sheet, including the full-body panel, and no sword, knife, staff, spear, bow, axe, gun, shield, tool, bag or prop slung, sheathed or strapped to them. Hands empty and visible. No text, no labels, no color swatches.

CHARACTER: Garbe, a wide-shouldered woman in her forties with weather-reddened brown skin and pale creases at the eyes, dark hair scraped back hard into a short tail, heavy straight brows, a long sleeveless coat of oiled brown canvas over a high-necked cream shirt with the sleeves pushed past the elbow, a wide leather belt with a tally stick and a folded knife, thick green trousers, mud to the knee, laced boots, a coil of rope over one shoulder.

CHARACTER DESIGN: 1990s Japanese anime and manga character design, like the cast of a 1993 sci-fantasy JRPG or OVA: youthful heroic young adults, large expressive eyes with simple highlights, small noses and pointed chins, big layered spiky hair with hard-edged shine bands, slim necks, clean readable silhouettes. Sci-fantasy costumes: high collars, bodysuits, long coats, half-capes, headbands, sashes, work clothes and travelling clothes. Armour only when the character's description asks for it. Simple clean shapes and large flat areas of color. Clean dark outlines, flat cel shading with only 2-3 tones per color, no gradients.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, different outfits or colors between panels, scenery, props not in the description, weapons, swords, knives, staves, spears, bows, axes, guns, shields, sheaths, scabbards, holding or carrying anything, text, letters, labels, color swatches, watermark, signature
````

## 3. Afterwards

- Regenerate until you like the design. This image becomes Garbe's look in every scene,
  the head-and-shoulders portrait beside the dialogue box, and the walk sprite's design.
Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/cast/garbe/refsheet/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/cast/garbe/refsheet
```

That writes `story/refs/garbe.png` and the dialogue-box portrait cut out of it. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.

- If the design differs from the `look` line in characters.md, update the `look` line to match.
