# FIELD — walking around the world (2.5D, Octopath style)

Owner's decision (2026-09-18): the world is **2.5D like Octopath Traveler**: simple 3D geometry
(ground, walls, heights) seen from a fixed-angle perspective camera, populated with pixel-art
**sprites**. Big surfaces (ground, floors, walls, cliffs, water) are **tiles**; everything else
(houses, trees, wells, carts, fences, signs, people, monsters) is a **sprite** sliced from a ChatGPT
sheet, because sprites allow far more interesting shapes than tiles and compress well. Art comes from
ChatGPT sheets through `story_prompt.py`, sliced like the panels are.

This file is the contract between the engine and the art pipeline. Both sides read it; neither
changes it without the orchestrator.

## Two work streams

- **Engine** (`src/field.h`, `src/field.cpp`, hooks in `src/star_logic.cpp`, `CMakeLists.txt`,
  `fast_reload.sh`): renderer, map loader, movement, camera, interaction, hot reload, dev controls.
  Ships with **procedural placeholder art** so the world is walkable before any sheet exists.
- **Pipeline** (`story_prompt.py`, `story/field/`): `tiles`, `props`, `walker` prompt commands, slicing
  into `story/field/tiles/`, `story/field/props/`, `story/field/walkers/`, and the manifest.

## Rendering

- The game lib already issues raw GL ES 3.0 calls (textures in `star_logic.cpp`). The host clears the
  screen **after** `tick` and then draws ImGui, so the field renders during `tick` into an **offscreen
  FBO** (colour + depth renderbuffer) and the FBO texture is drawn into the ImGui background draw list
  as one full-screen image, nearest-filtered. UI (joystick, dialogue box, Dev) stays ImGui on top.
- Internal resolution: **640x360** (landscape). Letterbox/pillarbox to the window with integer or
  nearest scaling; `dpi_scale` is not involved. This is what makes it look like pixel art.
- Camera: perspective, per-map **camera zones** (`## cameras`, rectangles in map units, the zone the
  player stands in wins, blends over ~0.4 s). Fields must vary (owner): not always a scrolling
  side-on view — some areas push movement into the screen with the character shrinking into the
  distance like FF7/FF8 field screens. Modes: `follow yaw pitch fov dist height` (Octopath default:
  pitch ~50°, FOV ~30°, small lag); `fixed cx cy cz tx ty tz fov [pan]` (camera stays put, FF7
  style); `rail ax ay az bx by bz tx ty tz fov` (slides along a segment with the player's progress,
  looking at T). The joystick is screen-relative in every mode, so controls never invert. Dev
  sliders edit the current zone live and "print zone" logs the map line to paste back.
- Geometry generated from the map at load: one floor quad per cell at its height, wall quads on every
  height discontinuity (textured with the wall tile, repeated per height step), water cells as floor
  quads with a scrolling UV. Vertex colours bake the light: faces away from the sun (sun from +X, -Z)
  darker, each height step slightly brighter, cliff bases darker. **No realtime lighting** (CLAUDE.md).
- Sprites are **billboards**: vertical quads standing on the cell floor, rotated to face the camera
  around Y only, depth-tested with `discard` on alpha < 0.5 (no blend sorting). Walkers (player, NPCs)
  are billboards with animation frames. Props may be `wide` (a house spans 3x2 cells: the sprite's
  base sits on the front row and the cells behind are blocked).
- Post-process slot: the FBO is drawn through one fullscreen pass so a tilt-shift blur (top and
  bottom bands) can be added later without restructuring. Not required in the first build.
- Shaders are inline GLSL ES 3.00 strings in `field.cpp` (Android is the only target).

## Map format — `story/field/maps/<name>.map`

Plain text, one cell per character, editable by hand and by agents. Sections start with `## `.

```
## meta
name: halm
size: 40 30          # width height, cells
spawn: 20 26 N       # x y facing
ground: grass        # default ground tile id
wall: cliff          # default wall tile id
sun: 1 -1            # optional

## height             # one row per line, one char per cell: 0-9 a-z = height in half-steps
## ground             # one char per cell mapping to tile ids via the `## tiles` legend; '.' = default
## tiles              # legend: <char> <tile-id>       e.g.  d dirt / s stone / w water / p plank
## props              # x y prop-id [wide WxD] [solid|walk]        e.g.  5 7 house_a wide 3x2 solid
## npcs               # x y walker-id facing name scene-id         e.g.  12 9 clerk S Clerk 0130_the_board
## exits              # x y w h map-name spawn-x spawn-y facing
## triggers           # x y w h kind arg                 kinds: message <text> | scene <scene-id> | zone <encounter-table>
```

**Movement is a navmesh, not colliders** (owner's call: lighter and more bug-resistant than 3D
movement against colliders). Each map has a `## nav` section: convex polygons in map units,
`id: x z h, x z h, x z h[, ...]` (3–8 vertices, counter-clockwise, each vertex with its own height, so
a ramp is just a sloped polygon). Adjacency comes from shared edges at load. A walker's state is
(polygon, x, z); height is interpolated from the polygon's plane. Motion that leaves a polygon
through a shared edge crosses into the neighbour; through an unshared edge it slides along the edge.
Obstacles are simply where the mesh isn't: the height grid, walls and props are rendering only and
block nothing. Walkers push out of each other by radius, then re-resolve against the mesh. The Dev
overlay draws the mesh as a wireframe with polygon ids, because maps are authored by hand. No jumping,
no falling. Cells are 1.0 units; a half-step is 0.25 units.

## Art contract — `story/field/`

Every file is a PNG with alpha (sprites) or opaque (tiles). The engine looks for art on the phone in
`files/field/<kind>/<id>.png` first, then in the APK assets, then falls back to its placeholder.
`fast_reload.sh` pushes changed files; `deploy.sh` bundles them, both exactly as they do for panels.

- **Tiles** `story/field/tiles/<id>.png` — **64x64**, opaque, seamless on all four edges. Ground tiles
  are top-down; wall tiles are front-on and repeat vertically per half-step (a 64x64 wall tile covers
  one full step = 2 half-steps). Expected ids for chapter one: `grass`, `dirt`, `stone`, `plank`,
  `water`, `cliff`, `wall_plaster`, `wall_timber`.
- **Props** `story/field/props/<id>.png` — any size, alpha, drawn 1 pixel = 1 internal pixel at 64
  px per cell. The bottom row of opaque pixels is the ground contact line; the horizontal centre is
  the anchor. Expected for chapter one: `house_a`, `house_b`, `guild_hall`, `well`, `cart`, `fence`,
  `tree_a`, `tree_b`, `grain_shed`, `ladder_house`, `practice_post`, `barrel`, `sign`, `milestone`.
- **Walkers** `story/field/walkers/<id>.png` — a sheet of **4 rows (S, W, E, N) x 4 columns** (stand,
  step-left, stand, step-right), each frame **32x48**, alpha. Expected: `falke`, `ottilie`, `hart`,
  `stolz`, `villager_a`, `villager_b`, `clerk`. NPCs that only face one way still use the sheet.
- **Manifest** `story/field/manifest.md` — generated by the pipeline: every id, its kind, size, source
  sheet, and whether the file exists. The engine does not read it; humans and agents do.

## Pipeline commands (`story_prompt.py`)

**The template sheet (owner's idea).** The tool does not ask ChatGPT to invent a layout. It **writes a
template PNG** — a flat magenta canvas with white-bordered slots already drawn, each slot with its
number painted in the corner (a tiny built-in pixel font; the tool has a stdlib PNG codec, no
Pillow) — and the package tells the owner to attach the template as the first image. The prompt then
says "fill slot 1 with …, slot 2 with …" and "keep the borders and numbers exactly where they are,
draw nothing outside a slot, leave the magenta untouched". Slicing reads the slot boxes from the
package's JSON (the tool made them, so it knows them; no border detection needed), keys magenta to
alpha, trims to the opaque bounds, and writes the files. If the returned image is not the template's
size, scale the boxes proportionally first.

- `tiles <id> [<id>...]` → template of square slots on **black** (tiles are opaque), prompt asks for
  seamless top-down (ground) or front-on (wall) pixel-art tiles in the locked style, style reference
  attached. Slice: known boxes, then nearest-neighbour resize to 64x64.
- `props <id> [<id>...]` → template of slots sized to each prop's stated footprint on **magenta**, one
  object per slot, front three-quarter view from slightly above matching the camera pitch, ground
  contact at the slot's bottom edge. Slice: known boxes, key magenta, trim, write `props/<id>.png`.
- `walker <Name>` → template of the 4x4 frame grid (32x48 frames scaled up 4x so ChatGPT has pixels to
  work with) on magenta, the character's reference sheet attached, rows labelled S/W/E/N in the
  prompt. Slice: known boxes, key magenta, nearest-neighbour downscale to 32x48, write the sheet.
- All three write `story/out/<name>.template.png`, `story/out/<name>.chatgpt.md` and
  `story/out/<name>.sheet.json`, append to `story/field/manifest.md`, and follow the same rule as
  `sheet`: a prompt that breaks the rules exits non-zero. Style text comes from `story/STYLE.md`'s
  locked blocks; never hand-write it. `slice` grows a mode for these packages (or they get their own
  `cut` subcommand); the owner's loop is generate → pull the PNG → one command.

## Game flow

Title → intro cutscenes (as now) → **field** on `halm.map` at the spawn. The Dev overlay gets a
**Field** button (jump straight to the field), a map dropdown, cell coordinates under the player, and
the camera sliders. Hot reload serializes: map name, player x/y (float), facing, and the camera
tuning. Touching an NPC's cell edge and tapping the action button plays its talk scene through the
existing cutscene player and returns to the field. Exits change map. `message` triggers show the
dialogue box with `Narrator` text. `zone` triggers log `encounter <table>` to the Dev messages for
now — battle is the next stream, not this one.

## Controls (touch, landscape)

Left half of the screen: a floating virtual joystick (appears where the thumb lands, 8-direction
snap, dead zone). Right half: tap = action/confirm, long-press = run. Draw both with ImGui, half
transparent, no textures.

## First maps

`halm.map` (the town: nine streets, a grain yard with the shed, a well, the guild hall, and Hart's
yard **up the hill** — use height, that is the point of 2.5D) and `hart_yard.map` (six practice posts,
water barrel, the ladder house), both blocked out with placeholder art and the props named above so the
pipeline's first sheets drop straight in.
