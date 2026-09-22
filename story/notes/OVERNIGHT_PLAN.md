# MORNING REPORT (2026-09-21, written by the orchestrator at the end of the unattended session)

Everything below is committed and on GitHub (`master` and `main`, latest `b37fb95`). **Nothing was pushed
to the phone.** The build on the phone is still the morning's slice. To play the new one: say the word.

## What is on the Mac now — verified by me, not just reported

All eight test suites pass under my own runs, and every game source cross-compiles for Android:
- `--chapter-playtest` — a bot with a stick and two buttons plays the WHOLE chapter on the real maps
  through the real menus, twice: a completionist (every hidden item by its designed jump) and a lazy
  player (goal line only, talks to nobody). Both reach the end card with all ten mandatory interactions.
- `--clips-selftest` (all six scenes end under tapping, every panel revealed once), `--battle-selftest`,
  `--battle-ui-test` (66 tapped commands), `--vox-selftest` (incl. colormap identity 255/255),
  `--vox-walktest all` (5 maps), `--chapter-logic-selftest`, `--robustness` (save/continue at all 26
  steps, hot-reload mid-clip and mid-battle, settings, 3k-tap battle fuzz, all 66 text ids fit the box).

## Fixed today
- Battle soft lock after attacking on touch (frame-order bug) + 3 more touch bugs; Guard always available;
  watchdog; battle logging; Dev "next step" walks the chapter; party per step (Falke alone vs machines).
- 9 blockers from the consistency audit that the old tests could not see: bed/signing/body triggers,
  day-one job sheet, Ottilie joining a day early, the bell run (now built with roofs and a wall to jump),
  both hidden-item routes, day-one swing win, 27 unreachable text lines placed. Plus six bugs the bot
  found (sealed grain yard, overlapping triggers, examine ignoring height, comments parsed as map data…).
- Night lantern: was neon green — the colormap was refitted to palette indices and lamp light saturated
  instead of bleaching. Fixed in the tool; the pool is a warm straw circle now (I looked).
- Chapter one text: four actors + two audits + editor merge + cold read + polish → draft 4.1: 85 boxes,
  every line tagged with picture and expression, Ottilie never coaches, her family shown not told, Distel
  a furred Mohn, plainer Hart, the parry's cause shown in a picture, The Counter now a 3-panel scene.
- Context-friendly: story_prompt.py → `storytool/` (49 modules, largest 636 lines, golden harness proves
  identical output); voxfield.cpp (6.5k) → nine vox_*.cpp files; CLAUDE.md has a task→files entry map and
  your standing rules; 13 stale doc references fixed; tray statuses have one source (stale shown red).

## Owner rulings — ANSWERED 2026-09-21: title *The Sheep on the Hill*; Hart's hint and Falke's reply restored; bell run 20 s is fine; cycling stays off; WINDED stays; names and handover fine.

## (the questions as asked)
1. Chapter title: "The High Pasture" (alternates: The Sheep on the Hill / The Hunt on the High Pasture).
2. Hart's supper hint was cut (it gave away the swing); it took Falke's "That's how you hit things!" with
   it. Keep the cut, restore both, or rehome Falke's line?
3. Bell run: Halm is 48x36, so the run is 20 s real time, not the spine's "six minutes". Fine, or a bigger Halm?
4. Colour cycling: the art uses the "reserved" palette entries (Hart's portraits ~12% on the fire range).
   Cycling stays OFF. Turning it on later = a palette version bump + re-derive every image (no redrawing).
5. Stamina: a "winded" state pauses regeneration until you Guard (needed so the swing can teach the parry).
6. "Mohn" as its own plural; "drover" for Klee's kind; the plainer one-handed sword handover.

## Not done / carried
- star_logic.cpp (3.2k) and battle.cpp (2.2k) still unsplit — the agent was cut off by an API outage.
- Headless Dev-button sweep and a 5x memory soak: not written.
- The Mac cannot make a portrait window; that shape is untested.
- Faint red banding on stone risers under the night table on the pasture; hill_path still reads terraced.
- Art tray queue (21 open): Falke + Ottilie walkers are STALE (old designs); Guildclerk/Stolz/Garbe
  expressions, six walkers, three field-sprite sheets and five scene shot sheets to generate.
  Reference sheets: all done except Ottilie (exists; redo only to lose the mace).

---

# While the owner is at work (2026-09-21) — the orchestrator's plan and log

Owner: "spend the rest of my usage thoroughly testing everything in game, making sure the codebase and
files are context friendly, and running multiple agents looking for any gaps in logic or
inconsistencies, and running actors for each character so their dialogue reads well, and eliminate all
ai smells." Nothing is pushed to the phone while the owner is away.

## Wave 1 (parallel, exclusive files)
- engine: battle soft lock on touch + Dev "next step" + battle logging + watchdog (src/**)
- actors (read-only → story/notes/actors/): Falke, Ottilie, Hart + townspeople, Distel
- auditors (read-only → story/notes/audit/): consistency across story/play/combat/maps/engine; AI smells
- tool: split story_prompt.py into a context-friendly package behind a golden-output harness

## Wave 2 (after wave 1 reports)
- story editor: ONE agent merges actors + smells + story-side consistency fixes into scenes/field text,
  tags expressions, regenerates the script; then a fresh cold read
- engine: engine-side consistency fixes; thorough play testing (bot that WALKS the whole chapter on the
  navmesh with simulated touch, battle UI fuzz, save/continue, hot reload mid-battle, soak/leak/perf on
  the Mac); split voxfield.cpp (6,239 lines), battle.cpp, star_logic.cpp into context-friendly modules
  with an ENGINE index; hill_path's 13 jump-only regions and its terraced look

## Wave 3
- orchestrator re-runs every test, commits, writes the morning report at the top of this file
