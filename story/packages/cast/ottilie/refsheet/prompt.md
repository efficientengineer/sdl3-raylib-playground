# ChatGPT package: Ottilie — reference sheet

## What exists already

- reference sheet `story/refs/lyra.png` — **exists**
- dialogue portrait `story/portraits/lyra.png` — **exists**

## 1. Start a new chat and attach these files, in this order

1. `story/refs/style.png`

## 2. Paste this prompt exactly

````
Create ONE image: a character reference sheet for Lyra. Canvas: landscape, 1536x1024.
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

ATTACHED REFERENCE IMAGES, in the order I attached them:
Image 1: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

SHEET: A character reference sheet on a solid pure black background, three separate white-bordered panels in one row. Left panel: full-body standing pose, head to boots, three-quarter view. Middle panel: head-and-shoulders portrait in three-quarter view against a flat neutral mid-grey background. Right panel: the same head in strict profile. The character is identical in all three panels: same face, hair, outfit, and colors. No text, no labels, no color swatches.

CHARACTER: Lyra, a tall, slender young woman with very long straight pale-gold hair, long bangs parted in the center, a thin gold circlet with a small red gem on her forehead, calm grey eyes, a white high-collared long coat with gold trim and wide gold-edged shoulder pieces over a dark navy bodysuit, a short white half-cape, a gold sun medallion on her chest, long white gloves, white heeled boots, a slim silver mace at her hip.

CHARACTER DESIGN: 1990s Japanese anime and manga character design, like the cast of a 1993 sci-fantasy JRPG or OVA: youthful heroic young adults, large expressive eyes with simple highlights, small noses and pointed chins, big layered spiky hair with hard-edged shine bands, slim necks, clean readable silhouettes. Sci-fantasy costumes: high collars, bodysuits, long coats, half-capes, oversized shoulder plates, headbands, sashes. Simple clean shapes and large flat areas of color. Clean dark outlines, flat cel shading with only 2-3 tones per color, no gradients.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, different outfits or colors between panels, scenery, props not in the description, text, letters, labels, color swatches, watermark, signature
````

## 3. Afterwards

- Regenerate until you like the design. This image becomes Lyra's look in every scene,
  the head-and-shoulders portrait beside the dialogue box, and the walk sprite's design.
Save ChatGPT's image into **this folder** as `returned.png` — the whole path is
`story/packages/cast/ottilie/refsheet/returned.png`. A .jpg or .webp works too; the tool converts it.

Then cut it up, from the repository root:

```
./story_prompt.py ingest story/packages/cast/ottilie/refsheet
```

That writes `story/refs/lyra.png` and the dialogue-box portrait cut out of it. Running `./story_prompt.py ingest` with no path does every package in
`story/packages/` that has a new image waiting. The returned file is never deleted, so a
bad cut can always be redone after a fix.

If the image needs another go, reply in the same chat and save the new one over `returned.png`.

- If the design differs from the `look` line in characters.md, update the `look` line to match.
