# Testing — the Dev panel, the tooling, and every suite

> Split out of `src/VOXFIELD_NOTES.md` (2026-09-21) when that file passed a thousand lines. Nothing
> was rewritten; the sections are exactly as they were. `src/ENGINE.md` is the map of `src/`, and
> `src/notes/` is the long-form record behind it.

Covers: the Dev panel, the tooling and ship lists, the voxel self-test, the three tests and what
each does NOT prove, the play-test, the other tests, the robustness sweep and the party lantern.
`./capture.sh` is the one way any of them is run on the Mac.

## Dev panel

Sun / shadow: azimuth and elevation (each re-renders the shadow map), strength, softness, a
**Snapped** toggle and **Sun default**. Then: map buttons and Respawn; Coords, No clip, Cut away;
**Nav view** and the movement sliders (walk, run, jump apex, gravity, radius — see "Movement");
Ortho, pitch, FOV, view blocks, sprite tilt, AO,
detail density, fog; HD-2D with tilt-shift, bloom, vignette, grade and res %; the colormap table
buttons and the ambient slider; **Print cell** (height, top block, walkable, tris, mesh ms, reach).
Above them, **Old 3D field** and **Old tile field**.

## Tooling

```
./capture.sh --vox-selftest                               every map loaded, meshed and checked
./capture.sh --vox-walktest [halm|all]                    the movement bot: walls, paths, 200 jumps
./capture.sh --vox halm --nav 1 [--radius 0.4]            the navmesh overlay, in a capture
./capture.sh --vox halm                                   build_desktop/vox_halm.png, 1920x1080
./capture.sh --vox halm --at 21,21 --name spawn           stand the party on a cell
./capture.sh --vox halm --ortho 1 / --hd2d 0              the toggles, for a side by side
./capture.sh --vox halm --light night:0.30 --name night   the same colormap the phone uses
./capture.sh --vox halm --pitch 40 --fov 32 --viewh 11 --face N --size 2400x1080
./capture.sh --vox halm --cut 0 --fog 0                   the diagnostics
./perf.sh halm                                            the A/B benchmark on the phone, table + report
./perf.sh --desktop halm                                  the same on the Mac
./perf.sh --watch                                         tail the periodic frame lines
```
`run_desktop.sh` runs the same binary; `files/map.flag` still drives it to a map with no taps.

`fast_reload.sh` and `deploy.sh` now ship `story/field/tilesets/<set>/decals/` as well as the atlas,
`tiles.md`, `tmaps`, `walkers`, `swatches` and `palette/`. Everything the voxel field reads is on that
list; a file that is not on it shows on the phone as black.

## The self-test

`./capture.sh --vox-selftest` loads every map in turn, builds and meshes each,
renders its shadow map and prints a `SELFCHECK vox:` line — and a second `SELFCHECK vox: nav` line
with the navmesh's own verdict (walkable voxels, walk-reachable, jump-only regions and their sizes,
how much was widened, `navreach`, `jump-targets`, build ms) — with the shaped-voxel count, the
number of buildings generated and `reach=ok|FAIL`, then a verdict line. The script greps that line
and exits non-zero. All five pass.

It also runs a **colormap identity check** first: `day` at level 0 must equal `master.hex` exactly
for all 255 opaque indices. It does (2026-09-21). Both shader lookups address texel centres —
`(idx+0.5)/256.0, (row+0.5)/u_cmaph` — and the CPU lookup indexes `cmap_px` directly, so the engine
is not shifted; the earlier "index 24 comes back cyan" report was a shifted diagnostic, not a
shifted engine. **Nothing in this engine cycles colormap columns**: `cmap_px` is written once at
load and never touched. The old tile field's per-frame column rewrite went with the tile field.

---

## The three tests, and what each one does NOT prove

This matters more than what they do prove, because for two months the chapter self-test passed every
run while three of the chapter's gates had **no map trigger anywhere** that could open them.

| test | drives | proves | does NOT prove |
|---|---|---|---|
| `--chapter-logic-selftest` (was `--chapter-selftest`) | `ch_on_text` / `ch_on_pickup` / `ch_on_battle` directly | the step table is well-formed, every clip exists in `cutscene_data.h`, the gates chain to the end card, `ch_steps_selfcheck` agrees with `ChStepId` | **anything at all about the world.** It types the answers in itself. A flag with no trigger, a trigger on no map, a trigger that fires in the wrong step — all invisible here |
| `--vox-walktest` | `vx_leader_move` directly, no `vx_tick` | the body: it never leaves the eroded region, never NaNs, never sticks with the way open; A* reaches every exit, door and NPC; 200 jumps land or respawn cleanly | anything about the chapter. It does not know what a flag is |
| `--chapter-playtest` | **the real game**, through `vx_tick`, a stick and two buttons | that a player can finish the chapter: every gate opened by walking to a reachable trigger and pressing the button, fights won through the real menu, clips tapped through the real renderer | the typewriter and pagination (see below), the Dev panel, resize. Save/load, the reload blob and the battle menu under random input are `--robustness` |

### What the play-test is

`CHAPTER_PLAYTEST=1`. A bot with a stick and two buttons. **It never calls `ch_set`, `ch_on_text`,
`ch_on_pickup`, `ch_on_battle`, `ch_jump` or `ch_advance`.** It is told a FLAG, looks up which text
or item id binds it in the chapter's own binding table, asks the MAP where that id is
(`vx_find_trigger`), path-walks there on the real navmesh, and presses the real interact button.
Three static checks run first, because they need no play:

- **the hidden finds**, both ways, against `CH_JUMP_ONLY` in `src/chapter01.h`: a find the player can
  walk up to has lost its lesson, and a jump-only target that is *not* in the table is a trigger
  nobody can reach;
- **the bell run**, measured: the direct road's length in cells at walking and running pace against
  `CH_BELL_RINGS * CH_BELL_PERIOD`. The clock has to bite between the two or it is not a clock;
- **the hill climb**, measured: the walking route from the bottom to the top gate, which has to be
  long enough that the climb is a climb and not a ramp.

Then two runs: the **completionist**, which also goes and takes every designed jump; and the **lazy
player**, which follows only the goal line and must still finish having seen all ten mandatory
interactions.

### Two things the play-test cannot do, by construction

1. **It runs headless.** `vx_tick` has one early return after the event queue: everything above it is
   the simulation, everything below is the render. Without it a run costs the GPU tens of minutes and
   ImGui's draw lists grow without bound inside the blocking loop. The consequence is that
   `msg_typing` and `msg_more` — which `draw_msg_box` decides by measuring text against the font —
   are never updated, so **a press closes a box whole**. The bot therefore does not exercise the
   typewriter or pagination. `./capture.sh --dialog` is what covers those.
2. **Clips and battles yield.** Both are drawn with ImGui and a tap is only delivered by an ImGui
   frame cycle, so those steps cost one real frame each and the play steps cost none. That is why the
   whole thing is a state machine driven from `game_tick` rather than a function that runs to
   completion.

### Where it stops (2026-09-21, second round: it does not — both runs finish)

Both runs reach the end card with **10 of the mandatory 10**, in **2 min 14 s of real time** for
5.7 minutes of play each. The four things that stopped the first round, and what each one was:

- **The boss could not be killed, and it was the bot's hands, not the fight.** The menu works like
  the player's: tapping a command row SELECTS it and puts its skill list and effort notch on screen,
  and tapping the same row again COMMITS. The old policy tapped a row twice and never touched the
  skill list, so every Skill cast its owner's FIRST skill — Drowsy, for Distel. Klee's phase one ends
  on **three Settles and nothing else**, so the party made a thing that was already open sleepy for
  ninety rounds. A turn is now planned once per (round, character) and walked as
  *select row → pick skill → set effort → commit*; the boss dies in 16 rounds. `bt_ui_skill_count`,
  `bt_ui_skill_kind`, `bt_ui_skill_min`, `bt_ui_skill_sel` and `bt_ui_point(kind 3)` are the API that
  made it possible to do this by tapping rather than by cheating.
- **`0160_the_herder` was never the problem.** The new `--clips-selftest` taps every scene in the
  chapter through the real player and all six end; what the old log showed was the clip's fade-in
  being logged once a frame, and then the boss fight underneath it. The step line is now written
  **once per step**, with its own line per battle and per pickup.
- **A step whose flags were all set was never ticked.** The chapter only advances inside `draw_field`
  and only when nothing is on screen, so a step that finished with the boss's own death text still up
  advanced on no frame at all — the bot's need loop walks nowhere when every need is met. It now
  reads whatever is up and gives the field its frames before deciding it is stuck.
- **An exit that worked looked like a walk that failed.** `pb_walk_into` asks whether the body ended
  up on the target cell, and stepping onto an exit starts a map change, so by the time it looks the
  body is on another map. The verdict is now the map, not the cell. `pb_play_step` also re-reads
  `vx_current_map` every iteration instead of caching it at the top.

And one engine bug the lazy run found, fixed in `voxfield.cpp` rather than in the map:

- **A cell may carry several triggers, and they now fire in a defined order.** `hart_yard` 6,7 is a
  `message` and a `pickup` on one box. `trig_at` returned the first match, so the pickup could never
  be reached at all. An examine now resolves the WHOLE cell into a queue — every message, then the
  pickup, then the door — and each box closing delivers the next (`trig_collect`, `trig_act`,
  `trig_drain`). The queue is built once per press and survives the boxes it opens; rebuilding it
  per tick would re-deliver the line the player has just dismissed, for ever.

`./capture.sh --chapter-playtest` now exits non-zero on **any** `PLAYTEST FAIL` line, not only on the
summary count.

## The other tests

| test | what it is |
|---|---|
| `--clips-selftest` | every scene in the chapter tapped through by the real player at 60 fps. It must END within `lines x 2 s`, every panel must be revealed exactly once, and every line must be reached — textless lines included. The unit test under the play-test's CLIP steps; run it after editing a scene file. `ct_drive` in `star_logic.cpp` |
| `--robustness [WxH]` | the sweep below. `WxH` is a windowed size (`STAR_WINSIZE`, host.cpp, desktop only) |

### The robustness sweep (`--robustness`)

Not a play-test: narrow assertions about what breaks when the game is interrupted rather than played.
Each names itself and says ok or FAILED; the run ends with one `SELFCHECK robustness` line.

- **A — save/continue at every chapter step.** Every one of the 26 steps: the POD the save and the
  reload blob are both a memcpy of comes back with the same step, flags, party size, items and known
  commands, and with a party the battle screen can draw and no negative hp or stamina.
- **B — the hot-reload blob.** Mid-clip, for every scene: serialize at a line, build a fresh state,
  deserialize, and the screen/scene/line are identical. Mid-battle: the contract is that a reload
  **never** lands you in a battle, and that is the assertion — the flags, step and party come back
  and `in_battle` does not. A blob with a bad magic must be ignored, not half-applied.
- **D — settings.** Volume and mute round-trip through `settings.ini`. `STAR_MUTE` forces mute at
  load and is never written back, which is asserted rather than worked around.
- **F — battle fuzz.** 2,000+ random taps and keys across **every** encounter in `BT_ENCOUNTERS`
  (`bt_enc_count`/`bt_enc_id`, so the list cannot go stale). The watchdog must never fire, no hp or
  stamina may go negative or over its maximum, and a menu with rows in it must always have at least
  one row that can be chosen.
- **H — every field-text id in the real dialogue box.** `vx_msg_measure` is `draw_msg_box`'s own
  geometry with nothing drawn: no page may overflow its space, and the three longest are named.
  All 66 ids fit; the worst are `halm.gate_watch_after`, `halm.lamp_charm_2` and
  `halm.shepherd_market_2`, at two pages each.

Run at 2400x1080, 1920x1080, 1280x720 and a portrait request: all pass, nothing crashes. **Note the
Mac clamps the window to the display**, so 2400x1080 and 1080x1920 both arrive as roughly 1920x1027
and the portrait *shape* is not actually exercised on this machine — the sweep reports the size it
really measured, so the log never claims otherwise.

**Still to write** (the deferred part D, honestly outstanding): the Dev panel sweep — every button
pressed once headlessly — and the five-times soak of the lazy run with RSS and GL object counts
(textures, buffers) asserted flat after the first loop.

### The party lantern

`./capture.sh --vox <map> --light night:0.16 --lantern [r,level]` draws the party's own light as well
as the map's `lamp:` lines, which is what a night map actually looks like in play. Tuned by eye on
`high_pasture`: the falloff is a **smoothstep**, not a square (a square puts nearly all the light in
the first cell and the pool reads as a hard bright disc), and the level is **capped at
`VX_LAMP_CAP` = 0.62** below the top of the `lamp` colormap row, because that row ends on paper white
and a pool that reaches it is a headlight rather than a lantern. The constant is in `voxfield.cpp`
twice on purpose — once as a float for `vx_dyn_lamp_at`, once as GLSL text for the world shader.
