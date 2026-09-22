# The battle screen — the four files, and where each rule lives

> Written 2026-09-21 with the `battle.cpp` split. `story/v3/COMBAT.md` §1-§9 is the **contract** and
> beats this file wherever they disagree; `story/v3/BESTIARY.md` is the enemy design. This is only
> the engine's map of where each of those rules is implemented.

`src/battle.h` is the public API — the only thing `chapter.cpp` and `game.cpp` may call.
`src/battle_internal.h` is private to the four files below and carries the types (`Battle`,
`BtEnemyState`, `BtActorState`), the §1 table declarations, and the round written out in full.

| file | owns | never |
|---|---|---|
| `battle_rules.cpp` | §0 palette + RNG, §1 the enemy/skill/encounter/party **tables**, §2 the turn logic, §4 the public API | draws, reads input, touches GL or ImGui |
| `battle_ui.cpp` | §3 the screen: sprites, bars, tell and OPEN markers, the menu, the effort slider, the skill list, the log, the banner, the keyboard, the `bt_ui_*` hit rects, the Dev box | changes a rule; every number it shows comes from §2 |
| `battle_script.cpp` | §9 Klee's two phases, the scripted break and turn, the phase-two restart | decide an ordinary round |
| `battle_test.cpp` | §5 `bt_selftest` and §6 the `battle_tool` executable (`BATTLE_TOOL_MAIN`) | ship a behaviour: nothing calls into it |

**Later chapters add ROWS to §1, not code anywhere else.** An enemy, a skill or an encounter is a
line in a table in `battle_rules.cpp`.

## The things that are easy to get wrong

- **The simulation must stay headless.** `bt_headless` is the switch, and the separation is a
  requirement rather than a nicety: `bt_selftest` drives `battle_rules.cpp` with no window, no GL
  and no file IO. Anything that needs a texture or an `ImGui` call belongs in `battle_ui.cpp`.
- **One command list, built in one place.** `bt_build_rows` is it. `bt_tick` and `bt_draw` both ask
  it, because a disagreement would mean the tap committing a different command from the one under
  the finger. It also guarantees the list is never dead: Guard is free, so the moment nothing else
  is affordable Guard is granted whether or not the teaching order has reached it.
- **The watchdog.** Anything that is not an input-accepting state is a presentation state, and one
  that has run for `BT_STUCK_S` is a bug: it is named in the log and forced forward. `stuck_fires`
  is asserted at zero by `--battle-ui-test`.
- **Enemy sprites** come from `story/field/sprites/<id>.png` (indexed on the master palette, index 0
  transparent, anchor bottom-centre, 64 px to the cell, a single still unless a `.json` sidecar says
  otherwise), then `story/field/walkers/<id>.png` as the three-column walker layout, then a flat
  palette silhouette. Art never blocks a fight.

## The tests

| command | what it drives |
|---|---|
| `./capture.sh --battle-selftest` | `bt_selftest`, headless: every rule in COMBAT.md §1-§9 |
| `./capture.sh --battle-ui-test` | the real screen driven by injected taps (`src/star_test.cpp`), watchdog asserted at zero |
| `./capture.sh --robustness` | F: 2,000+ random taps across every encounter in `BT_ENCOUNTERS` |
| `./capture.sh --chapter-playtest` | the bot fights through the real menu (`src/playtest.cpp`, `pb_plan_turn`) |
| `./build_desktop/battle_tool --capture <enc>[:<pose>]:<w>x<h>:<out.png>` | one still of the screen |

The play-test's battle policy and the four things that once stopped it are recorded in
`src/notes/testing.md`, under "Where it stops".
