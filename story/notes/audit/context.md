# AUDIT — context friendliness

2026-09-21. The brief: *"the codebase and files are context friendly"* — a fresh agent given ONE task
should find the right file fast and not load 6,000 lines.

Read-only except the doc files this agent was cleared to edit. **Nothing in `src/` was touched** (an
engine agent is working there); `src/VOXFIELD_NOTES.md` changes are proposed in §1b and §6 only.
Sibling reports: `story/notes/audit/consistency.md` (which already logged several of the stale
pointers below) and `story/notes/audit/smells.md`.

---

## 1. STALE DOCS

### 1a. FIXED in this pass

| file:line | was | now |
|---|---|---|
| `CLAUDE.md:55` | chapter one is *The Last Job Sheet* | *The High Pasture* (`story/v3/chapter01.md:1`) |
| `CLAUDE.md:190` | "`cutscene_data.h`, `field_text.h`, `chapter01.h` generated" | `chapter01.h` is **hand-written**; only two headers are generated (`storytool/export.py:30,33`) |
| `CLAUDE.md` (index) | a list of folders with no task routing | replaced by the entry map, §3 |
| `CLAUDE.md` (top) | two rules | the newcomer paragraph (§5) + the eight standing rules (§4) |
| `CLAUDE.md` (commands) | no Mac test path at all | `run_desktop.sh`, `capture.sh` (five modes), `perf.sh` added |
| `storytool/README.md:25` | "change a map rule … and `TILES.md`" | `WORLD.md` §3-4 |
| `story/field/tmaps/README.md:3` | "the plain-text format `TILES.md` fixes" | `WORLD.md` §3, named as the single source |
| `story/field/tmaps/README.md:14` | "`## height` … (NEW, parser pending)" | parsed and shipped; points at `WORLD.md` / `src/VOXFIELD_NOTES.md` |
| `story/field/tmaps/README.md:18` | "`VOXFIELD_NOTES.md`" | `src/VOXFIELD_NOTES.md` |
| `story/field/tmaps/README.md:30` | "`<= 1` voxel is a walk edge, more is a ledge" | **understated the climb rule by a whole voxel** — one is a step, **two is a hop**, three is a ledge (`src/voxfield.cpp`, `vx_can_step`) |
| `PALETTE.md:30` | "…panels and the parked 3D art alike" | the parked 3D art was deleted by D24 |
| `PALETTE.md:41` | tables "`day`, `dusk`, `night`, plus effect tables" | adds **`lamp`/`lantern`**, the point-light table, and makes `colormap.json` the authority for rows |
| `story/v3/STYLE.md:227` | "from `story/v4/DIFF.md` §4" | `v4/` was deleted by D24; marked as derived from the retired file, in `legacy-final` |
| `story/v3/SMELLS.md:3-5` | read as current story | banner: it is **evidence** against a replaced draft; Bron/Dorn/Lyra/Cray are the pre-token names; the `BAN:` list is the only live part |

**13 fixed.**

### 1b. LEFT — file not in this agent's edit scope. Owner or the owning agent to apply.

**Engine (`src/**` — the engine agent's):**

1. `src/VOXFIELD_NOTES.md:8-10` — *"`tilefield.cpp` is parked… the Dev panel's **Old tile field** button… **Old 3D field** still reaches `field.cpp`"*. Neither file exists in `src/` any more. → Delete the paragraph; say the voxel field is the only field.
2. `src/VOXFIELD_NOTES.md:12-14` — *"the same `tiles.md` entry list, the same atlas"* is still true, but reads as "nothing changed" when everything else did. → Reword to "the frozen valley data".
3. `src/VOXFIELD_NOTES.md:24` — *"up to 192 x 192 voxels and **48 up**"* contradicts **line 162** of the same file (*"`VX_VY` went from 48 to **64 voxels**"*) and `src/voxfield.cpp` (`#define VX_VY 64`). → 64 (96x96 cells, **32** cells high).
4. `src/VOXFIELD_NOTES.md:771-780` — the Dev panel section still lists **Old 3D field** and **Old tile field** buttons. → Delete those two.
5. `src/VOXFIELD_NOTES.md:877` — *"no `story/field/sprites/` directory exists at all"*. → Re-check; if the tray has cut any of the nine ids this is now wrong.

**Story (not in scope):**

6. `story/playlist.md:17` — *"**The Last Job Sheet** — six clips"*. → *The High Pasture*.
7. `story/STYLE.md:11` and `:27` — cite `./story_prompt.py **build** <scene>`, the single-page mode removed by D24 (`storytool/cli.py:109-112`). → Delete both clauses.
8. `story/v3/COMBAT.md:16` — *"**None of this exists in the engine yet.** §10 is how the chapter plays today."* → **False.** `src/battle.cpp` (2171 lines) implements §§2-5 and its numbers match line for line. Rewrite, and mark §10 (the no-combat fallback) as superseded.
9. `story/v3/BESTIARY.md` — gives the healer a stamina pool of **24** where `COMBAT.md:55` gives **36**; `BESTIARY.md:173` itself then says 36. `src/battle.cpp:285-288` carries a comment adjudicating the doc bug. → Delete the 24, then delete the adjudication comment.
10. `story/v3/PREMISE.md:36` "red-haired" → hair is forest-green (`story/characters.md:73`).
11. `story/v3/PREMISE.md:93,109` — {{MENTOR}} as "the old hunter" with **four** training machines. → He is the town's **engineer**; there are **three** machines (post, arm, swing).
12. `story/v3/PREMISE.md:37,132-134` — "loses to the last of the training machines" → loses to **the swing**.
13. `story/v3/PREMISE.md:95-105,132-142,156-158` — the **jar**, the water run, the **{{STAIR}}**, the road west, "(Chapter one.)". All retired (`story/v3/THREADS.md:7-10`, `NAMES.md:152`). → Mark the beats dead or cut them. D23 makes PREMISE a pool of ideas, so this is low priority — but the "(Chapter one.)" marker actively misleads.
14. `story/v3/STYLE.md:28` — *"{{HEALER}} asks precisely and rarely exclaims"* contradicts `NAMES.md:101-109` and this file's own line 105 (Ottilie is loose, teasing, the least formal person in the chapter). *(Left: a character ruling, not a stale file reference — outside "fix stale references only".)*
15. `story/v3/STYLE.md:307,312,314` — the sample lines quote the father's water run, **a jar**, and going **west**. All cut. → Replace with lines from `0150_the_sword`. *(Same reason as 14.)*
16. `story/characters.md:71,76,86` — the pre-v3 Tellwater/Sallowgate backstory for Falke and Lyra, and a `people: Kell` with no row in `NAMES.md`. → Mark "legacy, superseded" the way the Lyra entry half does.
17. `story/field/tilesets/README.md:3,24` — *"Generated by `./story_prompt.py tileset <set>`"* (generator deleted) and `TILES.md` as the decision of record. Also `:33` and `:99-100` describe tile/prop/building art the voxel world does not use. → One **FROZEN (D22/D24)** banner at the top; point at `WORLD.md`.
18. `story/field/tilesets/valley/tiles.md:3` — *"The engine reads this file (`src/tilefield.cpp`)"*. → `src/voxfield.cpp`.
19. `story/DECISIONS.md:70` — New Game plays the `## intro` list, deleted by D24 at `:314` of the same file. D-entries are an append-only log so history is fine, but this one reads as current. → One "superseded by D24" line.
20. `tools/arttray/README.md:61-66,96-97,33-35` — the `screen` three-step package as the worked example; painted screens were deleted by D24 and the tray has five one-step kinds. → Re-example or mark synthetic.

**Scripts (the engine agent's — this side does not touch them):**

21. `fast_reload.sh:105` "Field maps and art (FIELD.md)" and `:131` "Tilesets (TILES.md)" → `WORLD.md`.
22. `fast_reload.sh:119-122` still special-cases `*.screen`, `*.triggers`, `*.map` — formats D24 deleted.

**Tool internals (not in scope):**

23. `storytool/tmap.py:3` "the format in TILES.md" and `:245` "src/TILEFIELD_NOTES.md" (never existed post-pivot) → `WORLD.md` / `src/VOXFIELD_NOTES.md`.
24. `storytool/field.py:5,58` cite `src/FIELD_NOTES.md`, which does not exist.
25. `storytool/cli.py` usage text uses **Bron** as the worked example name throughout; Bron is Falke now (`story/v3/NAMES.md`). Every agent reading `--help` learns a dead name.

**25 left.**

Unrelated but found while validating: `./story_prompt.py check --all` currently fails with a **real**
error — `hill_path.tmap: fight id 'burr3' is not one of arm, burr, fleece, klee, lantern, lid, post,
swing` (twice). Not a doc problem; the map or `BESTIARY.md` needs the id. Everything else passes
(6 scenes ok, `text.md` 66 lines 0 placeholders, 4 of 5 maps ok, palette 87 images 0 problems).

### 1c. Searched and NOT found (so: clean)

"four coin"; a guest party member; "Distel is human" (she is explicitly a Mohn); the road creature
called "sheets" (`NAMES.md:55` already renames it **drape**); "Hart sells him the sword" in any
phrasing; `scenes_rejected/`, `outline/`, `canon.md`, `bible.md`, `plot.md`, `locations.md`,
`REVIEW.md`, `TREATMENT.md`, `pitches/`, `tableread/`, `variants/` as live paths; `FIELD.md` or
`TILES2_PROPOSAL.md` cited as current; "Rabe" as a live character (`NAMES.md:15` already flags
`{{FATHER}}`/Rabe unused).

---

## 2. DUPLICATION — facts written twice that can drift

| fact | copies | state | **single source** | reduce to a pointer |
|---|---|---|---|---|
| composition thresholds (panel counts, word caps, style-word ban) | `story/STYLE.md:35-64` · `storytool/rules.py:15-38` · `scenes.py:268` | **agree**, and `rules.py`'s docstring declares the mirror honestly | `rules.py` for the numbers, `STYLE.md` for the wording — the existing convention, keep it | hard-coded R4 shape counts at `scenes.py:268-269` should read `rules.py` |
| **"1 to 3 pages a scene"** | `story/STYLE.md:35` only | **drifted by omission** — no constant, no check; a 4-page scene passes `check` | `rules.py` | add `PAGES_MAX = 3` and check it in `scenes.validate`, or strike the prose |
| `.tmap` grammar + the 12 trigger kinds | `WORLD.md:92-148` · `src/VOXFIELD_NOTES.md:164-183` · `storytool/tmap.py:29-45` · `src/voxfield.cpp:1076-1106` · `tmaps/README.md` | agree on kinds/args | **`WORLD.md` §3** | `tmap.py:33-45` prose → pointer (keep `TRIGGER_KINDS` as executable data); `VOXFIELD_NOTES` keeps only the `VXE_*` codes; `tmaps/README.md` now points (fixed) |
| `## height` grammar (base36, `.`, `~`, clamp exemption) | **four copies**: `WORLD.md:112-120` · `VOXFIELD_NOTES.md:140-152` · `tmap.py:108-116` · `tmaps/README.md:14-32` | agree | **`WORLD.md` §3** | the other three to a pointer plus their own half only |
| the walk-step rule (1 step / 2 hop / 3+ ledge) | `voxfield.cpp` `vx_can_step` · `VOXFIELD_NOTES.md:153` · `tmaps/README.md:30` | **had drifted** — README said 1 | `src/voxfield.cpp` `vx_can_step`, restated in `VOXFIELD_NOTES` | fixed in `tmaps/README.md` |
| `VX_VY` (world height in voxels) | `VOXFIELD_NOTES.md:24` says 48 · `:162` says 64 · `voxfield.cpp` `VX_VY 64` | **self-contradictory inside one file** | `src/voxfield.cpp` | fix `:24` |
| combat numbers (effort costs 2/5/9/15/24, ×0.5-1.5, +8 regen, guard +4, parry +8, pools 40/36/30) | `COMBAT.md:41-62` · `battle.cpp:156-161,291-293` · `battle.h:38` | **numbers agree exactly**; `battle.cpp` cites COMBAT.md line by line | **`story/v3/COMBAT.md` §3** | `battle.h:38`'s restatement → pointer |
| the healer's stamina pool | `COMBAT.md:55` 36 · `BESTIARY.md` 24 · `BESTIARY.md:173` 36 · `battle.cpp:285-288` adjudicates | **drifted, and the code is carrying a comment to explain a doc bug** | `COMBAT.md` | delete BESTIARY's 24, then delete the adjudication comment |
| "none of this exists in the engine yet" | `COMBAT.md:16` vs `battle.cpp` (2171 lines) vs `CLAUDE.md:20` | **drifted** | the code | rewrite `COMBAT.md:16` and §10 |
| palette constants (256, index 0, 32 levels, `LAMP_CHROMA` 0.70, rows bright→dark) | `PALETTE.md` · `palette.py:64,73` · `colormap.py:3,25,45,124` · `voxfield.cpp:315,3261` | agree | **`PALETTE.md`** for the contract, `colormap.py`/`palette.py` for the constants | `PALETTE.md:41` now names all the tables and defers to `colormap.json` (fixed) |
| asset sizes (walker 128x192, sheet 384x576, atlas cell 128, portrait 288, 64 px/cell) | `WORLD.md:41-45,56` · `field.py:54,65,75-89` · `rules.py:95,114` · `voxfield.cpp:3350` | agree; the Python constants already cite WORLD.md | **`WORLD.md` §1 "Sizes"** | `voxfield.cpp:3350`'s bare `128` should read `atlas.json`'s `cell`, which the tool already writes |
| **tray statuses** | `packages.py:125,230-239,278` · `story/packages/README.md:20-30` (adds `exists`, `blocked`) · `tools/arttray/README.md:37-44` + `ArtTray.swift:82-116` (**a different five**, with no `stale`) | **drifted three ways.** A package whose `look` line changed reads `done` in the GUI and `stale` in the README — which defeats the fingerprinting CLAUDE.md calls the whole point | **`storytool/packages.pkg_status`** | the Swift enum should display what `pkg_status` returns; `arttray/README.md:37-44` → pointer |
| art package kinds (the five) | `CLAUDE.md:77-82` · `packages.py:3-4` · `cli.py` usage | agree | `packages.py` docstring | — |
| hot reload (1 s poll, 4x/s backgrounded, 1 MB heap, unique name, no `dlclose`, scale/360) | `CLAUDE.md` §Hot reload · `src/host.cpp:139-144,168,180,202-204,276-277,375-384` | **agree, string for string** | `src/host.cpp` | leave; it is working |
| music moods, scene kinds, expression ids | `CLAUDE.md:24` · `rules.py:45,67-83,108` · enums in `src/` | agree | `rules.py` | **no compile-time check either side** — a reorder silently breaks cut sheets (`rules.py:64-66` warns); worth a golden test |
| `storytool/README.md:36-84` module line counts | a hand-typed copy of `wc -l` over ~50 files | exact today | the filesystem | **generate it**; and `packages.py` at 636 already breaks the "~600 lines" rule `CLAUDE.md` states |
| the 1.5 MP ChatGPT cap | `CLAUDE.md:99-101` · `field.py:46-47` | agree | `CLAUDE.md` | — |
| `SCENE_MAX_PANELS` / `SHEET_MAX_PANELS`; `EXPR_OUT_H` / `PORTRAIT_H` | both pairs inside `rules.py` | agree; the comment on `EXPR_OUT_H` admits it is a copy | one constant each | alias, don't retype |

---

## 3. THE ENTRY MAP

Written into `CLAUDE.md` in place of the old `## Index`, 36 lines. Rows: change dialogue · change
examine text · rename anything · add/edit a map · change the chapter's order or flags · add an enemy
· fix a battle bug · change a combat number · fix a world/movement/render bug · change a palette
table · generate art · add a billboard sprite · change a composition rule · change the tool · play it
on the Mac · run every test · profile · put it on the phone. Each row names 1-3 files or commands and
the doc that explains them, followed by one "where things live" paragraph.

⚠ marks the files over ~1500 lines that are being split and must be read by section, never top to
bottom: **`src/voxfield.cpp` (6527)**, **`src/star_logic.cpp` (2712)**, **`src/battle.cpp` (2171)**.
`src/FastNoiseLite.h` (2441) is third-party and is not marked.

The "run every test" row is new and was previously findable nowhere: `./story_prompt.py check --all`,
`./capture.sh --vox-selftest | --vox-walktest all | --chapter-selftest | --battle-selftest`,
`tools/script/golden.py record|diff`. Likewise `./run_desktop.sh` and `./perf.sh` — the Mac test path
is how the standing "don't push to the phone" rule is actually obeyed, and CLAUDE.md did not mention
any of it.

---

## 4. AGENT HYGIENE

`CLAUDE.md` had two of the owner's standing rules (Android target, don't push to the phone). Added
the rest as **## Standing rules** immediately under the opening paragraph:

- pushed to the phone **only when the owner asks in that moment**; test on the Mac (was: partial)
- **every Mac run is a test run and is muted** (`STAR_MUTE`) — was only in two script headers
- **agents never commit and never deploy**
- **one agent per file**; conflicts to `story/notes/<agent>.md`, ruled on in `DECISIONS.md`
- **no sunk cost** — rebuild from the new idea; nothing kept as "legacy", it lives in `legacy-final`
- **brainstorm before dispatch** — `story/v3/BRAINSTORM.md` with the owner, no agents, until "write it"
- **the story is written as we go** (D23); canon is only what is on screen in a finished chapter

---

## 5. "WHAT IS THIS PROJECT"

Eight lines at the top of `CLAUDE.md`, replacing a two-line description that named neither combat nor
the art loop: PS4-style JRPG built one chapter at a time · voxel world with billboard sprites
(`voxfield.cpp`) · turn-based combat whose twist is the **effort slider** · chapter one *The High
Pasture* · terrain and buildings need **no drawn art**, generated from `.tmap` text · the art that
exists comes from ChatGPT through the tray · **the phone is the target**, the Mac is the test bench.

---

## 6. PROPOSED SPLITS (outline only — not applied)

### `src/VOXFIELD_NOTES.md`, 877 lines → an index + five notes

It is the single worst context file in the repo: an agent fixing a jump bug loads the house
generator, the benchmark methodology and the HD-2D pass to get there. The section headings already
cut cleanly.

- **`src/ENGINE.md`** (~60 lines) — the index. One paragraph per module (`voxfield`, `battle`,
  `star_logic`, `host`, `chapter01.h`, `vxperf.h`), the selftest/capture command list, and the
  **Known issues** table (current lines 823-877) with each row pointing at the note that owns it.
  This is what a fresh agent reads.
- **`src/notes/vox_world.md`** — current §§ *The shape of it*, *Shaped blocks*, *Blocks are palette
  colours*, *The house generator*, *Trees*, *Ramps*, *Terrain height and the flood fill* (≈230 lines).
- **`src/notes/vox_move.md`** — *Movement* through *Reachability*, plus *Input*, *Debug*, *The walk
  test* (≈200). The erosion proof and the five jump numbers are the payload.
- **`src/notes/vox_render.md`** — *The shadow map*, *Camera*, *Sprites*, *HD-2D*, *Occlusion*,
  *Numbers* (≈130).
- **`src/notes/vox_api.md`** — *What the `.tmap` becomes*, *New trigger kinds*, *Events*, *Field
  sprites*, *What the chapter script drives*, *The reload blob* (≈130). This is the file the story
  and chapter side reads, and today it is buried in the middle of a rendering document.
- **`src/notes/vox_perf.md`** — *Performance*, *The instrumentation*, *The HUD*, *The benchmark*,
  *What was optimised*, *The spike recorder*, *Overdraw* (≈170). Almost nobody needs it, and it is a
  fifth of the file.

Rule to keep it split: each note opens with one line saying what it owns and what it must never
duplicate; `ENGINE.md` is the only file anyone is told to read first. **The engine agent owns this
move** — it is in `src/`.

### Other `.md` over ~400 lines

| file | lines | verdict |
|---|---|---|
| `story/field/tilesets/valley/tiles.md` | 588 | **leave.** Generated-shaped data the engine reads; nobody reads it prose-first. Add the FROZEN banner (§1b item 17). |
| `story/v3/chapter01.md` | 542 | **split into two.** The spine (blocks P1-P8, clip slots) is the live document; the three dated revision-pass preambles at the top are history → `story/notes/chapter01-revisions.md`. |
| `story/v3/SMELLS.md` | 508 | **split.** The live part is the ~40-line `BAN:` block that `fieldtext.py` reads → `story/v3/BANS.md`, read by the tool and included by both STYLE files. The 460 lines of evidence stay as a note. This also kills the STYLE.md/SMELLS.md double copy in §2. |
| `story/STYLE.md` | 453 | **leave.** It is a contract read whole by `style.py`, and the locked blocks must stay verbatim. |
| `story/notes/cold-read-ch01-draft4.md` (502), `draft2.md` (425), `notes/audit/smells.md` (413) | | **leave.** Agent reports, read once, never a dependency. |

`CLAUDE.md` itself is 245 lines. That is near the ceiling for a file every agent loads; if it grows,
the *Hot reload* section (≈45 lines) is the one to move to `src/ENGINE.md` and point at.
