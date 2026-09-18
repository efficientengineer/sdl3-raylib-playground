# FIELD_NOTES — what the engine actually does

Companion to `FIELD.md` (the contract). Where the two disagree, this file describes the shipped
build and says why. Code: `src/field.h`, `src/field.cpp`, hooks in `src/star_logic.cpp`.
Maps: `story/field/maps/halm.map`, `story/field/maps/hart_yard.map`.

---

## What works

- **Title → intro → field.** The intro playlist runs as before; when it ends the game walks out into
  `halm.map`. The Dev overlay has a **Field** button that jumps straight there.
- **2.5D renderer.** Ground and walls are real 3D geometry from the map's height grid; buildings are
  3D boxes; everything else is a camera-facing billboard. It all renders into an offscreen FBO during
  `tick()` and goes to `ImGui::GetBackgroundDrawList()->AddImage` as one nearest-filtered image,
  because the host clears the framebuffer *after* tick. GL state is handed back the way the ImGui
  backend expects it (FBO 0, viewport, blend on, depth off, program 0, VAO 0, buffer 0, unit 0).
- **Navmesh movement** with a walker radius, so nothing clips into a fence or a wall.
- **Camera zones** — follow / fixed / rail per zone, with a framing *guarantee*.
- **See-through occluders** — a dithered circle cut out of whatever stands between camera and player.
- **Visible blockers** — every open navmesh edge grows a fence, hedge or wall automatically, except
  where the terrain, a building or an **exit** already explains itself.
- **Organic map layout** built to FIELD.md's "Map design rules" (checklist at the end of this file).
- Touch controls, NPC talk scenes through the existing cutscene player, map exits, message triggers,
  zone triggers logged to the Dev messages.
- **Hot reload keeps your place**: map, position, navmesh polygon, facing, the camera zone you were
  tuning and every debug toggle survive. An older blob falls back to the map's spawn, never crashes.

---

## Movement — the navmesh (supersedes FIELD.md's "Rules" paragraph)

Movement runs on a hand-authored convex-polygon navmesh. **The tile height grid, the walls, the props
and the buildings are rendering only.** Nothing blocks: obstacles are simply where the navmesh isn't.

### `## nav` syntax

```
## nav
1: 12.00 1.00 4.00, 17.00 1.00 4.00, 17.00 7.00 4.00, 12.00 7.00 4.00
20: 17.00 7.00 4.00, 18.00 7.00 4.00, 18.00 12.00 0.00, 17.00 12.00 0.00   # a ramp: one sloped poly
44: 5.00 26.00 0.00, 8.00 26.00 0.00, 8.00 29.00 0.00, 5.00 29.00 0.00 | hedge
```

- One polygon per line: `<id>: x z h, x z h, x z h[, ...]`, 3 to 8 vertices.
- `x z` are map units (a cell is 1.0). `h` is the vertex height **in half-steps** (the same unit the
  `## height` grid uses), so `4` is 1.0 world units up. Fractions are allowed. Each vertex carries its
  own height, so **a ramp or a hillside is one sloped polygon**.
- Winding does not matter — the loader normalises it from the signed area. Convexity does.
- Optional trailing `| fence` / `| hedge` / `| wall` / `| none` says what to draw on that polygon's
  open edges. The map-wide default is `edges: <kind>` in `## meta`.

### How it behaves

- Adjacency is computed at load from shared edges (the same two endpoints in either order, 0.01
  tolerance). An open edge whose midpoint lies **inside** another polygon logs
  `field: nav poly N edge E overlaps poly M but shares no vertices` — the one authoring mistake worth
  shouting about, and the only reason two polygons that look joined aren't.
- Player state is `(polygon, x, z)`. Height is **interpolated from the polygon's plane**, fitted
  through its three best-spread vertices — never sampled from the height grid.
- Moving applies the velocity, then: leaving through an edge **with** a neighbour walks into the
  neighbour; an edge **without** one projects the rest of the motion along it. Up to 4 crossings per
  frame, then a containment check that snaps back rather than leaving the mesh. Penetration is
  impossible by construction.
- **Walker radius** (`WALK_RADIUS`, 0.30 cells, Dev slider 0–0.8). The mesh resolves a point but the
  sprite has width, so after the slide-resolve the point is pushed out of every **unshared** edge of
  its polygon *and of its neighbours* that is closer than the radius, and the push is itself run
  through `mesh_move` so it can never end up off the mesh. Two iterations, which handles corners.
  Shared edges are deliberately left alone so crossings still line up exactly. The same pass runs on
  spawn, so you never appear inside a fence.
  *(The brief offered navmesh insetting as the first option; the push-out was taken because insetting
  a shared-edge mesh has to leave shared edges alone anyway, and a collapsed inset polygon is a much
  worse failure than a 0.3-cell push.)*
- NPCs are soft: a radius push-out (0.55), re-resolved through the mesh, then the wall buffer again.
- Triggers, exits and the NPC reach test are **rectangles in map units**.
- Fence strips stand exactly on the navmesh edge and buildings sit entirely outside it (the map
  generator asserts that), so what you see and what stops you are the same line. The walker sprite's
  anchor is its feet centre (the billboard's bottom edge, centred).

---

## Cameras — zones

```
## cameras
0 0 40 30 follow 0 50 30 13 0.8
14.0 13.5 9.0 8.0 follow -15 52 27 11.5 0.9
17.5 6.0 8.0 8.5 fixed 19.0 8.5 19.0 22.6 2.4 5.2 30
5.5 16.5 9.0 7.0 rail 5.5 6.5 26.0 14.0 6.5 26.0 18.6 1.2 17.4 32
```

- `follow <yaw> <pitch> <fov> <dist> <height>` — orbits the player. **Yaw is per zone**; a 90° turn is
  legal and the billboards keep facing the camera. (10 tokens.)
- `fixed <cx> <cy> <cz> <tx> <ty> <tz> <fov> [pan]` — FF7 style: the camera stays put, the player walks
  toward or away from it and perspective does the shrinking. (12 tokens, 13 with `pan`.)
- `rail <ax> <ay> <az> <bx> <by> <bz> <tx> <ty> <tz> <fov>` — the eye slides A→B with the player's
  progress along the zone's long axis, always looking at T. (**15 tokens** — an off-by-one here made
  the rail line parse as a follow shot with `height 6.5`, which is what the framing watchdog caught.
  A camera line that doesn't parse now logs and is ignored instead of silently becoming a follow.)
- The player's position picks the zone; **last definition wins** on overlap; a zone covering the whole
  map is required and is synthesised with a warning if `## cameras` is missing.
- Zone changes blend position, target and fov over **0.4 s** with a smoothstep.

### Framing — a guarantee, not a target

Two layers:

1. Every frame the engine binary-searches the smallest blend `u ∈ [0,1]` from the authored shot toward
   looking straight at the player that puts the player's head inside a limit rectangle:
   - **follow**: middle **50 %**. Below that it is the authored shot with a focus that **leads the
     player by 0.9 units** and lags with a 0.28 s time constant — so the lag only exists inside the
     rectangle. `u` blends the look-at's x, z **and y**, or a tall `height` would make u = 1 useless.
   - **fixed / rail**: **70 %**. (The brief said 80 %; at 0.8 a long street shot leaves the player half
     behind a foreground prop at the near end of the zone.) For `fixed` and `rail` only the look-at
     moves — this is the automatic `pan`, and `pan` in the map file just forces it on.
   `u` is applied **immediately** when it rises and released over 0.5 s, so the player can never slide
   out while the camera catches up.
2. After the camera is smoothed, the **drawn** camera is re-checked and hard-clamped: if the player is
   outside 90 % of the frame the look-at is pulled onto them by a second binary search. This is what
   makes the property true regardless of lag, zone edges, rail ends or a mis-authored shot — and it
   logs one Dev message every 4 s when it has to:
   `camera: clamped onto the player at halm 8.7,18.1 zone 3 u 1.00 (ndc 0.00,-1.65)`.
   That message is the debug aid the owner asked to keep; it is also how the rail bug above was found.

The follow focus is clamped to the map, so the look-at never runs off the edge.

### The shots in the two maps (a landmark is in frame in each)

| map | zone | shot | landmark |
|---|---|---|---|
| halm | whole map | follow, yaw 0, pitch 50, fov 30, dist 13 | the well / the hill |
| halm | the well square | follow, yaw −15, pitch 52, fov 27, dist 11.5 | the well |
| halm | the hill road | **fixed** (19, 8.5, 19) → (22.6, 2.4, 5.2), fov 30 — the depth shot up the bent road | Hart's hill and the gate |
| halm | the west street | **rail** (5.5,6.5,26)→(14,6.5,26) looking at the square | the well square |
| halm | the grain yard | **fixed** (8, 7, 21) → (6.8, 1.2, 12.6), fov 30 | the grain shed |
| hart_yard | whole map | **fixed**, low pitch: (11.5, 8.5, 28) → (11.5, 0.9, 8.5), fov 32 | the ladder house and the posts |

**Tuning on the phone**: the Dev sliders edit the *live* zone. **print zone** writes it back in map
syntax to the Dev message log, ready to paste into the `.map` file.

---

## See-through occluders

The owner's request: see the character when something is in front of them, and only then.

- In the fragment shader, an occluder fragment is cut when it is **between the camera and the player**
  (`gl_FragCoord.z < player_window_z - 0.0006`) and within a screen-space radius of the player's
  chest. The cut is an **ordered 4×4 Bayer dither** whose density ramps from a full hole at the centre
  to nothing at the radius: `discard if bayer(fragcoord) + d/radius < 1.0`. Screen-door, not alpha —
  no blending, no sorting, the depth test stays intact, and it reads as pixel art rather than glass.
- **Occluders** are buildings, wall/cliff faces, fence strips, props and other walkers (`u_occl = 1`).
  Ground, floors, water and **the player's own billboard** are never cut (`u_occl = 0`).
- The uniform is `u_see = vec4(player screen x, y in FBO pixels, player window depth, radius px)`;
  radius 0 turns it off entirely, so the common case costs one compare.
- **Radius fade**: a CPU test each frame asks whether anything actually blocks the player — a ray from
  the eye to the chest against every building's oriented box, every unshared navmesh edge's strip,
  every prop's cylinder, and a march over the wall heightfield. The radius eases toward
  `occluded ? dev_radius : 0` with a 0.09 s time constant (≈0.2 s in and out), so it never pops.
- Dev: **see-through occluders** toggle and a **radius px** slider (default 46 px at 360 high).

---

## Visible blockers

Rule: **every unshared navmesh edge gets something standing on it**, unless the world already says no.

- Kind per polygon (`| fence` / `| hedge` / `| wall` / `| none`), defaulting to `## meta`'s
  `edges: <kind>`. Both shipped maps use `edges: fence`, with `| wall` on the well square's kerb, the
  grain yard and Hart's terrace, and `| hedge` along the streets.
- The strip is a **mesh that follows the edge exactly**, one quad per edge, standing at the navmesh's
  own vertex heights so it follows a slope. Heights: fence 0.55, hedge 0.75, wall 0.85. The texture
  tiles once per map unit along the edge.
- **Skipped automatically** where a blocker would be wrong:
  - the height grid differs across the edge → it is already a cliff step;
  - the outward side is inside a building footprint (+0.6) → it is already a wall;
  - the edge is inside or within **0.5 of an exit rectangle** → it is the way out, not a wall. In Halm
    the gate through Hart's terrace wall is therefore a real gap, flanked by two `gate_post` props,
    with the dirt road painted through it and continuing off the north edge of the map, so walking out
    looks like walking down a road.
- Dev toggle **Blockers** draws a red line along every unshared edge, raised 0.3, so a map author can
  see at a glance what is blocked and where a fence was skipped. Where grass should be open, extend
  the navmesh instead of deleting the fence.

---

## 3D buildings

```
## buildings
# x z w d h id [rot deg]   footprint in map units, height in world units, +Z face = front
21.9 13.2 5.0 3.4 3.6 guild_hall rot -28.4
```

- A box `w × d` cells on the ground (the height grid under the footprint centre), `h` **world units**
  tall, with a gabled roof whose ridge runs along the box's local X and rises `min(w,d) × 0.34`, with
  a 0.16 eave overhang. Gable triangles close the ±X ends.
- **`rot`** is a yaw in degrees about the footprint centre. Billboards don't care; the box mesh, the
  occlusion ray test and the "is this edge behind a wall" test all work in the box's local frame.
- **Front is the +Z face** — the one the default camera looks at. (FIELD.md's brief said "toward −Z /
  the camera"; the camera sits at +Z and looks along −Z, so the face it sees is +Z. Same face.)
- Buildings block nothing. The navmesh already excludes them (the map generator asserts it); doors are
  ordinary trigger rectangles.
- Vertex-baked light per face, computed from the **rotated** normal against the sun (+X, −Z), so a
  building turned off the grid is still lit correctly. No realtime lighting (CLAUDE.md).

### Face-texture contract (for the art pipeline's future `building` command)

| file | size | how it is mapped |
|---|---|---|
| `story/field/buildings/<id>_front.png` | **128×128**, opaque | stretched **once** across the whole front wall, so a painted door and windows land exactly where the artist put them whatever the building's width |
| `story/field/buildings/<id>_side.png` | **128×128**, opaque, seamless both ways | tiles **once per cell horizontally and once per world unit vertically** on the back, both ends and the gables |
| `story/field/buildings/<id>_roof.png` | **64×64**, opaque, seamless | tiles **once per cell** in both directions on each roof slope |

Ids in use: `house_a`, `house_b`, `guild_hall`, `grain_shed`, `ladder_house`. Placeholders are
generated procedurally (plaster with a painted door and two windows on the front, timber-framed
plaster on the sides, red pantiles above) until the PNGs exist. Lookup order is the phone's
`files/field/buildings/…` first, then the APK assets, then the placeholder.

---

## Art the engine loads

Exactly as FIELD.md says: `files/field/<kind>/<id>.png` on the phone first, then the APK assets, then a
procedural placeholder. `fast_reload.sh` pushes changed files under `story/field/**` into
`files/field/<kind>/`; `deploy.sh` rsyncs `story/field/` into `android/app/src/main/assets/field/` and
wipes `files/field` after install so a stale push can't shadow the APK.

| kind | size | notes |
|---|---|---|
| `tiles/<id>.png` | 64×64 opaque, seamless | `grass dirt stone plank water cliff wall_plaster wall_timber`. Mipmapped, `GL_REPEAT`, nearest mag. |
| `props/<id>.png` | any, **alpha** | 1 px = 1 internal px at 64 px/cell. Bottom opaque row is the ground line, horizontal centre the anchor. |
| `walkers/<id>.png` | 4 rows (S,W,E,N) × 4 cols of 32×48 | `falke ottilie hart stolz villager_a villager_b clerk` |
| `edges/<kind>.png` | 64×64 **alpha**, tiles horizontally | new: `fence hedge wall` |
| `buildings/<id>_{front,side,roof}.png` | see table above | new |
| `maps/<name>.map` | text | pushed and bundled the same way |

> **Note for the art pipeline.** The props currently on the phone render as opaque black rectangles:
> their PNGs have no transparency. Props, walkers and edge strips are alpha-tested at 0.5, so a fully
> opaque sprite draws its whole bounding box. Tiles are the only kind meant to be opaque.

---

## Controls (touch, landscape)

- **Left half**: a floating virtual joystick appears wherever the thumb lands. 8-direction snap, dead
  zone ≈ 2.2 % of the screen width.
- **Right half**: tap (< 0.35 s) = action/confirm; hold > 0.35 s = **run** (×1.75) while held. A thin
  ring marks the pad; it fills while touched and shows `RUN`.
- Both fingers work at once. The field reads **SDL touch fingers directly**
  (`SDL_GetTouchDevices` / `SDL_GetTouchFingers`), not ImGui's mouse, because the ImGui SDL3 backend
  only ever sees the primary touch. With no touch panel the mouse stands in for one finger.
- **The stick is in screen space, always.** "Up" walks away from the camera along the ground projection
  of the view direction, "right" is camera-right, in every camera mode — a 90° zone yaw or a fixed shot
  never inverts the controls. Facing is stored in world terms and converted to the right S/W/E/N sprite
  row for the current camera.
- Input is suppressed while the Dev panel is open. The action button dismisses a message box if one is
  up, otherwise talks to the nearest NPC within 1.8 units in front.

---

## Dev overlay

`Dev` (top right) → the panel, which shows, when the field is on screen:

- `map <name>  x <x> z <z>  poly <id>  zone <n>  facing <S|W|E|N>` and the map's `landmark:` line
- map buttons **halm** / **hart_yard**, **Respawn**
- **Nav** — the navmesh as a coloured wireframe with polygon ids projected on top (open edges darker)
- **Blockers** — a red line along every unshared edge
- **fill width** — off renders a strict 640×360 pillarboxed; on (default) widens the internal buffer to
  the screen aspect at a fixed 360 height (800×360 on the phone), so no black bars
- `zone <n> mode <follow|fixed|rail>` and **print zone**
- live sliders for the current zone; **see-through occluders** + **radius px**; **walk radius**

Camera clamp warnings and `encounter <table>` from zone triggers arrive in the **Messages** list.

---

## Map design — how Halm and Hart's yard are built

Both maps are generated by a small authoring script (desire lines → junction hulls → offset ribbons)
rather than hand-typed, because every shared navmesh edge has to be an *exact* vertex match while
nothing is axis-aligned. The method, for the next map:

1. Mark the **origin point** (Halm: the well at 18.6, 17.4).
2. Place **junction nodes** and, for each, the directions and mouth widths of the streets leaving it.
   The junction polygon is the convex hull of the streets' mouth points, so each street's mouth is an
   exact hull edge. The well square is that hull turned into a **ring** around a central island, which
   is why the well itself is solid ground you walk around.
3. Draw each street as a **ribbon**: a bending centre line with a half-width per point, offset to left
   and right polylines, one convex quad per segment. Consecutive quads share a whole edge; the first
   and last edges are pinned to the junction mouths (oriented to the ribbon's own left/right, or the
   end quad self-intersects).
4. Terrain (hill, stream) comes before buildings; the ground tiles are painted **from the navmesh**, so
   the roads follow the bends exactly and grass fills the rest.
5. Buildings last, fronts to the path (`rot` computed from the direction to the street), with checks
   that no building overlaps a walkable polygon and no two neighbours within 9 units are within 8° of
   parallel.

**Street widths (binding for the next map): streets are 3–4 cells across, lanes never under 2.**
In Halm the mouth half-widths are 1.50/1.45/1.35 for the three main streets (3.0–3.0 cells), 1.05 for
the one deliberate pinch (2.1 cells, still comfortable), and 0.95–1.24 for the outer lanes.

---

## Deviations from FIELD.md, and why

1. **Camera yaw.** FIELD.md says the camera looks along +Z. It looks along **−Z** (GL's default), so map
   row 0 is the far/north edge and a `.map` file reads with north at the top. A zone's `yaw` rotates
   from there.
2. **Internal resolution.** 640×360 is the base, but **fill width** (default on) widens the buffer to
   the screen's aspect at a fixed 360 height rather than pillarboxing a 20:9 phone with 240 px of black
   each side. Turn it off in the Dev panel for the strict 640×360.
3. **Billboards are full camera-facing quads**, not Y-only. A Y-only billboard under a 50° pitch splays
   badly at the edges of frame (world-up lines diverge) — the first build looked like the houses were
   falling over. Leaning back along the camera's up vector also lifts a sprite clear of the ground
   behind it, so the depth test alone is enough: no sorting, no `1/cos(pitch)` stretch.
4. **`## props`' `solid` / `walk` flag is gone** (parsed and ignored). Blocking is the navmesh's job.
5. **`## npcs` gained an additive `say:` form**: `x z walker facing Name say:<line>`. A bare trailing
   token is still a scene id, exactly as FIELD.md's example.
6. **New sections**: `## nav`, `## cameras`, `## buildings`; new `## meta` keys `edges:` and `landmark:`.
7. **Exits and triggers fire on the rising edge**, so a spawn inside its own door rectangle doesn't
   bounce the player straight back.
8. Nav vertex heights are **half-steps**, matching `## height`; building heights are **world units**
   (they aren't terrain).
9. `src/field.cpp` is ~2340 lines rather than the ~1200 the brief asked for; the navmesh, camera zones,
   buildings, blockers and see-through all arrived after that budget was set.

---

## Still stubbed

- `zone` triggers only log `encounter <table>` to the Dev messages. Battle is the next stream.
- No post-process pass yet (the FBO is drawn straight through).
- NPCs don't walk; they stand and face a fixed direction.
- One Halm NPC ("Hunter", x22 z19) is wired to the real scene `003b_after_the_warning` to prove the
  field → cutscene → field round trip. Placeholder wiring, not a story decision.
- Talking to an NPC whose scene id isn't in the playlist shows
  `(scene "<id>" is not in the playlist yet)` in the field's dialogue box rather than failing.

---

## FIELD.md hand-off checklist

| item | answer |
|---|---|
| origin point named | **the well** (18.6, 17.4). The square is the junction it made; everything radiates from it. Hart's yard: the ladder house and the post arc. |
| desire lines drawn before buildings | Yes — the nine streets and both junctions exist in the generator before the `blds` list, and buildings take their rotation from the street they face. |
| no straight street longer than three houses | Yes. Every street bends at its junction mouth; the hill road bends twice climbing the shoulder. The longest straight run is one ribbon segment, ~3 cells. |
| no two neighbouring buildings parallel | Checked automatically: any two buildings within 9 units whose yaws are within 8° (mod 180) fail the build. None do. |
| stream and slope paths curved | The stream is a 7-point polyline crossing the east side at an angle to every street; the hill road is a curving ramp (one sloped navmesh chain), not a straight one. |
| edges ragged and the border hidden | 23 trees at uneven spacing round the border, the hill covers the north, the stream the east, the orchard lane dead-ends into the south-west corner. The exit road runs *off* the north edge rather than stopping at a fence. |
| landmark in every camera zone | Yes — table above. The depth shot is aimed at Hart's hill and the gate. |
| every building has a reason | guild hall (door trigger), grain shed (the yard and the Grainwife), ladder house (Hart's gate), well square houses (Marta, Ostler, the Hunter), the hill house (Gate Watch). 10 buildings, in the 6–12 band. |
| `landmark:` set | halm: *"the lopsided well square, and the road that climbs round the shoulder of Hart's hill"*. hart_yard: *"the six posts set in a crooked arc round the ladder house"*. Shown in the Dev overlay. |
| graph-paper test passed | Yes — checked from above on the phone with the Dev camera and in the generated `## ground` grid; no right angles, no repeated spacing, no axis-aligned street. |

---

## Proof it ran on the phone

`./fast_reload.sh`, device `192.168.1.217:5555` (final build of this pass):

```
=== Compiling star_logic.cpp ===
=== Compiling field.cpp ===
=== Linking libgame_logic.so ===
=== Pushing to device ===
  field maps/halm.map
  field maps/hart_yard.map
=== Setting reload flag ===
=== Waiting for host to apply reload ===
09-18 12:34:47.502 14873 14896 I QuestGlory: Hot reload SUCCESS (401888 bytes, gen 1)
=== Done ===
```

Confirmed on the device afterwards: `adb shell pidof com.playground.sdlraylib` still returns the same
pid, `logcat -d | grep -cE "F DEBUG|signal [0-9]"` returns 0, and the Dev message log has had no
`camera: clamped` line since the rail-parsing fix. Screenshots through the pass show Halm rendering
with 3D rotated buildings, fenced and walled edges, the gate gap with its road running off the map,
the organic radial street plan, the player framed in every zone and kept clear of walls, walking on the
navmesh, the zone trigger posting `encounter halm_outskirts`, and an NPC playing
`003b_after_the_warning` through the cutscene player and returning to the field.
