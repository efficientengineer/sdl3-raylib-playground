# ENGINE.md — the map of `src/`

One screen. What each file owns, how you enter it, and which test would go red if you broke it.
The long-form record is `src/notes/`; the contracts are `story/v3/COMBAT.md` (combat) and
`story/field/` (maps and art). Every file opens with a 6-12 line banner saying the same thing.

## The three libraries

`libmain.so` (`host.cpp`) owns the window, the GL context and the hot reload. `libgame_logic.so` is
the game, and is what is swapped: it is the nine `star`/`game` files, the four `battle_*` files and
the ten `vox*` files. `libimgui_shared.so` is the one copy of ImGui and SDL3 they all share.

## The game — `libgame_logic.so`

| file | responsibility | entry points | covered by |
|---|---|---|---|
| `star_logic.cpp` | the `GameAPI` table, and the single STB_IMAGE / STB_IMAGE_WRITE TU | `get_game_api` | every suite |
| `game.cpp` | `game_create`/`tick`/`serialize`, the screen switch, the fades, title and end card, the capture and flag polls | `game_tick`, `game_create` | every suite |
| `cutscene.cpp` | the panel and talk player: textures, portraits and expressions, panel reveal, typewriter, pagination, the Narrator box | `draw_scene`, `star_goto`, `star_advance` | `--clips-selftest`, `--dialog`, `--robustness` H |
| `chapter.cpp` | the chapter's runtime in the field: enter/leave, the clip fade, the field's events, the battle hand-off, the goal line | `draw_field`, `enter_field`, `chapter_selftest` | `--chapter-logic-selftest`, `--chapter-playtest` |
| `dev_panel.cpp` | `dev_log.txt`, the Messages tab, the Dev window's scene/chapter/mood controls | `draw_dev`, `dev_send` | drawn every frame of `--robustness` |
| `settings.cpp` | `settings.ini`, the debounced write, `STAR_MUTE` | `settings_load`/`save`/`tick` | `--robustness` D |
| `audio.cpp` | the FM voices and the adaptive music sequencer | `au_play`, `au_update`, `mus_start` | by ear |
| `star_test.cpp` | `--clips-selftest`, `--battle-ui-test`, `--robustness`; `bui_tap` is the ONE way a test presses a button | `ct_drive`, `battle_ui_test`, `rb_drive` | itself |
| `playtest.cpp` | `--chapter-playtest`: the bot with a stick and two buttons. It never touches the chapter's memory | `pb_begin`, `pb_drive`, `playtest_*` | itself |
| `star_internal.h` | `Star`, `Screen`, the cutscene constants, and the cross-file declarations | — | — |
| `chapter01.h` | the chapter's **data**: steps, flags, binds, trigger conditions. Tables, never code | — | `--chapter-logic-selftest` |
| `cutscene_data.h` | **generated** by `./story_prompt.py export`. Never edit by hand | — | `--clips-selftest` |
| `field_text.h` | **generated**: examine and NPC lines | — | `--robustness` H |

## The battle screen — `story/v3/COMBAT.md` §1-§9 is the contract

| file | responsibility | entry points | covered by |
|---|---|---|---|
| `battle_rules.cpp` | §1 the tables (enemies, skills, encounters, party) and §2 the simulation; §4 the public API. Headless: no GL, no ImGui, no file IO | `bt_start`, `bt_tick`, `bt_resolve` | `--battle-selftest` |
| `battle_ui.cpp` | §3 the screen, the menu, the effort slider, and the `bt_ui_*` hit rects the UI test taps | `bt_draw`, `bt_input`, `bt_ui_*` | `--battle-ui-test` |
| `battle_script.cpp` | §9 Klee's phases and the phase-two restart | `bt_boss_script` | `--battle-selftest` |
| `battle_test.cpp` | `bt_selftest` and the `battle_tool` executable | `bt_selftest`, `main` | itself |
| `battle.h` / `battle_internal.h` | the public API / the private types and tables | — | — |

Later chapters add **rows** to `battle_rules.cpp`'s §1 tables, not code anywhere else.

## The world — `src/notes/world.md`, `movement.md`, `rendering.md`

| file | responsibility | entry points | covered by |
|---|---|---|---|
| `voxfield.cpp` | the module's façade: create/destroy/tick/save/restore | `vx_create`, `vx_tick` | `--vox-selftest` |
| `vox_world.cpp` | loading a `.tmap`, the voxel build, houses, trees, ramps | `vx_load_map` | `--vox-selftest` |
| `vox_palette.cpp` | the master palette and the colormap light tables | — | `--vox <map> --light` |
| `vox_mesh.cpp` | the greedy mesh and the shadow map | — | `--vox-selftest` |
| `vox_render.cpp` | the camera, the world shader, sprites, the HD-2D pass | `vx_draw` | `--vox <map>` image diff |
| `vox_nav.cpp` | the generated navmesh, erosion, reachability | `vx_nav_build` | `--vox-selftest` (nav), `--vox-walktest` |
| `vox_actors.cpp` | the move, jumping, facing, followers and NPCs | — | `--vox-walktest` |
| `vox_triggers.cpp` | the trigger queue: a cell may carry several, and they fire in order | — | `--chapter-playtest` |
| `vox_dev.cpp` | the field's Dev box and the debug overlays | `vx_dev_ui` | — |
| `vox_bot.cpp` | the movement bot | `vx_walktest` | `--vox-walktest` |
| `vox_internal.h` / `vox_shaders.h` / `voxfield.h` | the private types / the GLSL / the public API | — | — |

## Who includes whom

```
host.cpp ──> game_api.h                         (the only thing crossing the .so boundary)

star_logic.cpp ─┐
game.cpp        │
cutscene.cpp    │
chapter.cpp     ├──> star_internal.h ──> game_api.h, cutscene_data.h, chapter01.h,
dev_panel.cpp   │                        field_text.h, dialogue.h, voxfield.h, battle.h
settings.cpp    │
audio.cpp       │
star_test.cpp   │
playtest.cpp   ─┘

battle_rules.cpp ─┐
battle_ui.cpp     ├──> battle_internal.h ──> battle.h, dialogue.h, field_text.h
battle_script.cpp │
battle_test.cpp  ─┘

voxfield.cpp, vox_world, vox_palette, vox_mesh, vox_render, vox_nav,
vox_actors, vox_triggers, vox_dev, vox_bot ──> vox_internal.h ──> voxfield.h
                                    vox_render.cpp also ──> vox_shaders.h
```

No `.cpp` includes another `.cpp`'s header directly: each group has exactly one internal header, and
the three groups meet only through `battle.h`, `voxfield.h` and `game_api.h`.

## The rules the split is held to

- **The source list lives twice and must match**: `CMakeLists.txt`'s `game_logic` target and
  `fast_reload.sh`'s `GAME_SRCS` loop. `battle_tool` compiles the four `battle_*.cpp` files with
  `BATTLE_TOOL_MAIN`.
- **The behaviour lock**, run before and after any move: `./capture.sh --chapter-playtest`,
  `--chapter-logic-selftest`, `--clips-selftest`, `--battle-selftest`, `--battle-ui-test`,
  `--vox-selftest`, `--vox-walktest all`, `--robustness`, plus byte-identical image diffs of
  `--vox halm --at 24,18`, `--vox high_pasture --light night:0.16 --lantern` and one `--dialog`.
- Aim for **≤1,200 lines a file**; nothing over 1,500. A file past that wants splitting, not a
  smaller font.
