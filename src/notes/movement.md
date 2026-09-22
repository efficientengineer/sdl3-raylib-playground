# Movement — the navmesh, free 8-way walking, and jumping

> Split out of `src/VOXFIELD_NOTES.md` (2026-09-21) when that file passed a thousand lines. Nothing
> was rewritten; the sections are exactly as they were. `src/ENGINE.md` is the map of `src/`, and
> `src/notes/` is the long-form record behind it.

Covers: the generated nav data, erosion and the 0.24 radius, the move itself, jumping, facing,
followers and NPCs, triggers, reachability, the debug overlay, the walk test, input and cost.

## Movement — a generated navmesh, free 8-way walking, and jumping

The owner, 2026-09-20: *"we need to use a navmesh for movement. We could start with the movement we
have now as a basis, but I want 8 directional movement instead of the 4 we have now, and jumping"*,
then *"Fully free is better for touch."* And the standing reason for a navmesh at all: *"lighter and
more bug resistant than relying on real 3D movement with colliders."*

**The party is a point with a radius.** Grid walking, the turn hold and the one-tap-to-turn are gone
from this field (they stay in the parked tile field). `VxActor` carries a float `x`/`z`/`y`; `tx`/`tz`
are `floor()` of them and exist only so the trigger grid, the Dev readout and the save keep working.

### The nav data, generated at load

No hand authoring: `vx_build_nav` reads the voxel grid the map builder just made. Resolution is the
VOXEL — half a walk cell, 2x2 to the cell — which is what lets a fence post block half a cell without
the whole cell going solid.

A nav voxel is walkable when its cell is walkable in the tile map, the voxel column's top is that
cell's own walking surface (nothing built on it), that top face is something you can stand on, and
**four voxels of headroom** are clear — the same rule `vx_can_stand` enforces for the houses, applied
per voxel. `nav_h` is the surface height at the voxel's centre, read from `vx_surface_v`, so a ramp is
sampled twice across and comes out as a continuous plane.

Connectivity is by height: **one voxel or less is a walk edge, more is a LEDGE.** A ledge may be
dropped off or jumped off; it may not be walked up. That test is made live against the body's own
height rather than baked, because after a jump the body can be anywhere.

- `nav_ok`, `nav_h`, `nav_reg`, `nav_clr` — one byte or float a nav voxel, about 250 KB a map.
- `nav_reg` is the connected components under WALK connectivity, seeded at the spawn so **region 1 is
  everything you can walk to**. Everything else is a **jump-only region** — which is where hidden
  loot goes later; the count and the sizes are in the `SELFCHECK vox: nav` line.
- `nav_clr` is a chamfer distance transform, for the overlay and the corridor-width log only.
- **Build time: halm 0.77 ms, hart_yard 0.20 ms, west_road 0.33 ms.** Once, at map load.

### Erosion, and why the radius is 0.24

The clipping bug from the old 3D field ("the player clips into fences and walls") is fixed by
**erosion by the agent radius**, done exactly and continuously rather than as a second grid: the body
is a circle and every move is resolved out of the square of any blocked nav voxel it overlaps. That
IS the Minkowski erosion, with no quantisation, so the centre can never come closer than `agent_r` to
a wall, a fence, water, or a ledge that steps up.

`VX_AGENT_R` is **0.24 walk cells**, and the number is a proof, not a taste:

> A nav voxel is 0.5 cells across, so its centre is 0.25 cells from each of its own edges. With the
> radius **under** 0.25 the circle at any free voxel's centre can never reach a blocked voxel, and the
> straight segment between two 4-adjacent free centres runs down the middle of a 0.5-wide free strip.
> So **walk connectivity on the nav grid always survives erosion**: the reachability proof and the
> collision can never disagree, and no corridor the tile map considers walkable can be closed by the
> radius.

Raise the default past 0.24 and that guarantee goes, and the widening path below starts to matter.
The Dev slider goes to 0.45 so the owner can see what that looks like; it re-derives the nav data on
release. On all three maps the widening pass has never had to run (`widened=0`).

### The move itself

- desired velocity from the input, smoothed with a 0.06 s time constant on the ground (a JRPG wants
  the character to start when you push), the same constant divided by the air-control fraction in the
  air;
- **sub-stepped** so no single step exceeds half a nav voxel — the body cannot tunnel through a
  one-voxel fence however large `dt` is (and `dt` is clamped to 0.1 s so a hitch is not a teleport);
- each sub-step resolved by up to 3 depenetration passes, deepest blocker first, which is what makes
  an inside corner converge instead of oscillating. Sliding falls out of it: the push is along the
  square's normal, so the tangential motion survives.
- **speeds are equal in every direction** — a keyboard diagonal is normalised.
- **Every spawn, teleport and door arrival SNAPS** to the nearest valid nav point by a ring search
  (`vx_nav_snap`) and logs when it had to move more than half a cell. That is the second old bug
  ("not landing on a walkable poly at spawn") closed by construction.
- **The invariant**: a frame may not end with the centre outside the region. If one does it is a bug,
  so it logs `BUG — the leader ended a frame off the navmesh` **once**, snaps back and carries on.
  The walk test asserts this never happens.

### Jumping

No physics engine, five numbers.

| | default | why |
|---|---|---|
| `sp_walk` | 4.4 cells/s | a shade under the old grid walk, which was 1/0.16 s = 6.25 and read as a sprint once movement went free |
| `sp_run` | 7.2 cells/s | the old run, 1/0.10 s = 10, likewise pulled back |
| `jump_apex` | 1.25 cells | the owner's number: clears a 2-voxel (one old block) step with room to see it |
| `gravity` | 40 cells/s² | tight. Apex 1.25 at g 40 is `v0` 10 and **0.50 s of air**, so a running jump crosses about **3.6 cells** — well past the 1.5 the brief asked for, and the arc still snaps |
| `agent_r` | 0.24 cells | see above |

Coyote time 80 ms, jump buffering 100 ms, air control 40% of the ground acceleration. All five are Dev
sliders and all five ride the reload blob.

While airborne the body is **off the navmesh**: the same circle is tested against voxel SOLIDS
instead, any column with something in the band from the feet to head height, with the same sliding.
Head bump under a ceiling, a crown or an eave kills upward velocity. Landing takes the nav surface at
the current x,z if it is valid, or nudges up to the radius to the nearest valid point; with no valid
landing at all — water, the void off the map edge, a top too narrow to stand on — the body **returns
to the takeoff point with a quick fade and a log line, and takes no damage**. Falling off the world
does the same. Walking off a ledge is the same fall with no impulse, and dropping any height onto
walkable ground is allowed.

**The blob shadow is what makes a jump readable**: it stays on the ground under the body and shrinks
and fades with height. The **camera follows the ground-projected position** with a softened height, so
a jump does not bob the whole town; it is clamped to within 6 cells of the body so it can never lose
the party — the third old bug.

### Facing, with four rows of art

Eight directions of movement, four rows of art (S, side, N; the side row mirrored for E), so a
diagonal sits exactly on a boundary and flickers if you take the nearest row every frame. The facing
is **kept until the move vector is more than 55 degrees off its axis**. The walk cycle is driven by a
phase that advances with the body's ACTUAL speed, so a creep on the touch stick ambles, a run strides,
and a body pressed into a wall stops moving its legs. Stopped is the stand frame.

### The followers, and the NPCs

**Ottilie walks the leader's breadcrumb trail** — a ring of 512 points at 0.06 cells apart with their
own arc length — sampled 1.15 cells behind per party slot. Same route, same jumps, at the same spots,
with no second simulation to get wrong, and **no collision between party members at all**, so she can
never be the thing standing in your way. If the trail is too short, the sample lands off the region,
or she is more than 8 cells away for 2 seconds (or 14 cells at all), she teleports beside the leader
with a quick fade.

**NPCs wander freely** on the nav region: a target within the home radius, taken only if the straight
line to it stays inside the region, walked at 1.5 cells/s, abandoned if they walk into something. They
stop and turn to face the player on interact. They block the player **softly** — a push-out circle
where the NPC takes 65% of the correction and the player 35%, both re-resolved against the region — so
an NPC can never wedge the player: being pushed at is what makes the NPC step aside.

### Triggers

Still **cell-based**, because the .tmap means them that way. `exit`, `door`, `zone`, `trap` and
`scene` fire when the leader's cell changes **on the ground**; an exit whose rectangle touches the map
border fires in the air too, so a jump off the edge still takes you to the next map. `message` and
`npc` need the interact press, with the `!` shown when the target is within 0.9 cells and roughly
faced — a 100-degree cone — or under the feet. Dialogue freezes movement and cancels jump input.

### Reachability

`vx_verify_reach` (cell level, unchanged) still runs first. Then the nav flood proves, under **walk
connectivity only — nothing the story needs may require a jump** — that every exit, door, message
trigger and NPC is in region 1. When one is not, `vx_build_nav` removes the **decoration** voxels
(anything above the surface that is not a full cube: sills, rails, eaves) from that cell and its
neighbours, logs how many, and rebuilds, up to four rounds. Structure is never removed. The verdict is
`navreach=ok|FAIL` in `SELFCHECK vox: nav`, with `widened=` and the jump-only region sizes beside it.

**A bug found doing this and fixed here:** `vx_can_stand` tested a ramp cell's own first two voxels
for a ramp SHAPE, but the generator fills the far row with a solid CUBE and puts the slope on top of
it. Every ramp cell in the game therefore failed the stand test, the reach pass declared it
unreachable, and the flatten-and-retry **deleted every ramp** — halm's two and west_road's thirteen —
after the `ramps=` counter had already been printed. Ramps now survive, and west_road's jump-only
voxels went from 4 to 0 and halm's from 76 to 60 as a result.

### Debug

Dev panel, under the map buttons: **Nav view**, **No clip** (off by default), and sliders for walk,
run, jump apex, gravity and radius. Nav view draws the **eroded** region — where the centre may be —
in translucent blue, jump-only regions in magenta, every ledge edge as an orange bar, the body's
radius circle in cyan and the breadcrumb trail in yellow. Blue and not green because most of this
world IS green grass. **The gap between the orange line and the blue is the buffer**; looking at that
gap in a capture is how the clipping bug is checked for. `Print cell` adds the nav verdict, the
region and the clearance at the body's own voxel.

```
./capture.sh --vox halm --at 21,21 --nav 1        the overlay, in a capture
./capture.sh --vox halm --nav 1 --radius 0.4      what a bigger body would be allowed
./capture.sh --vox-walktest [map|all]             the movement bot
```

### The walk test

`capture.sh --vox-walktest` drives the **same movement code** a thumb drives, with no rendering and no
phone, and exits non-zero on failure. Three parts:

1. **6 000 frames of scripted input** at jittered dt (8–35 ms) pressed into walls, fences and corners.
   Asserts the body never leaves the eroded region, never NaNs, and is never stuck for more than 3 s
   while the input is held along a path that is open a cell ahead.
2. **A\* on the nav grid** (the bot's own, walk connectivity only — nothing in the field pathfinds)
   from the spawn to every exit, door and NPC, then the bot walks the path and has to arrive.
3. **200 jumps** from random valid spots on random headings, each of which must end on valid ground or
   in a clean respawn.

```
WALKTEST halm:      ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 6/6 | jumps 200/200 clean (4 respawns)
WALKTEST hart_yard: ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 2/2 | jumps 200/200 clean (1 respawn)
WALKTEST west_road: ok  frames=6000 off-region=0 nan=0 stuck=0 | paths 1/1 | jumps 200/200 clean (2 respawns)
```

The respawns are jumps that ended over the stream or off the map edge — the clean outcome, not a
failure.

### Input

**Touch is a fully free floating analog stick.** It appears wherever the left thumb lands in the left
45% of the screen, any angle, dead zone 12% of its throw, and **the magnitude is the speed**: under
55% of the throw is a walk (creeping at the very bottom), over it is a run, with a ring drawn at the
line so the thumb can feel where it is. **There is no run button** — one fewer thing for a thumb to
find, and the owner's "fully free is better for touch". The right side has two round buttons, **JUMP**
and the interact button that was already there, both low and to the right so neither is near
Dev/Settings at the top right; `vx_btn_act`/`vx_btn_jump` are the one place their geometry lives, so
the hit test and the drawing cannot drift apart.

Keyboard: **WASD/arrows** (8 directions, normalised), **Shift** run, **Space** jump, **Enter or Z**
interact. Space used to be interact; it is the jump now.

### Cost

`movement` is its own perf scope, nested inside `tick/logic`. It measures well under 0.05 ms a frame
on the Mac with four actors and the NPCs — the whole system is a few dozen nav-voxel lookups. The nav
build is load time only and logs its own ms.
