# ChatGPT package: halm / square — painted screen

**One chat, three messages.** The painting first; then each mask, in its own message, in the
same chat, so ChatGPT is tracing an image it can see. Do not start a new chat for a mask, and
do not ask for both masks at once — one mask with three classes in it is what failed: the
model invented a class of its own and drew line art over the top.

## What exists already

- painting `story/field/screens/halm_square_paint.png` — missing
- walkable mask `story/field/screens/halm_square_walk.png` — missing
- foreground mask `story/field/screens/halm_square_fg.png` — missing
- base map `story/field/screens/halm_square_base.png` — missing
- the engine's file `story/field/screens/halm_square.screen` — missing

Style blocks used from `story/STYLE.md`: header, rendering, negative (`negative` minus its comic-page complaints). The panel blocks — `layout`, `framing`, `acting`, `character_design`, `dialogue_box`, `sheet_layout` — are deliberately **not** used: this is a field background, not a manga page, so there are no panels, no borders and nobody acting.

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

## 2. Message one — paste this and get the painting (1536x1024)

````
Paint one complete game background: the whole screen a player walks around in, as a finished picture. This is a field background, not a comic page — no panels, no borders, no frame, one single image filling the canvas edge to edge.

16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.

THE LANDMARK, which must be clearly in frame: the lopsided well square, and the road that climbs round the shoulder of the hill.

THE PLACE:
The lopsided well square at the middle of Halm, seen from above and to one side, late afternoon, the valley town half emptied. The square is not square: an irregular open space of worn grey paving that spreads wider at the near end and pinches to a lane at the far one, with beaten dirt showing through where feet have crossed it and grass creeping in at the edges. The round stone well stands a little off centre with its plank roof on two posts and its bucket hooked at the rim, and the paving fans out from it in rings that the town grew around. The long low guild hall faces the square from the right, its deep porch on squat timber posts and its double doors standing open, a bare plank board fixed to the wall beside them. Plastered houses under low red pantile roofs crowd the left and the far side, no two of them set at the same angle, their doors open and a shutter or two gone from the hinges; a two-wheeled handcart stands loaded above the sides with a rolled mattress and tied bundles where the lane leaves the far corner. A dry-laid stone wall with a flat coping closes the grain yard at the near right, one stretch of it bellied out and patched with newer stone, with the dark open front of the grain shed behind it. A broad street leaves the left edge of the frame and bends out of sight between the houses; a narrower lane climbs the far edge toward the shoulder of the hill, past a broad valley tree with a heavy rounded crown. Nothing is straight for longer than two houses, the gaps between the buildings are all different, and the town dissolves at the edges of the frame into orchard, hedge and shed rather than stopping at a line.

CAMERA: one fixed three-quarter view from above, looking down at about 50 degrees, on a long lens so the perspective is gentle and near and far things stay close to the same size. The ground fills most of the frame: if any horizon shows at all it sits in the top fifth. Everything in the picture is seen from this one camera — roofs from above and a little to the side, walls square on to it, the ground running away from the bottom of the frame toward the top.

THE GROUND A PERSON CAN WALK ON is the most important thing in the picture and must read at a glance: one continuous, clearly bounded surface of paving, dirt or boards, unbroken where it is meant to be walked, with a visible edge where it stops. Do not scatter clutter across it, do not break it into disconnected islands, and do not bury its edge in shadow. Everything standing on it — a wall, a tree, a cart, a post — has its own clear silhouette against it and its own clear line where it meets the ground.

THE WAYS OUT reach the edge of the picture as visible walkable paths: the left edge, a way out toward west road; the top edge, a way out toward hart yard. Each one runs off that edge of the frame — a road, a lane, a track, a stair — wide enough to walk, touching the very edge of the canvas, not stopping short of it and not hidden behind anything.

HOW THE PLACE IS SHAPED — people grew this, they did not lay it out: nothing straight for longer than two or three buildings, streets that bend, fork, pinch and widen, no two neighbouring buildings set at the same angle, gaps and spacings all different, paths that curve because something is in the way. The open ground is a rough, lopsided shape, not a rectangle. The edges of the picture dissolve into trees, hedges, sheds, walls and fields rather than stopping at a clean line.

NO PEOPLE, no characters, no animals: the game draws those on top as moving sprites, and a painted one would stand still forever. No text, no letters, no numbers, no writing on signs or boards, no labels, no watermark, no user interface, no frame, no border, no vignette and no letterboxing.

RENDERING: Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels on a single consistent pixel grid, limited palette of about 32 colors, checkerboard dithering for skies, walls, and shadows only, never as noisy texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base tones with saturated accents on clothing and hair.

SIZE: one landscape image, 1536x1024, painted edge to edge with nothing left blank.

AVOID: western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture, smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render, photorealistic, modern anime, chibi, high resolution detail, lens flare, glow effects, mixed pixel sizes, text, letters, captions, speech bubbles, watermark, signature, empty unpainted areas, a comic panel layout, people or animals, text or writing of any kind, a user interface, a border or a vignette
````

Save it in **this folder** as `returned.png` (the whole path is `story/packages/ch01/halm/screens/square/returned.png`).
A .jpg or .webp works too; the tool converts it.

## 3. Message two — in the same chat, paste this and get the walkable mask

````
Make a WALKABLE MASK of the image you just generated. This is a technical image for a game engine, not an illustration.

Output: the exact same image size, framing and camera as the painting, aligned pixel for pixel, so that it can be laid over the painting and every edge lines up. Do not re-imagine, re-compose, crop, zoom or shift anything. Trace the painting.

Use exactly TWO flat colours and nothing else:
- Pure green #00FF00 = ground a person could stand or walk on.
- Pure black #000000 = everything else.

No outlines, no line art, no shading, no gradients, no texture, no anti-aliasing, no third colour, no text. Every pixel is either pure green or pure black. Edges are hard.

GREEN (walkable): paved plaza and street surfaces, stair treads (paint a whole staircase as one solid green shape, not stripe by stripe), ramps, bridges and dock planks a person could walk along, doorway and archway floors up to the threshold. Walkable areas that connect in the painting must connect in the mask: stairs join the plaza they lead to, a dock joins the quay.

BLACK (not walkable): buildings, roofs, awnings, tops of walls and balustrades, water, boats, cliffs, rocks, trees, bushes, planters, barrels, crates, benches, market stalls, lamp posts, statues, fountains and their basins, banners, the sky. If an object stands on the ground, the footprint it covers is black — cut its base out of the green, but do NOT cut out the parts of it that only overlap the ground visually (a lamp post's footprint is a small dot at its base, not the whole post).

Keep passages at least as wide as they are in the painting. When unsure whether a person could walk there, paint it black.
````

Save it in this folder as `returned_walk.png`.

## 4. Message three — still the same chat, paste this and get the foreground mask

````
Now make a FOREGROUND MASK of the same painting, same rules: exact same size, framing and camera, aligned pixel for pixel, traced from the painting.

Use exactly TWO flat colours: pure white #FFFFFF and pure black #000000. No outlines, no shading, no gradients, no anti-aliasing, no text.

WHITE = every object that rises up from the ground and could hide a person walking behind it. Paint the object's whole visible silhouette in white, exactly matching its outline in the painting: lamp posts, the statue and fountain, trees and bushes, planters, barrels, crates, benches, market stalls and their awnings, banners, free-standing walls and balustrades, gate arches, mooring posts, and the front faces of any wall or building that a walkable area passes behind.

BLACK = the ground itself (plaza, stairs, docks), water, sky, and anything far away that no walkable area passes behind.

Where two white objects touch or overlap, leave a 2-pixel black gap between them so they stay separate shapes. Each object should be one solid white shape with no holes unless the painting really shows a see-through gap (an open archway is a hole; a window is not).
````

Save it in this folder as `returned_fg.png`.

## 5. Check all three before cutting

- [ ] The painting is one field background: no panels, no border, painted edge to edge
- [ ] The ground a person walks on reads at a glance and is continuous
- [ ] Every way out reaches the edge of the frame as a visible path (left, top)
- [ ] No people, no animals, no text, no interface, no vignette
- [ ] Both masks are the same size and framing as the painting, pixel for pixel
- [ ] Both masks are two flat colours only — no outlines, no shading, no third colour
- [ ] The walkable green is one connected shape wherever the painting connects, and is not
      painted on roofs, windows, arches on the skyline or anywhere the player cannot reach
- [ ] Every standing object is white in the foreground mask, down to where it meets the ground

The tool forgives a lot of this: it opens the walkable mask to kill traced outlines, keeps
only the ground the player can actually reach, and reads the foreground mask per pixel rather
than per object. What it cannot forgive is a mask at a different size or framing from the
painting. If one drifts, reply in the same chat: "Trace the painting exactly — same size and
framing, two flat colours only, no outlines."

## 6. Then cut

```
./story_prompt.py ingest story/packages/ch01/halm/screens/square
```

That fits all three to one size and writes `story/field/screens/halm_square_paint.png`, `story/field/screens/halm_square_walk.png`, `story/field/screens/halm_square_fg.png`, the base map
`story/field/screens/halm_square_base.png` and `story/field/screens/halm_square.screen` — the size, the nav polygons, the base map, the exits, the spawn
and the walker scale, all in screen pixels, which is what the engine loads.
Add `--debug` to also write `story/field/screens/halm_square_debug.png`: the painting with the nav polygons outlined
and the base map in false colour, so what the tool understood can be seen at a glance.
