# CLAUDE.md

## What this project is

A Phantasy Star IV style JRPG on SDL3 + OpenGL, built **one chapter at a time**. You walk around a
**voxel world with billboard sprites** (`src/voxfield.cpp`), talk in dialogue boxes, watch manga-page
cutscenes, and fight **turn-based battles whose twist is an effort slider** — every action is spent
at effort 1-5 against a stamina bar (`story/v3/COMBAT.md`, `src/battle_rules.cpp`). Chapter one is
*The High Pasture*. Terrain and buildings need **no drawn art** — the engine generates them from
`.tmap` text maps — so the only art is what moves or reads as a character, and it comes from ChatGPT
through **the art tray** (`story/packages/`). **The phone is the target**; the Mac is where it is
tested. C11 own code, C++ deps allowed.

## Standing rules (they override everything)

- **TARGET: Android.** Build and deploy to the phone only; don't build desktop for shipping.
- **Nothing is pushed to the phone unless the owner asks in that moment** (D22). No session polling
  loop, no automatic `fast_reload.sh`. Test on the Mac (`./run_desktop.sh`, `./capture.sh`).
- **Every Mac run of the game is a test run and is muted** (`STAR_MUTE`; the scripts force it).
- **Agents never commit and never deploy.** The owner/orchestrator makes every commit.
- **One agent per file.** Conflicts and tool complaints go in `story/notes/<agent>.md`; the
  orchestrator rules on them in `story/DECISIONS.md`.
- **No sunk cost.** When the idea changes, rebuild from the new idea — don't preserve old work for
  its own sake. Nothing is kept as "legacy": deleted work lives in the `legacy-final` git tag (D24).
- **Brainstorm before dispatch.** A chapter is talked through with the owner in
  `story/v3/BRAINSTORM.md` first; agents are spawned only after the owner says write.
- **The story is written as we go** (D23). **Canon is only what is on screen in a finished chapter**,
  plus `story/v3/NAMES.md`. `PREMISE.md` is a pool of ideas, not a schedule.

## The game

The game builds as `libgame_logic.so` and is hot-reloadable — see *Hot reload* below.
**`src/ENGINE.md` is the map of `src/`**: every file's responsibility, its entry points, the test
that covers it, and the include graph. Read it before opening a source file.

- **`src/voxfield.cpp` is the world.** Terrain, water and buildings are voxels the engine generates
  at load from the `.tmap` text maps: palette ramps, procedural shader detail and rule-built houses.
  **None of that needs generated art**, which is the whole point of the design. Half-size voxels,
  shadow map, free analog movement and jumping. `src/notes/world.md`, `movement.md` and
  `rendering.md` are the engine's own contract.
- **`src/battle_rules.cpp`** is the turn-based combat (with `battle_ui/script/test.cpp`, and
  `src/notes/battle.md` for which file owns which rule); `src/dialogue.h` the field dialogue box.
- **Cutscenes** are manga pages with typewriter dialogue and adaptive music, drawn from the generated
  `src/cutscene_data.h`. A panel with no art yet draws a placeholder box with its description in it,
  so a chapter plays end to end while its sheets are being generated.
- **Music** is one sequencer with six moods (`wonder, dread, tense, confront, sorrow, hope`),
  two-operator FM voices for a Genesis flavour. A dialogue line's `{mood}` tag sets the target; the
  change lands on the next beat with a stinger, the tempo glides, the bar count never resets.
- **Landscape only** — `host.cpp` sets `SDL_HINT_ORIENTATIONS` before creating the window.
- The **Dev** button (top right) opens the message log, scene controls and mood audition.

## The contracts

Four documents, and they beat anything written anywhere else:

| file | what it owns |
|---|---|
| `WORLD.md` | the voxel field: what needs art and what does not, the `.tmap` format, map design rules, asset sizes |
| `PALETTE.md` | 256 indexed colours, the colormap light tables, conversion (D19) |
| `story/STYLE.md` | the locked art style and composition rules for cutscene panels |
| `src/ENGINE.md` | the map of `src/`: file, responsibility, entry points, test |
| `src/notes/*.md` | the engine's long-form record (world, movement, rendering, battle, chapter, testing) |

`story/DECISIONS.md` is the append-only log of every scope and story call, with how to reverse it.

## Writing the game: D23, one chapter at a time

There is **no master plan to execute**. `story/v3/PREMISE.md` is a pool of ideas, not a schedule.
**Canon is only what is on screen in a finished chapter**, plus `story/v3/NAMES.md` and the
characters those chapters rely on.

Per chapter: talk it through with the owner (ideas in `story/v3/BRAINSTORM.md`, no agents) → one
writer pass when the owner says write → a cold read by an agent who knows nothing → the owner reads
`chapterNN_script.md` → revise. No planting for a future that is not decided; a detail stays if it is
good in its own scene. `story/v3/THREADS.md` is the short list of what finished chapters have
promised the player. **Art, maps and generators are built only for chapters that are written.**

Chapter one is *The High Pasture*: `story/v3/chapter01.md` (the spine — what the player does),
`story/scenes/01*.md` (six scenes — what anyone says), `story/v3/chapter01_script.md`,
`story/v3/ch01_room/` (the slot contract and the designer's brief), `BESTIARY.md`, `LOOT.md`,
`COMBAT.md`, and `story/field/text.md` (what the world says when you look at it).
`src/chapter01.h` is the spine turned into a table of steps and flags — **hand-written**, not
generated; it is the one file that knows the chapter's shape.

**Everything is tokenised.** Every proper noun in story text is `{{TOKEN}}`, speaker labels included;
`story/v3/NAMES.md` says what each is called this week and every command substitutes before it uses
the text, so a rename is one line. A token with no row is a validation error — `./story_prompt.py
names` finds them.

**`story/playlist.md` is what counts.** Its `## chapterNN` lists are what the game plays (New Game
starts at the first) and the tool's definition of a real scene: a file in `story/scenes/` that no
list names is a draft — no package, no count, no export.

## The art tray: the whole art loop

**`./story_prompt.py packages` → open a folder → generate in ChatGPT → save `returned.png` →
`./story_prompt.py ingest`.** `story/packages/README.md` is the ordered to-do with a status per row,
and `./tools/arttray/run.sh` is the Mac GUI over it.

Five kinds of package and nothing else:

1. `cast/<name>/refsheet` — the reference sheet. Everything else attaches it.
2. `cast/<name>/expressions` — ten dialogue-box faces.
3. `cast/<name>/walker` — the nine-frame walk sheet.
4. `ch01/sprites` — the voxel world's billboards: creatures, animals, the training machines.
5. `ch01/scenes/<scene>` — one shot sheet per panel scene.

- **Never hand-write an image prompt**, and never paste style text into one. Write the data (a scene
  file, a `characters.md` entry, a `story/field/sprites.md` entry) and run the tool; it inserts the
  locked style blocks and each character's `look` verbatim and refuses a description that breaks a
  rule. Fix the source; don't bypass the tool. **Never edit anything under `story/packages/` by hand.**
- **A package that has been drawn is frozen**: its template and slot boxes never move, so the image
  already in the tray can always be cut again. New ids go into a fresh `<name>_2` beside it.
- **Statuses are computed, not guessed.** `ingest` stamps each package with a fingerprint of the
  inputs it cut from, so editing a `look` line or a scene's panels later shows the art as **stale**,
  with the reason on the row. `done` means the inputs have not changed.
- `story/packages/tilesets/` is **frozen and finished** — the valley atlas the voxel field reads. It
  is never regenerated and never on the queue. There is no work there.
- Raw generations are archived in `story/sheets/` so anything can be re-cut; a package's
  `returned.png` is gitignored scratch.
- **Review before cutting.** Check a returned sheet against the checklist at the end of its
  `prompt.md` (gaze, expression, screen direction, design, hands, sheet hygiene) and say what fails
  before running `ingest`. Reject on gaze or expression even when the art is good.
- The ChatGPT app caps every image at about **1.5 megapixels** whatever size is asked for, so a sheet
  that is half empty throws half the detail away and past about eight slots everything comes back
  mush. That is why the tool splits sheets.

### Commands

```bash
./story_prompt.py check --all       # scenes + field text + every .tmap + the palette. Run this.
./story_prompt.py packages          # (re)build the tray
./story_prompt.py ingest [folder]   # cut every returned.png waiting in it
./story_prompt.py export            # -> src/cutscene_data.h and src/field_text.h
./story_prompt.py names | stats | brief ["beat"]
./story_prompt.py script [chapter]   # rebuild story/v3/chapterNN_script.md: the chapter as a play
./story_prompt.py tmap check <map>|--all   |   tmap preview <map>
./story_prompt.py palette build | apply | check | colormap
./tools/arttray/run.sh [--selftest]  # the Mac GUI; --selftest prints one PASS/FAIL line
tools/script/golden.py record|diff   # the tool's own contract: every command's output, hashed
./run_desktop.sh                    # play it on the Mac, muted, reading story/ directly
./capture.sh --vox <map>            # one rendered frame to build_desktop/, no phone
./capture.sh --vox-selftest | --vox-walktest [map] | --chapter-selftest | --battle-selftest
./perf.sh [map] | --desktop | --watch   # the A/B benchmark (src/notes/rendering.md, "Performance")
./deploy.sh                         # android APK -> phone (only when the owner asks)
./fast_reload.sh                    # hot reload libgame_logic.so (~0.7s; only when the owner asks)
./compile_shaders.sh                # GLSL 450 -> SPIR-V -> glsl330, glsl300es, msl, hlsl
./dev_msg.sh "message"              # send a dev message to the in-game Messages tab
```

## Style

- Own code: C11, `.c` files. C++ deps linked, never authored.
- Flat `src/`. No subdirs until ten files.
- One `CMakeLists.txt` shared by desktop and Android. FetchContent for SDL3 and cglm; single headers
  in `third_party/`, with `#define ...IMPLEMENTATION` once, in `src/third_party_impl.c`.
- Write GLSL 450 in `shaders/`; `compiled/` is generated and gitignored.

## Libraries and the decisions behind them

**SDL3** (windowing, input, lifecycle), **cglm** (math), **par_shapes**, **stb_image/write/truetype**
(Roboto bundled), **FastNoiseLite**, **sokol_gfx**, **SPIRV-Cross** (build tool).

- SDL3 for windowing, not sokol_app — more mature, better Android lifecycle.
- No raylib — too high level; raw GL preferred. Generation only, no asset loaders.
- FastNoiseLite, not FastNoise2 — C-compatible; FN2 is C++17.
- No realtime lighting: light is baked, and colour comes from the palette's light tables (D19).
- JDK 21 via brew for Android builds — Android Studio's bundled JDK 25 breaks Gradle 8.9.

## Hot reload

Split-library architecture for live code reloading on Android without an app restart.

- **libmain.so** (`host.cpp`) owns the SDL window, GL context, ImGui init and the reload machinery.
  **libimgui_shared.so** carries ImGui + SDL3 so every `.so` shares one copy. **libgame_logic.so** is
  all the game logic and is what gets swapped. **libSDL3.so** is shared on Android.
- The host checks `reload.flag` about once a second: serialize state → `dlopen` the new `.so` under a
  unique name (`libgame_logic.so.<pid>.<gen>` — **no `dlclose` on Android**, it backgrounds the app)
  → create fresh state → deserialize. A bad `.so` logs `Hot reload FAILED: keeping previous game
  logic` and the app keeps running.
- The serialization buffer is **heap-allocated** (1 MB). Stack allocation crashes Android's SDL thread.
- The blob's header carries `sizeof(Game/Character/Enemy)`. If a layout changed, deserialize keeps
  the fresh state and says so. No restart; you lose unsaved in-memory state.
- **Reloads land while backgrounded**: the host sets `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0` and polls
  4x/s while hidden. Once Android freezes the process, `fast_reload.sh` prints `Queued` and the
  reload applies when the app is opened.
- **SDL3 never queues lifecycle events.** `SDL_EVENT_DID_ENTER_BACKGROUND/FOREGROUND` reach only
  `SDL_AddEventWatch` callbacks, never `SDL_PollEvent`. Any `if (e.type == SDL_EVENT_DID_ENTER_*)`
  inside a poll loop is dead code.
- **UI scale** (`dpi_scale`) = shorter screen edge / 360, computed every frame in `host.cpp`. Never
  derive scale from height alone. Font size is live: the host sets `style.FontSizeBase` before every
  `NewFrame`; changing that ratio is a host change and needs `./deploy.sh`.
- `fast_reload.sh` is self-verifying: it checks the device, verifies the pushed md5, launches the app
  if needed, and waits for `Hot reload SUCCESS/FAILED` in logcat. **Read its output.** Changing
  `host.cpp` or `game_api.h` needs a full `./deploy.sh`.
- `CMAKE_SHARED_LINKER_FLAGS` with `-Wl,-z,max-page-size=16384` must be set BEFORE
  `FetchContent_MakeAvailable(SDL3)`.

### Diagnosing a crash on the phone

Don't guess from the diff. `adb` is at `~/Library/Android/sdk/platform-tools/adb` (not on PATH).

```bash
ADB=~/Library/Android/sdk/platform-tools/adb
$ADB logcat -c; $ADB shell monkey -p com.playground.sdlraylib 1; sleep 6
$ADB logcat -d | grep -E "F DEBUG|signal [0-9]|SDL/APP"
```

Read the `#00`/`#01` frames. A crash inside ImGui called from `libgame_logic.so` is almost always
garbage input, not an ImGui bug. If it crashes on load, suspect `files/save.dat` first — it is a raw
`memcpy` of `Game`, so adding a field to anything it embeds scrambles an old blob. The loader must
reject a mismatched size, never "migrate" by copying bytes. Verify a fix by **launch**, not by
compile: `$ADB shell pidof com.playground.sdlraylib` still returns a pid ten seconds later.

## The entry map — one task, one place to look

Find your row. Don't read the whole repo. ⚠ = over ~1500 lines and being split; read only the
section you need (each has a banner comment), never top to bottom.

| I want to… | Touch | Explained in |
|---|---|---|
| change dialogue in a cutscene | `story/scenes/01NN_*.md`, then `./story_prompt.py export` | `story/STYLE.md`, `story/v3/STYLE.md` |
| change what the world says when examined | `story/field/text.md`, then `export` | `WORLD.md` §3 |
| rename anything | `story/v3/NAMES.md` (one row), then `./story_prompt.py names` | `story/v3/NAMES.md` |
| add or edit a map | `story/field/tmaps/<map>.tmap`, then `tmap check` + `tmap preview` | `WORLD.md` §3-4 |
| change the chapter's order, flags or goals | `src/chapter01.h` (one row of `CH_STEPS`) | `story/v3/chapter01.md` |
| add an enemy | `story/v3/BESTIARY.md`, a `fight` trigger in a `.tmap`, a row in `src/battle_rules.cpp` ⚠ | `story/v3/COMBAT.md`, `src/notes/battle.md` |
| fix a battle bug | `src/battle_rules.cpp` (logic) or `battle_ui.cpp` (screen) ⚠; `./capture.sh --battle-selftest`, `--battle-ui-test` | `src/notes/battle.md` |
| change a combat number | `story/v3/COMBAT.md` **first** (it is the source), then `src/battle_rules.cpp` §1 ⚠ | `story/v3/COMBAT.md` |
| fix a world/movement/render bug | the `src/vox_*.cpp` file `src/ENGINE.md` names ⚠; `./capture.sh --vox-selftest`, `--vox-walktest` | `src/notes/world.md`, `movement.md`, `rendering.md` |
| change a palette table or light | `story/palette/**` via `./story_prompt.py palette build \| colormap` — never by hand | `PALETTE.md` |
| generate art | `./story_prompt.py packages` → the folder → `returned.png` → `ingest` | `story/packages/README.md` |
| add a billboard sprite | `story/field/sprites.md`, then `packages` + `ingest` | `WORLD.md` §2 |
| change a composition rule | `storytool/rules.py` **and** `story/STYLE.md` together | `storytool/README.md` |
| change the tool | the module named in `storytool/README.md`'s table | `storytool/README.md` |
| play it on the Mac | `./run_desktop.sh` (muted), `./capture.sh --vox halm` | `capture.sh` header |
| run every test | `./story_prompt.py check --all`; `./capture.sh --vox-selftest`, `--vox-walktest all`, `--chapter-selftest`, `--battle-selftest`; `tools/script/golden.py record\|diff` | this file |
| change a cutscene's look or the dialogue box | `src/cutscene.cpp` ⚠; `./capture.sh --dialog`, `--clips-selftest` | `src/ENGINE.md` |
| profile | `./perf.sh [map]`, Dev panel → Perf HUD | `src/notes/rendering.md` |
| put it on the phone (**only when asked**) | `./fast_reload.sh`, or `./deploy.sh` for `host.cpp`/`game_api.h` | *Hot reload*, above |

Where things live: `src/` — **`ENGINE.md` is its map**, and `notes/` (`world`, `movement`,
`rendering`, `battle`, `chapter`, `testing`) the long-form record. The game is `star_logic.cpp` the
entry, `game.cpp` ⚠ screens and the GameAPI, `cutscene.cpp` ⚠ the page player, `chapter.cpp` ⚠ the
chapter in the field, `dev_panel.cpp`, `settings.cpp`, `audio.cpp`, and `star_test.cpp` /
`playtest.cpp` the suites; combat is `battle_rules/ui/script/test.cpp` ⚠; the world is
`voxfield.cpp` + the `vox_*.cpp` files ⚠; `host.cpp` window/GL/hot reload; `chapter01.h` the chapter
table; `cutscene_data.h` and `field_text.h` are **generated** by `export`) · `story/` (`STYLE.md`,
`characters.md`, `playlist.md`, `scenes/`, `panels/`, `refs/`, `portraits/` and `packages/` the
tray, `sheets/` archived returns, `palette/`, `v3/` the live story, `notes/`, `DECISIONS.md`) ·
`story/field/` (`sprites.md`+`sprites/`, `walkers.md`+`walkers/`, `tmaps/`, `tilesets/valley/`
frozen, `text.md`, `manifest.md` generated) · `story_prompt.py` is a shim over the `storytool/`
package — **`storytool/README.md` is its map**; no module past ~600 lines · `tools/arttray/` the Mac
GUI, `tools/script/golden.py` the tool's refactor contract (require `IDENTICAL`, take the
before/after pair back to back) · `shaders/`, `assets/`, `third_party/`, `android/`.
