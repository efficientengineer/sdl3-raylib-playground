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
