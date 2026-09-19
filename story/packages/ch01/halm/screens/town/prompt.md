# ChatGPT package: halm / town — painted map

**One chat, 2 messages.** The map first, then the mask, each in its own message in the same chat, so
ChatGPT is tracing a picture it can see. Do not start a new chat for the mask, and do not ask
for the map and the mask in one message — the mask must be traced from the finished map.

## What exists already

- `story/field/screens/halm_town_paint.png` — missing
- `story/field/screens/halm_town_walk.png` — missing
- `story/field/screens/halm_town.screen` — missing

Style blocks used from `story/STYLE.md`: map_header, map_rendering, map_negative (`negative` minus its comic-page complaints). The panel blocks — `layout`, `framing`, `acting`, `character_design`, `dialogue_box`, `sheet_layout` — are deliberately **not** used: this is a top-down field map, not a manga page.

## 1. Start a new chat and attach these files, in this order

1. `story/refs/style.png`
2. `story/field/props/tree_a.png`
3. `story/field/props/tree_b.png`
4. `story/field/props/barrel.png`
5. `story/field/props/cart.png`
6. `story/field/props/fence.png`

- Image 1: STYLE reference. Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.
- Image 2: The tree a as it is drawn in this game. It belongs in this place; keep its design, colours and proportions.
- Image 3: The tree b as it is drawn in this game. It belongs in this place; keep its design, colours and proportions.
- Image 4: The barrel as it is drawn in this game. It belongs in this place; keep its design, colours and proportions.
- Image 5: The cart as it is drawn in this game. It belongs in this place; keep its design, colours and proportions.
- Image 6: The fence as it is drawn in this game. It belongs in this place; keep its design, colours and proportions.

## 2. Message 1 — paste this and get the map (1536x1024)

````
Paint one complete top-down map for a 16-bit JRPG: the whole place the player walks around in, as one finished picture. This is a field map, not a comic page and not a scene — no panels, no borders, no frame, one single image filling the canvas edge to edge.

16-bit Sega Genesis era pixel art, early 1990s JRPG field map, Phantasy Star IV look.

THE PROJECTION, and it is the most important instruction here: the classic top-down oblique those games are drawn in. The ground is seen from straight above, flat, as if the map were laid on a table. Buildings and objects show their roof and their front — their south — face only, and every vertical edge runs straight up the screen. There is NO perspective, NO vanishing point, NO horizon and no sky. Nothing gets smaller further up the picture: one uniform scale from edge to edge, the same size at the top as at the bottom. Nothing leans, nothing is foreshortened, no side or three-quarter faces on anything.

THE LANDMARK, which must be clearly on the map: the lopsided well square, with the guild hall along one side of it.

THE PLACE:
The whole valley town of Halm on one map, seen from straight above, late afternoon, half emptied. It grew round its well, so the middle of the map is a lopsided open square of worn grey paving — wider at its south end, pinching to a lane at its north — with the round stone well a little off centre, its plank roof on two posts and its bucket at the rim, and the paving laid in rings that spread out from it and give way to beaten dirt and creeping grass. The long low guild hall stands on the east side of the square with its deep porch on squat timber posts, its double doors open and a bare plank board fixed to the wall beside them; it is the biggest roof on the map. Plastered houses under red pantile roofs crowd the square and the lanes off it, staggered along the lanes at all different spacings and in clearly different sizes, some set forward to the path and some back behind a yard wall, none of them in a row. Their doors stand open and a shutter or two is off its hinges. Nine lanes leave the square and none of them is straight: they bend, fork, pinch to a gap and widen again, and two of them end in a yard. The grain yard fills the south-east corner behind a dry-laid stone wall with a flat coping, one stretch of it bellied out and patched with newer stone, with the dark open front of the grain shed inside it and a plank bench of grain scales by the door. North-west of the square the carters' yard stands empty: bare rutted earth, one two-wheeled handcart left with its shafts down and loaded above the sides, and nothing else. A stream comes in at the north-east, runs a crooked line south-west across the top of the town under two plank footbridges, and leaves at the west edge. A wide dirt road leaves the west edge of the map toward the valley; a narrower path climbs the north edge toward the shoulder of the hill. Broad valley trees with heavy rounded crowns stand in ones and twos through the town, and the edges of the map dissolve into orchard rows, hedges, sheds, garden walls and open field rather than stopping at a line.

THE SCALE, which everything is drawn to: a person is about 64 pixels tall on this map and a door about 74 pixels tall, at 1536x1024. A house is a few people wide, a well is about one person across, and every path, street and gap a person has to walk through is at least 192 pixels wide — 3 people abreast. Draw no people; the scale is there so the buildings and the gaps between them come out the right size.

THE GROUND A PERSON CAN WALK ON reads at a glance: open ground, paving, dirt and grass, with a clear edge where it stops and a clear line where every object stands on it. Paths connect to each other and to the places they lead; nothing walkable is left as an island that cannot be reached.

THE WAYS OUT run off the edge of the map as roads or paths: the left edge, the way to west road; the top edge, the way to hart yard. Each one reaches the very edge of the canvas, at least 192 pixels wide, not stopping short of it and not blocked by anything.

HOW THE PLACE IS SHAPED — people grew this, they did not lay it out. It spread from one reason, so the oldest and densest part is round that and it thins toward the edges. Lanes bend, fork, pinch and widen, and some end in a yard. Nothing is in a row: buildings are staggered along the lanes at irregular spacings and in clearly different sizes and shapes, set forward and back from the path rather than lined up, so no two edges run together for long. No grid, no city blocks, no repeated spacing, no mirror symmetry — those belong to the ancients and this is not one of their places. The map does not stop at a clean line: the edges dissolve into orchard, hedge, wall, shed, rock and field.

NO PEOPLE, no characters, no animals: the game draws those on top as moving sprites, and a painted one would stand still forever. No text, no letters, no numbers, no writing on signs or boards, no labels, no watermark, no user interface, no map legend, no compass, no frame, no border, no vignette and no letterboxing.

RENDERING: Low resolution pixel art upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors. Checkerboard dithering only on large flat areas of ground, water and roof, never as noisy texture on small objects. Thin 1-pixel dark outlines around every object that stands on the ground. Muted earthy ground tones — grass, dirt, stone, timber — with saturated accents on roofs, doors, awnings, cloth and water. Flat shading with 2-3 tones per color and one consistent light direction across the whole map.

SIZE: one landscape image, 1536x1024, painted edge to edge, the map filling the whole frame like a piece cut out of a larger world.

AVOID: perspective, vanishing point, horizon, sky, isometric view, three-quarter view, side view, things shrinking with distance, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, high resolution detail, lens flare, glow effects, mixed pixel sizes, grid layout, buildings in rows, repeated spacing, people, characters, animals, text, letters, numbers, labels, captions, watermark, signature, user interface, map legend, compass, frame, border, vignette, letterboxing, empty unpainted areas, writing of any kind on signs or boards
````

Save it in **this folder** as `returned.png` (the whole path is `story/packages/ch01/halm/screens/town/returned.png`). A .jpg or .webp works too; the tool converts it.

## 3. Message 2 — paste this and get the walkable mask

````
Make a WALKABLE MASK of the map you just generated. This is a technical image for a game engine, not an illustration.

Output: the exact same image size and framing as the map, aligned pixel for pixel, so that it can be laid over the map and every edge lines up. Do not re-imagine, re-compose, crop, zoom or shift anything. Trace the map.

Use exactly TWO flat colours and nothing else:
- Pure green #00FF00 = ground a person could stand or walk on.
- Pure black #000000 = everything else.

No outlines, no line art, no shading, no gradients, no texture, no anti-aliasing, no third colour, no text. Every pixel is either pure green or pure black. Edges are hard.

GREEN (walkable): open ground, grass, dirt, paving, streets, paths and yards; steps and stairs (paint a whole flight as one solid green shape, not tread by tread); ramps; bridges and plank walkways. Walkable areas that connect on the map must connect in the mask: a path joins the square it runs into, a bridge joins the bank at both ends. A doorway a person can enter gets a small green notch at its threshold, in the wall, the width of the door.

BLACK (not walkable): every object exactly as it is drawn, its whole drawn area — buildings and their roofs, walls, fences, hedges, gates, wells, troughs, barrels, crates, carts, market stalls, signposts, statues, trees, bushes, crops, rocks, cliffs, water and streams. If it is drawn on the map and it is not ground, it is black, over the whole shape the painting gives it.

Keep paths at least as wide as they are on the map. When unsure whether a person could walk there, paint it black.
````

Save it in **this folder** as `returned_walk.png`.

## 4. Check before cutting

- [ ] Top-down: the ground is seen from straight above, objects show only roof and front face
- [ ] No perspective, no vanishing point, no horizon, no sky, nothing shrinking with distance
- [ ] One scale everywhere: a door about 74 px tall, paths 192 px wide
- [ ] Lanes bend; buildings are staggered, different sizes, never in rows; no grid
- [ ] Every way out runs off its edge of the frame (left, top)
- [ ] No people, no animals, no text, no interface, no border
- [ ] The mask is the same size and framing as the map, pixel for pixel
- [ ] The mask is two flat colours only — no outlines, no shading, no third colour
- [ ] Green is only ground: every object, roof, tree, wall and stretch of water is black
- [ ] Every enterable door has a small green notch at its threshold

If a mask drifts, reply in the same chat: "Trace the map exactly — same size and framing,
two flat colours only, no outlines."

## 5. Then cut

```
./story_prompt.py ingest story/packages/ch01/halm/screens/town
```

That fits everything to one size and writes `story/field/screens/halm_town_paint.png`, `story/field/screens/halm_town_walk.png`, `story/field/screens/halm_town.screen` — the
size, the nav polygons, the doors, the exits, the spawn and the walker height, all in screen
pixels, which is what the engine loads.
Add `--debug` to also write `story/field/screens/halm_town_debug.png`: the map with the nav polygons outlined and
the doors and exits marked, so what the tool understood can be seen at a glance.
