# Chapter one — consistency audit (story / design / combat / maps / engine)

Read-only pass, 2026-09-21. Five layers checked against each other: STORY (`story/scenes/01*.md`,
`chapter01_script.md`, `story/field/text.md`, `characters.md`, `v3/NAMES.md`, `v3/THREADS.md`),
PLAY DESIGN (`v3/chapter01.md`, `v3/ch01_room/flow.md`), COMBAT DESIGN (`v3/COMBAT.md`,
`BESTIARY.md`, `LOOT.md`), MAPS (`story/field/tmaps/*.tmap`, `sprites.md`, `WORLD.md`), ENGINE
(`src/chapter01.h`, `src/battle.cpp`, `src/voxfield.cpp`, `src/cutscene_data.h`).

`./story_prompt.py check --all` and `names` both pass — every failure below is a cross-layer one the
validators do not look for.

**Counts: 9 BLOCKER, 21 MAJOR, 16 MINOR.**

## Ranked findings

| # | Sev | Layers | What's wrong | Evidence | Cheapest fix (which side changes) |
|---|---|---|---|---|---|
| 1 | BLOCKER | engine ↔ map | `F_BED` gates the P3 step, but **no map triggers `hart_yard.bed`**. The chapter cannot leave day one in real play. | `src/chapter01.h:112-113` (`{ F_BED, -1 }`), `:156` bind `hart_yard.bed`; `story/field/tmaps/hart_yard.tmap:98-113` has no bed trigger | MAP: add `message hart_yard.bed` on the bed cell in hart_yard (and `room_sword_gap` beside it) |
| 2 | BLOCKER | engine ↔ map | `F_SIGNED` gates P5, but **no map triggers `halm.clerk_signing` / `_late`**. The bell run can never complete. | `src/chapter01.h:128-129`, `:163-164`; `halm.tmap:133-156` — no clerk npc/trigger at the counter | MAP: add `npc clerk ... halm.clerk_signing` at the guild-hall counter (34-35, 20). ENGINE must also pick `_late` when `ch_bell_late()`; nothing does today |
| 3 | BLOCKER | engine ↔ map | `F_BODY` gates the last PLAY step before C6, but **no map triggers `high_pasture.body`**. The chapter cannot reach its end card. | `src/chapter01.h:144`, `:167`; `high_pasture.tmap:177-193` — no body trigger | MAP: add `message high_pasture.body` on/next to the `fight klee` rect (33 13 8 7), enabled after the fight |
| 4 | BLOCKER | map ↔ design ↔ engine | `halm.job_sheet` fires from a plain walk-on trigger at the board on **day one**, when the spine says the board is bare and `halm.board_closed` is what the player reads. It also sets mandatory #6 out of order, and P2's step can complete with day-two content already read. | `halm.tmap:137` `33 20 1 1 message halm.job_sheet`; `chapter01.md:167-168, 316-323`; `text.md:157-160` (`board_closed`, day one) vs `:225-232` | MAP+ENGINE: two triggers on the board, one consumed per step (`vx_disable_trigger`), or gate `job_sheet` on `F_SWING_BEATEN` |
| 5 | BLOCKER | map ↔ engine ↔ story | `halm.ottilie_door` fires on a walk-on cell (41 22) that is **on the day-one lane home**, right beside `halm.ottilie_house` (42 22). Asking her along and her joining the party (`bt_party_init(&c->party, 2)`) can happen on day one, before the sword and before the job exists. Her line reads "I signed for a job … I'm going tonight." | `halm.tmap:142-143`; `src/chapter01.h:287`; `text.md:248-256` | MAP/ENGINE: make the door trigger a step-gated one (only live once `F_JOB_SHEET`), or move `ottilie_house` off the door cell and disable the door trigger until P5 |
| 6 | BLOCKER | map ↔ design | The P5 bell run's **entire design is unimplemented**: `halm.tmap` has **no `## height` section**, so there are no roofs, no grain-yard wall to jump, no well-kerb shortcut. The direct route is the only route, and the spine says the direct route does not make it. | `chapter01.md:303-309`; `grep "## height" story/field/tmaps/*.tmap` → only `hill_path`, `high_pasture` | MAP: author height on halm (roof line, grain-yard wall, trough), or DESIGN: drop the shortcuts and re-tune `CH_BELL_RINGS` |
| 7 | BLOCKER | map ↔ loot | `{{LOOT_YARD}}` is specified as a three-cell running jump from the workshop roof; on the map it is a **flat ground pickup on the ladder cell**, and `hart_yard.tmap` has no height at all. The chapter's first platforming lesson does not exist, and P5's jumps are "already taught by the ladder". | `LOOT.md:19`; `hart_yard.tmap:103-104`; no `## height` in that file | MAP: add height + a roof; or LOOT: restate the find as reachable from the ladder (then P5 loses its teaching) |
| 8 | BLOCKER | map ↔ loot ↔ design | `{{LOOT_PASTURE}}` must be **night-sight only, after C5, on a jump-only shelf above the fold**. The map places it as an ordinary pickup at 35 4 with **no `nightsight` flag** and nowhere near the fold (29 22), so a player takes it in P7 before Distel joins. | `LOOT.md:21`; `chapter01.md:421-424`; `high_pasture.tmap:187-188`; engine supports the flag, `voxfield.cpp:1048-1053` | MAP: move the pickup onto the shelf above the fold and add the trailing `nightsight` word |
| 9 | BLOCKER | engine ↔ design | P1's gate is `F_SWING_TRIED`, set by **any** swing fight — but `ch_on_battle` also sets `F_SWING_BEATEN` if the player happens to win on day one, which completes P4's gate before day two exists. P1 is explicitly "you do not win today". | `src/chapter01.h:311-316`; `chapter01.md:105-112` | ENGINE: ignore a day-one win (only set `F_SWING_BEATEN` when `step >= P4`), or make the swing unwinnable while `!ch_has(F_BED)` |
| 10 | MAJOR | story ↔ design | **31 of 66 field-text ids have no trigger and no step** — a third of the writer's draft-three text is unreachable, including every supper box, both day-two lines, both job-sheet boxes' continuation, the whole board/market/town-healer layer, all four boss in-field exchanges and both body boxes. Full list below. | `story/field/text.md` vs `story/field/tmaps/*.tmap` | MAP mostly: place the triggers. ENGINE for the four boss ids (they are `BTE_TEXT` events, `battle.h:62-63`) |
| 11 | MAJOR | combat ↔ engine | `high_pasture.boss_break` / `_turn` (+`_2`) are written and staged as in-field exchanges but nothing fires them: no map trigger, no wiring from `BTE_TEXT` to the field text box in `star_logic.cpp`. | `chapter01.md:451-459`; `text.md:334-352`; `src/star_logic.cpp:993` handles `BTE_GOAL` only | ENGINE: handle `BTE_TEXT` by showing the named field line |
| 12 | MAJOR | combat design ↔ engine | **Ottilie's pool: 24 vs 36.** COMBAT §3/§5 says 36 and the engine uses 36 (with a comment ruling on it); `chapter01.md` P8 and `BESTIARY.md` still say 24 and build phase two's tension on it. | `COMBAT.md:55,169`; `battle.cpp:146-153`; `chapter01.md:463`; `BESTIARY.md:173` | DESIGN: strike 24 from `chapter01.md:463` and `BESTIARY.md:173` |
| 13 | MAJOR | combat design ↔ engine | **Distel cannot afford Settle three times.** Design says "24 stamina a go, she can afford about three"; her pool is 30 with +8/round regen, so it is once, then a long wait. Phase one is three cycles. | `chapter01.md:444-449`, `COMBAT.md:245-252`, `BESTIARY.md:162`; `battle.cpp:154` (`distel … 30`) | DESIGN or ENGINE: raise her pool to ~50, or say "twice, and the second one hurts" |
| 14 | MAJOR | combat design ↔ engine | The engine invented a **WINDED** state (cancels the +8 regen for 1+ rounds, cleared by guarding) — the actual mechanism by which §4a.3 "closes the door". COMBAT.md never mentions it; drain is `5 + 4*wind`, also undocumented. | `battle.cpp:343-349, 572-573, 715-726, 775-782`; `COMBAT.md:§3, §4a.3` (no "winded") | DESIGN: write WINDED into COMBAT §3 and §4a.3 with the numbers |
| 15 | MAJOR | design ↔ engine | Boss phase-one **goal line is two different lines and one is 7 words**: spine and engine say "Hold it open for {{HERDER}}."; COMBAT §9 and BESTIARY say "Hold it open so {{HERDER}} can calm it." (7 words, breaks the six-word rule stated in the same files). | `chapter01.md:444` / `battle.cpp:280` vs `COMBAT.md:245`, `BESTIARY.md:160` | DESIGN: delete the long form in COMBAT/BESTIARY |
| 16 | MAJOR | design ↔ map | **Grain-yard lid count: three by design, nine on the map.** Three `fight lid` triggers, each spawning an encounter of three lids. | `chapter01.md:181-183`, `BESTIARY.md:26`; `halm.tmap:146-148`; `battle.cpp:272` (`{"lid",{"lid","lid","lid"}}`) | MAP: keep one `fight lid` trigger; or ENGINE: add a single-lid encounter |
| 17 | MAJOR | design ↔ map ↔ engine | Hill path: design says ones and twos low, **three near the top**; the map uses five single-burr encounters and the `burr3` encounter is an **orphan** nothing references. | `chapter01.md:353-355`; `hill_path.tmap:182-186`; `battle.cpp:274` | MAP: make the top two encounters `burr3` |
| 18 | MAJOR | map ↔ design | The false lantern (mandatory 8) sits at **5 5**, a far corner, while the sleeper trail runs 22 39 → 27 22 and `tracks_stop` is at 12 26. The set piece is not on the route and the P7 gate needs both flags in **either** order, so C5 (Distel standing at the tracks) can fire after the player has already been to the corner, or the player can hit the tracks first and then be sent back across the dark map. | `chapter01.md:388-402`; `high_pasture.tmap:180-186` | MAP: put the lantern set piece between sleeper 3 and the tracks. ENGINE: split the P7 row into two steps so the order is fixed |
| 19 | MAJOR | story ↔ design | **C6 describes the holes a second time.** The spine forbids it (the player found them in `high_pasture.body_2`); Ottilie's line restates size, number and age. | `chapter01.md:475-476`; `text.md:328-330` vs `0180_what_was_on_it.md:35` | STORY: cut or re-aim the line ("Who does that to a dog?" etc.) |
| 20 | MAJOR | story ↔ story | **The job pays four coin in `characters.md` and in `beats.md`, five everywhere else** (sheet, C6, THREADS, chapter01.md). | `characters.md:113`; `ch01_room/beats.md:43,94,105` vs `text.md:230`, `0180:41`, `chapter01.md:319` | STORY: fix `characters.md`; DESIGN: `beats.md` is superseded, mark it |
| 21 | MAJOR | story ↔ design ↔ combat | **"Four training machines" survives in `characters.md`**, and `flow.md` numbers them ("machine 4") twice — both against "there are three, named never numbered". | `characters.md:95`; `ch01_room/flow.md:80,108` vs `COMBAT.md:197-203`, `BESTIARY.md:106-109` | STORY/DESIGN: three, by name |
| 22 | MAJOR | design ↔ engine | **P4 is four sessions in the spine, three PLAY rows in the engine**, and the light sequence disagrees: spine dawn → sun up → high sun → low gold; engine night 0.35 → **dusk 0.75** → day 1.00. The win therefore lands in full daylight while C4's panels and beat are "the last light of the second day". | `chapter01.md:254-259`; `src/chapter01.h:116-121`; `0150_the_sword.md:4,11` | ENGINE: four rows, ending `dusk`/low gold |
| 23 | MAJOR | map ↔ art ↔ engine | **Sprite ids do not match.** Maps and `battle.cpp` want `post`, `arm`, `swing`, `lantern`; `sprites.md` defines `machine_post`, `machine_arm`, `machine_swing`, `false_lantern`. Nothing can ever be found. (`story/field/sprites/` does not exist yet, so every fight draws a placeholder.) | `sprites.md:88,93,98,54`; `hart_yard.tmap:106,108,110`, `high_pasture.tmap:186,193`; `battle.cpp:204-215` | ART: rename in `sprites.md` to the engine's ids (the engine's are the contract in `battle.h:73-77`) |
| 24 | MAJOR | map ↔ art | `hart_yard.tmap` places `npc hart`, but there is no `story/field/walkers/hart.png` and **no Hart (or Distel, or Garbe, or Clerk) walker anywhere**; only falke, ottilie, villager_a/b exist. Hart is on screen in P1–P4. | `check --all` warning; `ls story/field/walkers/` | ART: queue hart, distel, garbe, clerk walkers |
| 25 | MAJOR | design ↔ map | P1 says "no exits open yet", but `hart_yard.tmap:113` exits to halm from the first minute and nothing gates it, so the player can walk into day-two Halm during P1 with a goal line about a swing. | `chapter01.md:92`; `hart_yard.tmap:113`; no gating in `ch_apply_step` | MAP/ENGINE: block the gate until `F_SWING_TRIED` |
| 26 | MAJOR | story ↔ design | **Bread is bought before the money for it exists.** `halm.bread` says a coin a loaf, three loaves; the three coin arrive from Stolz in C2, which fires after the guild hall. The only earlier coin is one lid drop. | `text.md:139-142`; `0130_the_counter.md:26`; `chapter01.md:166` | STORY: make the baker line a slate/"on Hart's account", or move the three coin earlier |
| 27 | MAJOR | story ↔ threads | **THREADS promises the chapter does not keep on screen.** "Two people say so out loud" about the Mohnen (only `halm.lamp_charm_2` is reachable; `halm.shepherd_market` is dead); the Linde/full-standing thread is `halm.town_healer`, dead; the Stolz three-coin debt is `halm.rival_door`, dead. | `THREADS.md:24-25,34,35-36`; dead-id list below | MAP: place those three triggers (they are already written), else strike the threads |
| 28 | MAJOR | story ↔ design | `flow.md` is stale in four places the writer may still read as the contract: C1's back-into goal "Take the repaired part to the guild hall." (8 words, and the spine says *Deliver the part. Buy bread.*); C2 "the clerk states two rules… then the third" (the spine cut it to one); "80 written ids" (66); the defence of a "lost forty-one times" line that no longer exists in `0150`. | `flow.md:81-82, 88-89, 141, 149-151` vs `chapter01.md:42, 193-198`, `text.md`, `0150_the_sword.md` | DESIGN: reconcile `flow.md` to the spine |
| 29 | MAJOR | story ↔ art | **Distel carries a crook in four panels and no crook in her `look`** (horn and coiled lead only), so the generator will draw the reference-sheet props and the panels will disagree. | `0160_the_herder.md:15,24,29`, `0180:20`; `characters.md:102` | STORY: add the crook to the `look` line (it is the truth of the panels) |
| 30 | MAJOR | story ↔ engine | The clerk's two signing lines exist but nothing chooses between them: `ch_bell_late()` is never read outside the Dev panel, and both ids bind to the same flag. Late/on-time is currently invisible. | `src/chapter01.h:163-164, 338`; `text.md:233-241` | ENGINE: pick the id by `ch_bell_late()` when the counter trigger fires |
| 31 | MINOR | story ↔ names | `text.md` writes Distel's speaker as the literal `Distel` (three lines) where every other speaker is a token; a rename breaks it silently. | `text.md:336,341,346` | STORY: `- name: {{HERDER}}` |
| 32 | MINOR | story ↔ scenes | Scene files use the literal `Distel` and `Garbe` as speakers and in panel text while the rest is tokenised. Legal (they are handles) but inconsistent with `{{HERO}}`/`{{HEALER}}` beside them. | `0160_the_herder.md:36-48`, `0180:32-41` | STORY: tokenise for consistency |
| 33 | MINOR | story ↔ design | Scene 0180's title is "What Was On **Him**" and its text calls the beast "he"; the spine, the flag and the file name say "it". | `0180_what_was_on_it.md:1`; `chapter01.md:470` | STORY: the scene is right (Distel's "him"); retitle the slot in the spine |
| 34 | MINOR | story | `chapter01_script.md` claims "86 boxes"; the six scenes hold 88 lines. | `chapter01_script.md:7` | STORY: regenerate |
| 35 | MINOR | design ↔ engine | The spine's P3 is day → dusk → night with supper at sundown; the engine puts the whole hart_yard supper/bed step at `night 0.40`, so the player arrives at a dark house at "sundown". | `chapter01.md:205`, `0140_supper.md:5` vs `src/chapter01.h:112` | ENGINE: dusk row, then night at bed |
| 36 | MINOR | engine | `ch_jump` rebuilds the party from flags but never re-applies carried items or `known` bits, so a Dev jump into P8 fights the boss with a party that never learned Guard/effort unless the encounter grants them (klee does; nothing else would). | `src/chapter01.h:271-280`; `battle.cpp:278-280` | ENGINE: grant `known` by step in `ch_jump` |
| 37 | MINOR | design ↔ engine | The four hidden finds never gate anything and are never checked at the end card, but `LOOT.md` calls the teaching find "the chapter's contract". Nothing tells a player they missed it. | `src/chapter01.h:47-51`; `LOOT.md:10-14` | none needed — noted so it is a choice, not an oversight |
| 38 | MINOR | story | `0110`'s reveal order is 1, 3, 4, 2 — panel 2 (Ottilie mid-laugh on the wall) appears after the impact and the menace shot. Legal, but the page builds backwards against its own layout. | `0110_the_yard.md:31-34` | STORY: optional |
| 39 | MINOR | design | `designer_to_writer.md` §2 still asks for 15 ids that draft three deleted or renamed (`halm.carter_saying`, `hart_yard.errand_1`, `halm.npc_1/2/5`, `hart_yard.supper_3`, `high_pasture.ottilie_ready`, …). | `ch01_room/designer_to_writer.md` vs `text.md` | DESIGN: mark the file superseded by `chapter01.md`'s mandatory ten |
| 40 | MINOR | build/docs | `fast_reload.sh:105,131` still cite `FIELD.md` and `TILES.md`, deleted by D24 and folded into `WORLD.md`; `story/field/tmaps/README.md:3` and `story/field/tilesets/README.md:3,24` do the same; `story_prompt.py:5202` cites `src/TILEFIELD_NOTES.md`, which no longer exists (the engine note is `src/VOXFIELD_NOTES.md`). | as cited; `DECISIONS.md:308` | DOCS: point them at `WORLD.md` / `VOXFIELD_NOTES.md` (the scripts are the engine agent's to edit) |
| 41 | MINOR | docs | `DECISIONS.md:70` still describes New Game as playing the `## intro` list, which D24 deleted 240 lines further down the same file. | `DECISIONS.md:70` vs `:314`, `playlist.md:3-5` | DOCS: mark D-entry superseded |
| 42 | MINOR | story | `NAMES.md` keeps a full live-reading bio for `{{FATHER}}` Rabe (the water run, twenty years, died eleven years ago) although BRAINSTORM's no-sunk-cost note and THREADS both retire him; `STYLE.md:16` quotes that line as a model sentence. | `NAMES.md:15,152-154`; `THREADS.md:7-11`; `BRAINSTORM.md:143` | STORY: reduce the row to "unused, chapter two+" |
| 43 | MINOR | design | The west road is "gone with the road" in BESTIARY/LOOT, but `west_road.tmap` is still an exit target from `halm.tmap:154` and keeps two examine lines; a P2 player can walk out of the chapter into an unwritten map. | `halm.tmap:154`; `west_road.tmap`; `text.md:356-364`; `BESTIARY.md:3-6` | MAP: close the west exit for chapter one |
| 44 | MINOR | maps | 30+ `check --all` warnings: every solid stamp (`tree`, `boulder`, `wall_*`, `gate_post`, `hut`, `haystack`, `practice_post`, `signpost`, `crate`, `barrel`, `hedge`) leaves 24-75% of a blocked tile transparent — the player sees walkable ground inside a solid cell on every map. | `./story_prompt.py check --all` | ART or MAP: shrink the footprints, since the atlas is frozen |
| 45 | MINOR | design/engine | `CH_BELL_RINGS 12 × 9 s = 108 s` for the run, against a designed 6 minutes "including the board and the door". With no shortcuts (finding 6) the number is untestable either way. | `src/chapter01.h:206-207`; `chapter01.md:295-331` | ENGINE: re-tune after finding 6 |
| 46 | MINOR | story | `hart_yard.practice_posts`'s `- what:` note says "kept because hart_yard.tmap triggers it" — it does not. | `text.md:54-56`; `hart_yard.tmap` | STORY: delete the line or place the trigger |

## Dead text ids (written, no map trigger, no step — 31 of 66)

`hart_yard.errand`, `hart_yard.practice_posts`, `hart_yard.book`, `hart_yard.supper_1`,
`hart_yard.supper_2`, `hart_yard.room_sword_gap`, **`hart_yard.bed`**, `hart_yard.day2_b`,
`hart_yard.day2_d`, `halm.lamp_charm_2`, `halm.board_closed`, `halm.shepherd_market`,
`halm.shepherd_market_2`, `halm.healer_bench`, `halm.town_healer`, `halm.gate_watch_after`,
`halm.marta_after`, `halm.ostler_after`, `halm.job_sheet_2`, **`halm.clerk_signing`**,
`halm.clerk_signing_late`, `halm.rival_door`, `halm.ottilie_door_2`, `hill_path.ottilie_dark`,
`hill_path.ottilie_dark_2`, **`high_pasture.body`**, `high_pasture.body_2`,
`high_pasture.boss_break`, `high_pasture.boss_break_2`, `high_pasture.boss_turn`,
`high_pasture.boss_turn_2`.

Note: `_2` continuation boxes are expected to follow their first box, so nine of these are only dead
because the first box is. The bold three are the blockers (findings 1-3). No trigger anywhere points
at a missing id — the error is entirely one-directional.

## Dead / undefined tokens

- Defined and **unused anywhere** (35): `{{VILLAIN}}`, `{{VILLAGER_1}}`, `{{CARETAKER}}`, `{{STAIR}}`,
  `{{SECOND_MOON}}`, `{{THE_DOOR}}`, `{{SEA_WALL}}`, `{{COAST_ROAD}}`, `{{GRAIN_CREATURE}}`,
  `{{THIEF_CREATURE}}(_PL)`, `{{AMBUSH_CREATURE}}(_PL)`, `{{STALKER_CREATURE}}(_PL)`,
  `{{ROAD_CREATURE}}(_PL)`, `{{RUIN_CREATURE}}(_PL)`, `{{CAVE_CREATURE}}(_PL)`,
  `{{SHRINE_CREATURE}}(_PL)`, `{{GUARD_BEAST_PL}}`, `{{HILL_CREATURE}}(_PL)`,
  `{{NIGHT_CREATURE_PL}}`, `{{PASTURE_CREATURE}}(_PL)`, `{{LOOT_ROAD}}`, `{{LOOT_SECOND_SWORD}}`,
  `{{LOOT_SHRINE}}`, `{{ANCIENTS}}`, `{{GUILD}}`, `{{DRY_CITY}}`.
  Harmless (they are chapter-two stock) **except** `{{HILL_CREATURE}}`, `{{PASTURE_CREATURE}}` and
  `{{NIGHT_CREATURE_PL}}`, which chapter one's own design text uses in the singular/plural the table
  does not carry — the creatures are in play but their names never reach a player.
- No token is used without a row (`names` is clean).
- `{{HIGH_PASTURE}}`, `{{CARETAKER}}`, `{{STAIR}}`, `{{SECOND_MOON}}`, `{{THE_DOOR}}`, `{{SEA_WALL}}`
  still read as "*(no name yet)*" placeholders; `{{HIGH_PASTURE}}` is on screen four times in
  chapter one as the plain phrase "high pasture".

## Stale references, by file

- `fast_reload.sh:105` — "Field maps and art (FIELD.md)"; `:131` — "Tilesets (TILES.md)". Both files
  deleted by D24 (`DECISIONS.md:308`).
- `story/field/tmaps/README.md:3` — "the plain-text format `TILES.md` fixes" → `WORLD.md`.
- `story/field/tilesets/README.md:3,24` — `TILES.md` as the decision of record.
- `story_prompt.py:5202` — `src/TILEFIELD_NOTES.md` (the module is `src/voxfield.*`,
  `src/VOXFIELD_NOTES.md`).
- `story/DECISIONS.md:70` — New Game plays the `## intro` list (gone, `:314`).
- `story/v3/NAMES.md:15,30,31,152-154` — Rabe, the water run, the Stair and the caretaker written as
  live world facts; `story/v3/STYLE.md:16` quotes the Rabe line as a model.
- `story/v3/chapter01.md:3` — "replacing *The Jar Run*"; fine as history, but it is the only live
  mention of the jar outside BRAINSTORM.
- `story/characters.md:95` — "the four training machines"; `:113` — the job "pays four coin";
  `:68` — Falke's role line still reads "hunter of the Even Hand, **axe**" (he is given a sword in
  C4 and the guild is unnamed in v3); `:79` — Lyra's role "mace" is fine but sits beside a v3 entry.
- `story/v3/ch01_room/flow.md:80,108` — "machine 4"; `:60` — "Gone with the road" while `halm.tmap`
  still exits to `west_road`.
- `story/v3/ch01_room/beats.md:43,94,105` — four coin, and the pre-revision job sheet.
- `story/field/text.md:356-364` — two `west_road` lines kept only so a map that chapter one should
  not reach validates.
- `story/v3/PREMISE.md:97,103,132,136,157` — the sealed jar, the water run and "Carry the jar to the
  {{STAIR}}" still stand as Act I beat 1; `:152,154` the woman under the Stair. D23 demotes PREMISE
  to a pool of ideas, but nothing in the file says its chapter one is dead.
- `story/v3/BRAINSTORM.md:59,64,72,90,126,127` — the jar, the water run, Hart's thirty coin and "came
  down the Stair" survive in the file whose own header says it is emptied once written.
- `story/v3/STYLE.md:312` — the worked craft example is a Stolz line about the jar (`You'll be up all
  night with a jar`); the real line is now "three coin for your bread" (`0130_the_counter.md:26`).
- `story/v3/ideas_interludes.md:164` — an interlude premised on the party "just come from the
  {{STAIR}}".
- `src/voxfield.cpp:91` — `VX_MAPS` still ships `west_road`; `capture.sh` and
  `src/VOXFIELD_NOTES.md` (nine places) use it as the standing benchmark map, which is what keeps
  the cut map alive.
- `story/DECISIONS.md:173` (`Contract in FIELD.md`), `:192` ("painted block-outs stay available where
  exact layout matters (the Stair)" — two dead things in one live-sounding commitment), `:213`
  (`New engine module src/tilefield.*`, which no longer exists).
- `story_prompt.py:2888-2892` — the painted-view rationale still reads as current direction although
  the tool's own header (`:81`) and D24 retired it.
- `story/field/walkers.md:7,9` and `story_prompt.py:25,26,30,32` — four help lines teach `Bron` as
  the name to type; the current name is Falke (the handle is correct, the examples mislead).
- `story/characters.md:76,85` — v1 backstory and fate (Bron Sallow, template 4, "shot in the road at
  Tellwater in ch.11") still sit unmarked on the two entries whose v3 identity is set below them.
- **Not stale:** the repo's own `CLAUDE.md` (197 lines) is the post-D24 rewrite and clean; the long
  tile-field/FIELD.md/`## intro` CLAUDE.md text that agents are being given as project instructions
  is an older copy and does not match this worktree. Worth flagging to the owner: agents are being
  briefed off a document the repo has already replaced.

## Things checked and found consistent

Party size per step (machines 1, hill/pasture 2, boss 3) matches the spine and the battle setups;
the effort table (2/5/9/15/24, ×0.5-×1.5), +8 regen, guard +4, parry +8 and OPEN×2 match COMBAT §2-§4
exactly; skill minimums and owners match §5; `klee`/`klee2` phases, `BTF_IMMUNE_ATTACK` and no-run
match §9; every clip id in `CH_STEPS` exists in `cutscene_data.h` and in `playlist.md`; all four item
effects in `battle.cpp:297-303` match `LOOT.md`; Falke's `look` is green-haired, unarmoured and
weaponless and Distel's is explicitly furred and non-human in every scene and panel — no line
anywhere still describes her as a human-looking girl or him as red-haired; nine appears only as the
ring of holes; eleven only as the flock; the flock is uncounted on the sheet.
