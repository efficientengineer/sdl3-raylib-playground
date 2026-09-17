# STYLE.md — locked art direction for scene images

Target look: 16-bit Sega Genesis manga cutscenes in the manner of Phantasy Star IV.
This file is the single source of truth. `story_prompt.py` parses it, so the
headings, the `- key: value` lines, and the fenced blocks are load-bearing.

## Rules for anyone (human or model) writing image prompts

1. **Never hand-write a final image prompt.** Write a scene file in `story/scenes/`
   and run `./story_prompt.py sheet <scene>` (ChatGPT shot sheet, the normal path) or
   `./story_prompt.py build <scene>` (single finished page, for generators without
   reference images). The tool assembles the prompt from the locked blocks below, and
   refuses scenes that break the composition rules.
2. **Never paraphrase the locked blocks or a character's `look`.** They are inserted
   verbatim so every image matches. To change the style, edit this file, not a prompt.
3. **A scene file only decides content and camera:** which shot from the menu each
   panel uses, and what is in it. It never restates style, palette, or rendering.
4. **No readable text in images.** Image models mangle pixel fonts. Dialogue lives in
   the scene file and is drawn by the game. The image gets an empty dialogue box at most.
5. If the tool rejects a scene, fix the scene. Do not bypass the tool.

## Composition rules (enforced by `story_prompt.py`)

- **R1 panel count** — a scene is 1 to 3 **pages** of 2 to 4 panels, at most 8 panels in all. A line of
  `---` in `## Panels` starts a new page: the game clears the screen and builds the next page from
  empty, the way Phantasy Star IV does. One shot sheet holds the whole scene. Aim for 6 to 8 shots in
  a scene that matters; 3 or 4 is fine for a short beat. R3, R4, and R5 apply to each page on its own.
- **R2 menu only** — every panel uses a shot id from the shot menu below.
- **R3 alternate scale** — neighbouring panels never share a scale. A close-up sits
  next to a wide or medium shot, never two medium shots side by side.
- **R4 vary shape** — 2 panels need 2 different shapes; 3 or 4 panels need at least 3.
  One wide, one tall, one small square reads as this style. A grid of equal panels does not.
- **R5 anchor and punch** — with 3 or more panels, at least one panel is an anchor
  (scale wide, full, or medium) and at least one is a punch (scale close, extreme, or insert).
- **R6 known cast** — every character named in a scene exists in `characters.md`, and is
  listed in the scene's `characters:` line. Their `look` is inserted verbatim.
- **R7 short panels** — a panel description is at most 40 words. It says who, doing what,
  where they look. It does not describe style.
- **R8 no style leakage** — panel descriptions may not contain style or rendering words
  (gradient, photorealistic, 3D, blur, glow, painterly, realistic, HD, 4k, smooth).
- **R9 no text** — panel descriptions may not ask for written words, signs, captions,
  or speech bubbles.
- **R10 staging** — every panel scene has a `- staging:` line fixing screen direction for the whole
  scene: who stands on which side, facing which way, and where the thing they react to is. Every
  panel obeys it (the 180-degree rule), so a character who faces right in one panel never faces
  left in the next without a reason.
- **R11 acting** — every cast member named in a panel gets an acting line under that panel:
  `- Name: <where they look>, <expression>, <body>`. It must say where the eyes point (at a named
  target or a screen direction) and what the face is doing. "Neutral" is allowed only when written.
  Characters reacting to the same thing look at the same thing. Max 25 words per line.

## Review checklist (before accepting a generated sheet)

`sheet` prints a per-scene version of this in the ChatGPT package, with a ready-made correction to
paste back. Whoever receives a generated sheet (the owner, or a model shown the image) checks, per panel:

1. **Gaze** — is every character looking where their acting line says? Do characters reacting to the
   same thing look at the same spot? This is the most common failure.
2. **Expression** — does each face show the stated emotion, or the reference sheet's neutral face?
3. **Screen direction** — does the panel obey the scene's staging line?
4. **Design** — hair, outfit, colors, and distinguishing marks match the reference.
5. **Hands and props** — right number of fingers, weapon on the correct side, nothing invented.
6. **Sheet hygiene** — panels separate on pure black, right count and proportions, no text or labels.

Reject on 1-3 even when the art is beautiful: a wrong eyeline breaks the scene. Ask ChatGPT to redraw
only the failing panel and keep the rest; it usually can.

## Locked style blocks

### header

```
16-bit Sega Genesis era pixel art, early 1990s JRPG manga cutscene style.
```

### layout

```
A comic page built from rectangular panels of clearly different sizes and
aspect ratios, arranged asymmetrically on a solid black background with
generous empty black space. The panels cover roughly half to two thirds of
the frame, never tiled edge to edge, never an even grid. Thin white panel
borders with a 1-pixel dark inner line. Panels overlap slightly, smaller
panels layered on top of the corners of larger ones. Each panel is a single
static camera shot.
```

### framing

```
Tight cinematic crops, figures cut off by the panel edge at the waist,
shoulders, or top of the head. Characters in three-quarter view or profile,
rarely facing the camera straight on. Eyelines follow each panel's acting
notes exactly. Shallow staging: figures placed on one or two flat planes,
a foreground figure overlapping a background figure, flat backdrop with no
vanishing point. Backgrounds simplified to a few flat shapes, or replaced
entirely by a flat color, dithered tone, or speed lines in emotional moments.
```

### acting

```
ACTING IS THE POINT OF EVERY PANEL. Read the situation first: each panel is a
moment in that story, not a character catalogue pose. For every character, the
head and both eyes point at what the acting note says they are looking at, and
two characters reacting to the same thing look at the same spot. Obey the
staging: screen direction stays the same in every panel. Faces show the stated
emotion in 1990s anime shorthand, pushed far enough to read at small size:
anger is brows pulled down into a V, narrowed eyes, gritted teeth; fear is
wide eyes with small irises, raised brows, parted lips, a sweat drop; alarm is
a sharp turn of the head, open mouth, hair swinging; resolve is a set jaw, a
level stare, brows low; sorrow is lowered eyes, brows raised at the centre,
closed mouth; suspicion is one narrowed eye and a sidelong look. Bodies act
too: leaning toward or away from the threat, a hand tightening on a weapon,
shoulders raised. Never draw the calm neutral face from a reference sheet
unless the acting note says calm.
```

### character_design

```
1990s Japanese anime and manga character design, like the cast of a 1993
sci-fantasy JRPG or OVA: youthful heroic young adults, large expressive eyes
with simple highlights, small noses and pointed chins, big layered spiky hair
with hard-edged shine bands, slim necks, clean readable silhouettes.
Sci-fantasy costumes: high collars, bodysuits, long coats, half-capes,
oversized shoulder plates, headbands, sashes. Simple clean shapes and large
flat areas of color. Clean dark outlines, flat cel shading with only 2-3 tones
per color, no gradients.
```

### rendering

```
Low resolution 320x224 upscaled with nearest-neighbor, crisp visible pixels
on a single consistent pixel grid, limited palette of about 32 colors,
checkerboard dithering for skies, walls, and shadows only, never as noisy
texture on skin, hair, or cloth. Thin 1-pixel outlines. Muted earthy base
tones with saturated accents on clothing and hair.
```

### dialogue_box

```
Across the bottom, an empty dark blue dialogue box with a beveled grey
border and a small white down-arrow in its lower right corner. The box
contains no text.
```

### sheet_layout

```
This is a production shot sheet, not a finished comic page: the panels will be
cut apart afterwards. Solid pure black background (#000000). Every panel is a
separate rectangle with a thin white border and a 1-pixel dark inner line.
Panels never overlap, never touch each other, and never touch the edge of the
image: no panel covers any part of another, not even a corner, and all four
corners and all four border lines of every panel are fully visible. If space
is tight, draw the panels smaller rather than closer. Leave a gutter of pure black at least 5 percent of the image width
between neighbouring panels and around the outside. The panels have clearly different sizes and proportions and sit at the stated
positions; this is not an even grid and the panels do not fill the image.
Nothing is drawn in the gutters: no
numbers, labels, captions, arrows, or decorations. Each panel is a complete,
self-contained picture of a single static camera shot.
```

### sheet_avoid

```
overlapping panels, panels touching, panels bleeding off the image edge,
panel numbers, labels, gutters that are not pure black, rounded panel corners,
a single merged illustration, a dialogue box
```

### refsheet

```
A character reference sheet on a solid pure black background, three separate
white-bordered panels in one row. Left panel: full-body standing pose, head to
boots, three-quarter view. Middle panel: head-and-shoulders portrait in
three-quarter view against a flat neutral mid-grey background. Right panel: the
same head in strict profile. The character is identical in all three panels:
same face, hair, outfit, and colors. No text, no labels, no color swatches.
```

### refsheet_avoid

```
western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture,
smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render,
photorealistic, modern anime, chibi, high resolution detail, lens flare,
glow effects, mixed pixel sizes, different outfits or colors between panels,
scenery, props not in the description, text, letters, labels, color swatches,
watermark, signature
```

### negative

```
western fantasy art, tabletop RPG illustration, gritty realism, realistic anatomy, bulging muscles, wrinkles, detailed beards, noisy texture,
smooth gradients, anti-aliasing, blur, soft shading, painterly, 3D render,
photorealistic, modern anime, chibi, high resolution detail, lens flare,
glow effects, mixed pixel sizes, even panel grid, panels filling the whole
frame, deep perspective, centered full-figure composition, text, letters,
captions, speech bubbles, watermark, signature
```

## Panel shapes

- wide: a wide horizontal panel
- tall: a tall narrow vertical panel
- square: a small square panel overlapping the corner of a larger panel
- slit: a very wide, very short letterbox panel

## Sheet panel shapes

Used by `sheet` mode, where panels must not overlap. The ratios are width:height.

- wide: a wide horizontal panel, exactly twice as wide as it is tall (2:1)
- tall: a tall narrow vertical panel, exactly twice as tall as it is wide (1:2)
- square: a small perfectly square panel (1:1), the smallest panel on the sheet
- slit: a very wide, very short letterbox strip, four times as wide as it is tall (4:1)

## Reference images

Files to attach in ChatGPT. A missing file is skipped with a warning and the text
description is used alone. Character references are set per character in
`characters.md` with a `- ref:` line; make them with `./story_prompt.py refsheet <Name>`.

- style: story/refs/style.png
- style_note: Match this image's pixel art rendering, limited palette, dithering, outline weight, and panel border style. Do not copy its characters, setting, or composition.

## Shot menu

### two_shot
- shape: wide
- scale: medium
- use: default conversation or party shot
- prompt: Medium two-shot at eye level. Two or more characters from the waist or knees up, the nearer one overlapping the other, cropped by the panel edge.

### portrait_inset
- shape: square
- scale: close
- use: marks who is speaking or reacting
- prompt: Head-and-shoulders portrait in three-quarter view, cropped at the top of the hair. Background: one flat dark color taken from this scene's own setting and lighting, no scenery, not a color copied from a reference image.

### eyes_slit
- shape: slit
- scale: extreme
- use: shock, anger, resolve
- prompt: Extreme close-up showing only the eyes and brow, filling the panel from edge to edge.

### establishing_tall
- shape: tall
- scale: wide
- use: where we are; no characters
- prompt: Establishing shot of architecture or landscape from a slight low angle, open space or darkness filling the top half, no characters.

### establishing_wide
- shape: wide
- scale: wide
- use: where we are, with tiny figures for scale
- prompt: Wide establishing shot of the location at eye level, any figures small and seen from behind, flat layered backdrop.

### full_body_reveal
- shape: tall
- scale: full
- use: introductions, villains, monsters
- prompt: Single standing figure from head to boots at a low angle, looming, simple dark or flat background.

### low_angle_menace
- shape: tall
- scale: close
- use: threat, authority
- prompt: Camera below chin level looking up at the face and shoulders, face partly in shadow, dark or abstract background.

### high_angle_down
- shape: wide
- scale: medium
- use: vulnerability, defeat, discovery on the floor
- prompt: Camera looking down from above on a kneeling, fallen, or crouching character, floor filling most of the panel.

### over_shoulder
- shape: wide
- scale: medium
- use: confrontation, facing something
- prompt: Over-the-shoulder shot. Dark back of a head and shoulder in the foreground at one edge, the facing character or object at mid-distance.

### profile_flat
- shape: square
- scale: close
- use: quiet or sad beats
- prompt: Strict side-view profile of one face. Background: one flat dark color or dithered tone taken from this scene's own setting and lighting, not a color copied from a reference image.

### impact
- shape: wide
- scale: full
- use: attacks, traps firing, sudden motion
- prompt: Figure mid-action against radial speed lines or a solid bright color, slight camera tilt, no environment.

### object_insert
- shape: square
- scale: insert
- use: clues, items, mechanisms, hands
- prompt: Tightly cropped insert of a single object or a hand holding it, nothing else in the panel, flat dark background.
