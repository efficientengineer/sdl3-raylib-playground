# CLAUDE.md

SDL3 + OpenGL playground. C11 own code, C++ deps allowed.

## Session startup

At the start of every session, ask the user if they want to start polling (bug reports + dev messages from the Android device).

Always hot reload (`./fast_reload.sh`) after making code changes — don't wait to be asked.

## The game

`src/star_logic.cpp` is the game: a Phantasy Star IV style JRPG, currently the intro only (title,
narration prologue, manga-panel cutscenes with typewriter dialogue, adaptive music). World navigation
and combat are deliberately not started. It builds as `libgame_logic.so`, so everything under
"Hot Reload Architecture" applies unchanged. `src/game_logic.cpp` is the previous RPG (Quest & Glory),
kept for reference and **no longer built**; don't add features there.

- **Content is data.** Scenes live in `story/scenes/*.md`; `story/playlist.md` orders them.
  `./story_prompt.py export` writes `src/cutscene_data.h` (scene kind, text, panel layout, reveal
  order, music mood, portrait and portrait side per line). `fast_reload.sh` and `deploy.sh` both run
  the export first, so editing a scene file and hot reloading is the whole loop. Never edit
  `cutscene_data.h` by hand.
- **Three scene kinds** (`CsKind`, the scene file's `- type:` line): `panels` (default) is the manga
  page; `narration` is text over black; `talk` is a Phantasy Star IV field conversation — no panels,
  black background or the `- backdrop:` panel shown dimmed, dialogue box with speaker portraits.
- **Speaker portraits** are drawn at one end of the dialogue box in panel *and* talk scenes, with the
  text narrowed to fit and the name still in yellow. They slide in when the speaker changes. The
  first speaker in a scene takes the left, the second the right, later speakers alternate, and a
  speaker keeps their side for the whole scene. A line with no portrait (an NPC with no reference
  sheet) draws the box exactly as it was before portraits existed.
- **`Narrator` is a reserved speaker.** A line whose speaker is `Narrator` is document text, not
  somebody talking: in a panel or talk scene the game draws the box with **no name and no portrait**
  and centres the text; in a narration scene it is the text over black as before. It takes no
  left/right side, never gets a portrait, and the validator treats it as always known (no NPC
  warning). The generated header defines `CS_NARRATOR` and `star_logic.cpp` tests against it.
- **Panel art** comes from `story/panels/`, portraits from `story/portraits/` (generated, gitignored).
  `fast_reload.sh` pushes new or changed PNGs to the phone's `files/cutscenes/`; `deploy.sh` bundles
  them into the APK assets. Portraits ship under the name `portrait_<name>.png`. The game looks in
  `files/` first. A playlist scene whose panels aren't all generated yet is skipped by the export,
  with a warning; talk and narration scenes are never skipped.
- **Hot reload keeps your place**: screen, scene, and line index survive, and the page is rebuilt
  from them. Edit a line of dialogue or a music pattern and judge it on the line you were looking at.
- **Music** is one sequencer with six moods (`wonder, dread, tense, confront, sorrow, hope`), two-operator
  FM voices for a Genesis flavour. A dialogue line's `{mood}` tag sets the target; the change lands on
  the next beat with a stinger, the tempo glides, and the bar count never resets. Patterns are in
  `mus_step`, chords and tempi in `MOODS`. An untagged line keeps the current mood.
- **Landscape only.** host.cpp sets `SDL_HINT_ORIENTATIONS` before creating the window (owner's call:
  the pages are composed for a wide screen). A portrait page layout still exists as a fallback.
- The **Dev** button (top right) opens the phone-to-dev message log, scene controls (restart intro,
  next scene, title), and mood audition buttons. The old QA bug reporter was not carried over.

## Switches

- **TARGET: Android** — build and deploy to phone only. Don't build desktop.

## Style

- Own code: C11, `.c` files. C++ deps linked but not authored.
- Flat `src/`. No subdirs until ten files.
- One `CMakeLists.txt` shared by desktop and Android.
- FetchContent for SDL3, cglm. Single headers in `third_party/`.

## Commands

```bash
./build.sh [target]           # desktop build+run (default: combined_demo)
./deploy.sh                   # android APK → phone (Wi-Fi adb)
./compile_shaders.sh          # GLSL 450 → SPIR-V → glsl330, glsl300es, msl, hlsl
./fast_reload.sh              # hot reload game_logic.so to phone (~0.7s, NDK direct)
./hot_reload.sh android       # hot reload via gradle (~3s, use fast_reload.sh instead)
./tools/arttray/run.sh        # Art Tray: the Mac GUI for story/packages (prompt + refs out, image back, ingest)
./dev_msg.sh "message"        # send dev message to in-game Messages tab
./watch_bugs.sh               # monitor bug reports on device
./story_prompt.py sheet <scene>  # scene file → ChatGPT package (prompt + refs to attach) in story/out/
./story_prompt.py slice <sheet.json> <img>  # cut the generated shot sheet into story/panels/
./story_prompt.py portraits      # refs sheets → story/portraits/<name>.png (dialogue-box portraits)
./story_prompt.py stats [--all]  # per chapter: scenes by type, panels, lines, optional scenes, art
./story_prompt.py names          # the {{TOKEN}} table, and every token used with no row in it
./story_prompt.py packages [--all]  # (re)build story/packages/ — the art tray, by chapter and map
./story_prompt.py ingest [folder]   # cut every returned.png waiting in story/packages/ (the owner's one command)
./story_prompt.py tileset <set>  # THE FIELD ART PATH (D18): story/field/tilesets/<set>/tiles.md → one
                                 # template sheet package per sheet; ingest packs them into atlas.png
./story_prompt.py tmap check <map>|--all    # validate a .tmap against its tileset, text ids and exits
./story_prompt.py tmap preview <map>        # render it from the atlas → story/out/<map>.tmap.png
./story_prompt.py tiles <id>...  # (parked) field art: template sheet of square slots → story/field/tiles/
./story_prompt.py props <id>...  # field art: slots sized by each prop's footprint → story/field/props/
./story_prompt.py walker <Name>  # field art: the 4x4 walk grid (S W E N) → story/field/walkers/
./story_prompt.py building <id>...  # field art: front wall + wall and roof samples → story/field/buildings/
./story_prompt.py screen <map> <zone>  # (parked) a top-down 16-bit map painted whole from
                                 # story/field/screens.md, plus its walkable mask → story/field/screens/<map>_<zone>.screen
./story_prompt.py view <map> <zone> # (older path, block-out painting) → story/field/views/<map>_<zone>_paint.png
                                 # (export also writes src/field_text.h from story/field/text.md)
./story_prompt.py cut <sheet.json> <img>    # cut a filled-in template: key magenta, resize, write the files
```

## Shader pipeline

Write GLSL 450 in `shaders/`. Run `./compile_shaders.sh`. Output in `shaders/compiled/`.
Tools: glslang (GLSL→SPIR-V), SPIRV-Cross (SPIR-V→everything else). Installed via brew.

## Libraries

- **SDL3** — windowing, input, lifecycle (FetchContent)
- **cglm** — math: matrices, vectors, quaternions, SIMD (FetchContent)
- **par_shapes** — procedural meshes: spheres, cylinders, tori, knots
- **stb_image/write/truetype** — image I/O, font rasterization (Roboto bundled)
- **FastNoiseLite** — procedural noise: Perlin, simplex, cellular
- **sokol_gfx** — GPU abstraction (GL/GLES/Metal/WebGPU); use with SDL3 windowing
- **SPIRV-Cross** — shader cross-compilation (build tool, not linked)

Single headers need `#define ...IMPLEMENTATION` once — that's in `src/third_party_impl.c`.

## Decisions

- SDL3 for windowing, not sokol_app — more mature, better Android lifecycle.
- sokol_gfx for GPU abstraction when needed, layered on SDL3's GL context.
- No raylib — too high level; raw GL preferred.
- Generation only — no mesh/asset loaders (no cgltf).
- FastNoiseLite, not FastNoise2 — C-compatible; FN2 is C++17.
- Procedural textures in shaders, not CPU libraries.
- Roboto font bundled — open source, consistent cross-platform.
- JDK 21 via brew for Android builds — AS bundled JDK 25 breaks Gradle 8.9.
- No realtime lighting — bake light into vertex colors/textures.
- Readability via: height fog, distance fog, good procedural textures.

## Hot Reload Architecture

Split-library architecture for live code reloading on Android without app restart.

### How it works

- **libmain.so** (host.cpp) — owns SDL window, GL context, ImGui init, hot-reload machinery
- **libimgui_shared.so** — shared ImGui + SDL3 symbols, linked PUBLIC so all .so files share one copy
- **libgame_logic.so** — all game logic, hot-reloadable via dlopen/dlclose
- **libSDL3.so** — SDL3 built as shared lib on Android

Host checks for `reload.flag` every ~1 second. When found: serializes game state →
dlopen new .so (unique filename, no dlclose to avoid Android backgrounding the app) →
creates fresh state → deserializes. Game continues seamlessly.

### Key details

- `SDL_GetPrefPath("com.playground", "questglory")` returns `/data/data/com.playground.sdlraylib/files/` on Android
- Serialization buffer is **heap-allocated** (1MB via malloc). Stack allocation crashes Android's SDL thread (limited stack).
- On Android, **no dlclose** — old .so stays in memory. dlclose causes Android to background the app.
  New .so is copied to a unique filename (`libgame_logic.so.1`, `.2`, etc.) and dlopen'd fresh.
- `fast_reload.sh` compiles directly with NDK clang++, skipping gradle entirely (~0.7s vs ~3s). It finds the gradle CMake dir by glob (the hash differs per checkout), so it works in a git worktree once `./deploy.sh` has run there. A worktree also needs `android/local.properties` (`sdk.dir=...`), which is gitignored.
- `hot_reload.sh android` uses gradle (slower but guaranteed correct flags).
- Reload flag: file with content = trigger reload; host truncates to 0 bytes after consuming.
- **Reloads land while backgrounded.** Host sets `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0` so the SDL loop keeps running when the app is hidden; it skips rendering and polls the flag 4x/s, logging `Hot reload applied while backgrounded`. No need to foreground the app. Exception: after a while in the background Android moves the process to its cached state and freezes it, and a frozen process cannot poll. `fast_reload.sh` detects that, prints `Queued`, and exits 0; the reload applies the moment the game is opened.
- **SDL3 never queues lifecycle events.** `SDL_EVENT_DID_ENTER_BACKGROUND/FOREGROUND` go only to `SDL_AddEventWatch` callbacks, never to `SDL_PollEvent`. Host uses a watcher for save-on-background and the foreground reload check. Any `if (e.type == SDL_EVENT_DID_ENTER_*)` inside the poll loop is dead code.
- **UI scale** (`dpi_scale`) = shorter screen edge / 360 (phone: 3.0 in both orientations), computed in host.cpp every frame. Never derive scale from height alone: it flips between orientations and SDL reports transient sizes at startup.
- **Font size is live**, not baked: host sets `style.FontSizeBase = 10 * dpi_scale` before every `NewFrame` (ImGui 1.92+ dynamic fonts); game logic multiplies by `FontGlobalScale` 1.3. Changing either ratio is a host change (`./deploy.sh`). `setup_touch_style` runs every tick so a bad deserialized `dpi_scale` can't stick.
- Orientation is locked to landscape by `SDL_HINT_ORIENTATIONS` in host.cpp. Without that hint SDL overrides the manifest (the window is `SDL_WINDOW_RESIZABLE`) and the app follows auto-rotate.
- Reload blob has a header with `sizeof(Game/Character/Enemy)`. If any changed since the running build, deserialize logs "layout changed" and keeps the fresh state from `game_create` (which loads the validated `save.dat`). No restart needed; you just lose unsaved in-memory state.
- Host loads the new .so **before** destroying the old game. A bad .so logs `Hot reload FAILED: keeping previous game logic` and the app keeps running.
- Unique reload copies are named `libgame_logic.so.<pid>.<gen>` so a restarted process never reuses a name.
- `fast_reload.sh` is self-verifying: checks the device is connected, verifies the pushed md5, copies via tmp+mv, launches the app if it's not running, then waits for `Hot reload SUCCESS/FAILED` in logcat and exits non-zero on failure. Read its output; don't assume the reload landed.
- `deploy.sh` deletes `files/libgame_logic.so*` after install so a stale hot-reloaded lib can't shadow the freshly built APK lib.
- `c++_static` STL works fine — imgui_shared.so is the bridge, no duplicate C++ runtime.
- `CMAKE_SHARED_LINKER_FLAGS` with `-Wl,-z,max-page-size=16384` set BEFORE `FetchContent_MakeAvailable(SDL3)`.

### Diagnosing crashes on the phone

Follow these steps in order. Don't guess from the diff alone.

1. **Get the real backtrace.** `adb` is at `~/Library/Android/sdk/platform-tools/adb` (not on PATH).
   ```bash
   ADB=~/Library/Android/sdk/platform-tools/adb
   $ADB logcat -c; $ADB shell monkey -p com.playground.sdlraylib 1; sleep 6
   $ADB logcat -d | grep -E "F DEBUG|signal [0-9]|SDL/APP"
   ```
   Look at the `#00`/`#01` frames. A crash inside an ImGui function called from `libgame_logic.so`
   almost always means garbage input (bad index into `CLASS_COLORS[]`, etc.), not an ImGui bug.
2. **Suspect the save file first if it crashes on load.** `files/save.dat` is a raw `memcpy` of
   `Game`, which embeds `Character` and `Enemy`. Adding a field to *any* of those changes the layout,
   and an old blob loaded over the new layout scrambles every field after the change. Check the header:
   `$ADB shell run-as com.playground.sdlraylib od -A d -t d4 -N 8 files/save.dat` → `version, sizeof(Game)`.
   The loader must reject a mismatched size and validate indices; never "migrate" by copying bytes.
   To rule the save out entirely: `$ADB shell run-as com.playground.sdlraylib rm files/save.dat`.
3. **Remember the hot-reload copies.** The app loads `files/libgame_logic.so` if present, else the APK's copy.
   `fast_reload.sh` handles the app-not-running and backgrounded cases itself and prints the host's verdict.
   If it prints `ERROR: no reload confirmation`, check `pidof` and the logcat `QuestGlory` tag before retrying.
   Note the script runs `logcat -c` before flagging, so earlier lifecycle lines are gone; re-run without it to see them.
   Changing `host.cpp` or `game_api.h` needs `./deploy.sh` (full APK); hot reload only swaps game_logic.
4. **Verify the fix by launch, not by compile.** Confirm `$ADB shell pidof com.playground.sdlraylib` still
   returns a pid ~10 s after launch and logcat has no new `F DEBUG` lines.

### Dev messaging

- `dev_msg.sh "text"` — pushes `[DEV HH:MM:SS] text` to device's `files/dev_log.txt`
- In-game Messages tab (QA Tools) — user types messages saved as `[YOU HH:MM:SS] text`
- Game checks for new messages every ~0.5 seconds when Messages tab is open
- Monitor from CLI: poll `files/dev_log.txt` via `adb shell run-as` every 1 second

### File format

- `dev_log.txt`: one line per message, `[DEV HH:MM:SS] text` or `[YOU HH:MM:SS] text`
- Log is capped at `MAX_MSGS` (25, owner's choice); loader keeps the **newest**. It once kept the oldest, which silently dropped every new DEV line once the file passed the cap. `MSG_LEN` 512.
- `mirror_to_phone.py` = Claude Code `Stop` hook (registered in the session project's `.claude/settings.local.json`): pushes every paragraph of the assistant's turn to the phone via `dev_msg.sh`, so the owner sees in-app everything said on the web. Hook config is read at session start; restart the session after changing it.
- `dev_msg.sh` does read-append-write of the whole file; the game re-reads the file before every save so appended lines survive. Small race if you Send within the same ~100ms.
- `reload.flag`: non-empty = reload requested; host truncates to 0 after consuming
- `libgame_logic.so`: pushed to `files/` for hot reload; bundled copy in APK as fallback

### QA Tools

Debug panel (top 1/3 of screen) with two tabs:
- **Bugs** — in-game bug reporter with binary save format (QA_SAVE_VER=2, BUG_DESC_LEN=256)
- **Messages** — chat-style log between dev and user, timestamps, auto-scroll

## Story scenes and image prompts

Scene art is 16-bit Genesis manga cutscenes (Phantasy Star IV look). The style is **locked** in
`story/STYLE.md`; read its "Rules" section before writing anything image-related.

### The story files

**The 2026-09-18 retirement.** *The Fair Copy* — 326 scenes in fifteen chapters and an epilogue — was
set aside for the v3 rewrite. Those files now live in **`story/scenes_rejected/`** with their old play
order in `scenes_rejected/playlist_rejected.md`; nothing builds from them. `story/scenes/` holds
`000_TEMPLATE.md` and chapter one's seven v3 scenes (`0110`-`0170`), and `story/playlist.md` has only
`## intro` and `## chapter01`. The reference files below still describe the retired draft where they
have not been rewritten; `story/v3/` is the live one. Start at **`story/REVIEW.md`**. Then:

- `story/DECISIONS.md` — the orchestrator's log, D1-D10: every story and scope call, and how to reverse it.
- `story/canon.md` — settled facts and rulings. **Canon beats the bible and the outlines**, in that
  order, and `003_the_warning` beats canon. Sections: the fifteen questions (1), rulings (2), Ket's
  light ledger (3), calendar/ages/party (4), the cross-act plant table (5), the name register (6),
  five prohibitions (7), branching scenes (10), continuity rulings (11).
- `story/bible.md` — the world. Sections 1 (titles), 2 (premise), 9 (the party), 12 (the chapter plan)
  are the ones worth reading; the rest is reference.
- `story/outline/act1-5.md` — **history.** Superseded wherever they disagree with canon; two are
  knowingly stale.
- `story/playlist.md` — play order, and **the tool's definition of what counts**. `## intro` is what
  the game actually plays; the `## chapter..` lists carry `(optional)` and `(branch: ...)` markers.
  `stats` and `packages` consider only the scenes some list names — a scene file in `story/scenes/`
  that no list names is a draft: no package, no count, no export. `--all` takes the folder as it stands.
- `story/v3/NAMES.md` — **the name table (D13)**. Every proper noun in the story text is a token:
  `{{HERO}}`, `{{HOME_TOWN}}`, speaker labels included. The files stay tokenised forever; every
  command substitutes the table's current name before it uses the text, so the game and ChatGPT only
  ever see real names and a rename is one line. A token with no row is a validation error;
  `./story_prompt.py names` prints the table and every token used that is missing from it. A token's
  value **never carries its own article** — the sentence writes `the {{STAIR}}` — and two articles in
  a row after substitution is an error. A token whose value is a note rather than a name reads as a
  plain phrase (`{{SEA_WALL}}` → "sea wall"). A speaker written as a token resolves token → name →
  handle or alias in `characters.md` exactly as an alias does, which is why a renamed character needs
  an `- alias:` line: without it the game draws no portrait and the tool says so.
- `story/characters.md` (54 entries), `story/locations.md` (311 keys), `story/plot.md` (short state file).
- `story/packages/` — generated but tracked: **the art tray**, see below.
- `story/notes/` — each agent's report. `continuity.md` has the ten weakest scenes; `editor.md` the
  five biggest risks; `cast-designer-2.md` §5 the reference-sheet priority order.

**Frozen wording.** Some text is quoted across many chapters and must be reproduced character for
character, never paraphrased: the Tally's fifteen questions, the guild's Four Lines, the hall-guards'
oath and the relief formula, the payroll line, the intake form, Crewe's list entry. Canon section 1
names every scene allowed to quote the Tally; no other scene may. `003_the_warning`'s panels, staging
and acting lines are frozen outright because its art is approved.

**Branch scenes.** The format has no selector, so a branching moment is **one complete scene file per
outcome**, suffixed `a`/`b`/`c` (`0845a/b/c`, `1340a/b`), each stating its condition in the first
paragraph of its `## Beat` and marked `(branch: ...)` in the playlist. The game must play exactly one
of a set; until a selector exists, only `0845c` is safe to ship.

**How big story work is done here.** Orchestrator-only: the main session writes a brief per agent,
spawns Opus subagents that each own an exclusive set of files, merges their output, rules on the
conflicts they report, and makes every commit. **Agents never commit, never touch `src/`,
`story_prompt.py`, `STYLE.md` or another agent's files, and never deploy to the phone.** Conflicts and
tool complaints go in `story/notes/<agent>.md` and the orchestrator rules on them in `DECISIONS.md`.

- **Never hand-write a final image prompt**, and never paste style text into one. Write a scene
  file in `story/scenes/NNN_slug.md` (copy `000_TEMPLATE.md`), then run the tool. It inserts the locked
  style blocks and each character's `look` verbatim, and exits non-zero if the scene breaks a
  composition rule (panel count, shot menu only, alternate scale, vary shape, anchor + punch, known
  cast, no style words, no text). Fix the scene; don't bypass the tool.
- **ChatGPT path (normal).** Images are generated by the owner in ChatGPT, one *shot sheet* per
  generation: all of a scene's panels as separate white-bordered rectangles on pure black, never
  overlapping, so they can be cut apart. The game layers the panels into the manga page itself.
  In day-to-day work the owner never runs the four commands below one at a time: `packages` writes
  every one of them into `story/packages/` as a folder with the prompt in it, and `ingest` does the
  cutting. The steps are what those two run underneath, and how to drive one scene by hand.
  1. `./story_prompt.py refsheet <Name>` once per character → owner generates it and saves it at the
     character's `ref:` path in `characters.md` (`story/refs/<name>.png`). Optional style reference
     at `story/refs/style.png`. That sheet's middle panel also becomes the character's in-game
     dialogue portrait — see "Portraits" below — so keep it head-and-shoulders.
  2. `./story_prompt.py sheet <scene> [more scenes]` → `story/out/<name>.chatgpt.md`: which files to
     attach in which order, and the prompt to paste. Max 6 panels per sheet; two 3-panel scenes fit one.
     The tool computes a varied layout (tall panels get a side column spanning most of the height,
     other rows stagger left and right) and states each panel's position and size as percentages.
     Without that, ChatGPT returns a uniform 2x2 grid with every panel near 1.4:1.
  3. `./story_prompt.py slice story/out/<name>.sheet.json <download>` → `story/panels/<scene>_pN_<shot>.png`.
     It auto-detects panels by their borders (works on dark interiors), matches them to the expected
     panels by position and proportion (not reading order), converts non-PNG via `sips`,
     and writes nothing if the count is wrong. Escape hatches: `--boxes "x,y,w,h;..."`, `--trim N` to
     shave the white border. Pure stdlib; no Pillow on this machine.
  4. `./story_prompt.py preview <scene>` → `story/out/<scene>.preview.html`: layers the sliced panels
     manga-style and reveals them line by line with a dialogue box (tap or space to advance; `#all`
     on the URL shows every panel at once). Missing panel images become labelled placeholders, so a
     scene's pacing can be tested before any art exists. This is the reference for the in-game player.
- **Talk scenes need no art.** `- type: talk` is a field conversation: no `## Panels`, no `[n]` reveal
  tags, `## Dialogue` only. `- backdrop: <scene_stem>:<panel_number>` optionally names an existing
  panel from another scene to show dimmed behind the box (the tool checks that the scene and panel
  exist; a not-yet-generated image is only a warning). `sheet` and `build` refuse a talk scene;
  `check`, `brief`, `export`, and `preview` handle it. Any speaker name works — one not in
  `characters.md` is a one-off NPC and just gets no portrait.
- **Handles and aliases.** A `## Name` in `characters.md` is the **handle**: the word a writer types
  in panel text and on a `characters:` line, matched case-insensitively as a whole word, which is why
  handles are never common words. A character known on screen by a different name gets an optional
  `- alias: Name[, Name...]` line. An alias may be used as a dialogue **speaker** and on a
  `characters:` line (it resolves to the handle for the portrait, the left/right side, and the
  cast-membership check), and the game prints it as the speaker name. Aliases are **never** matched
  inside panel descriptions — that is the whole point: `Nona` is called `Nine` on screen, and a handle
  of `Nine` would false-match "the Nine Doors". Two characters may not share an alias, an alias may
  not be another character's handle, and `Narrator` is reserved.
  **One exception, and it is the token table's:** an alias that is a name token's current value *is*
  matched in panel text, because with names tokenised it is the only form a writer types — `{{HERO}}`
  becomes `Falke` in the panel, and `Falke` has to find `## Bron`. The table's values are proper nouns
  by construction, which is what makes that safe. So a panel names a character by the token or by the
  handle, and the acting line under it uses whichever the panel used; a prompt then addresses them by
  the name the story uses now. A character with a token but no matching `- alias:` line validates but
  gets no portrait, and `check` says exactly that.
- **Chapter numbering.** A scene file is `<chapter><scene>_slug.md` in four digits: `0105` is chapter 1,
  `1595` chapter 15, `16xx` the epilogue. The intro files written before that scheme (`p01_prologue`,
  `001`-`003b`) count as chapter 1. `stats` and `packages` group by it.
- **`stats`** prints, per chapter, the scene count by type, panels, dialogue lines, optional scenes
  (`- optional: yes`) and which panel scenes have art, then the totals, the number of distinct
  speakers, and every speaker that is neither a handle, nor an alias, nor `Narrator` — the one-off
  NPCs, with the scenes that use them. Run it before a continuity pass.
- **`packages` is the art tray**, and the owner's loop is **generate → save as `returned.png` →
  `./story_prompt.py ingest` → `./fast_reload.sh`**. `packages` (re)builds `story/packages/` as a
  folder tree walked top to bottom — reference sheets first, because everything else attaches them:

  ```
  story/packages/README.md                      the index: the short path first, then everything, with status
  story/packages/cast/<name>/refsheet/          the character reference sheet  (folder named for the
  story/packages/cast/<name>/walker/            the 4x4 walk sheet              current name, as maps are)
  story/packages/ch01/<map>/tiles/              that map's ground and wall tiles
  story/packages/ch01/<map>/props, props_2/     split when one sheet's slots would be too small to draw in
  story/packages/ch01/<map>/scenes/<scene>/     a shot sheet per panel scene played on that map
  ```

  Every folder holds `prompt.md` (self-contained: what already exists, the files to attach in order,
  the prompt in one block, the review checklist), `sheet.json`, a `template.png` where the sheet is a
  template, `package.json` for the tool, and `RETURN_HERE.md` — the one instruction: save ChatGPT's
  image **in that folder** as `returned.png`. Maps come from the `- map:` lines on scenes and on
  `story/field/tiles.md` / `props.md` entries; an id used on several maps is drawn once with the first
  and listed as shared under the others, so no id is ever drawn twice. **A package that has been
  drawn is frozen**: once a returned image sits in it, or every file it makes exists, its id list,
  slot boxes and template stay exactly as they were, and ids added to `tiles.md` or `props.md`
  afterwards go into a fresh `tiles_2` beside it — otherwise adding one tile would re-shuffle a sheet
  the owner had already generated and the archived image could never be cut again. The README opens with **the
  short path to something on the phone**: a numbered to-do with status boxes — the missing reference
  sheets, then the walk sprites they unblock, then the first map's tiles and props, then the first
  scene's shot sheet — and the rest follows by map in chapter order.
  Only the cast the selected scenes actually use gets a `cast/<name>/` folder (a handle or alias on a
  `characters:` line or as a speaker), plus every `story/field/walkers.md` id that no cast entry has
  taken over. `--all` lifts that and the playlist filter together.
- **`ingest`** is the other half: it walks `story/packages/` (or one folder), finds every `returned.*`
  that is newer than what it makes, and runs the right cutter — `slice` for a shot sheet, `cut` for a
  template, and for a reference sheet it saves `story/refs/<name>.png` and cuts the dialogue portrait
  out of it. Non-PNG goes through `sips`. Before cutting it keeps the raw image in `story/sheets/`
  under the package's path (`ch01-halm-tiles.png`), skipping the copy when it is already there
  byte for byte, because `returned.png` itself is gitignored scratch. One line per package and a summary; non-zero exit if any
  failed; the returned file is **never deleted**, so a bad cut is redone by fixing and rerunning.
  `--force` redoes work that is already up to date. `cut` now refuses an image whose margins are not
  the template's background colour, which is what a file from the wrong package looks like.
- Both are safe to rerun at any time: `packages` rewrites prompts and templates, never a `returned.png`,
  and only deletes a folder that has left the playlist **and** holds nothing the owner generated (one
  that does is kept and listed at the end of the README). A scene that fails validation is reported at
  the end and does not stop the run. Regenerate after any change to a scene, `characters.md`,
  `story/field/*.md` or `STYLE.md`; never edit anything under `story/packages/` by hand.
- **Portraits.** `./story_prompt.py portraits` cuts every character's reference sheet down to its
  middle panel (the head-and-shoulders portrait the `refsheet` block asks for) and writes
  `story/portraits/<name>.png`. It reuses the same border detection as `slice`, needs exactly three
  panels on the sheet, and warns and skips otherwise. `fast_reload.sh` and `deploy.sh` run it before
  the export, so regenerating a reference sheet and hot reloading updates the in-game portrait.
- **Pages and sheet size.** A scene is 1-3 pages of 2-4 panels, at most 8 panels, with `---` in
  `## Panels` starting a new page (the game clears the screen, as Phantasy Star IV does). Aim for 6-8
  shots in a scene that matters. One sheet holds the whole scene: up to 4 panels use 1536x1024-class
  canvases, more use 2560x1440-class ones (the largest size OpenAI doesn't call experimental). Layout is
  by fixed relative shape sizes (`SHAPE_UNITS`) packed into staggered rows and scaled to fit, so wide
  panels are always the big ones. Two earlier search-based layout engines produced huge insets and tiny
  two-shots; don't go back to scoring functions.
- **The ChatGPT app caps every image at about 1.5 megapixels.** Asking for 1440x2560 returned 941x1672:
  the aspect is honored, the pixel count is not (the larger sizes in OpenAI's docs are API-only). Seven
  panels therefore share the pixels four used to have. It still looks right in game because pixel art
  scales cleanly, but past 8 panels, or for a scene that needs detail, make one sheet per page instead.
- **Getting a generated image onto the Mac:** images pasted into the Claude chat are capped at the same
  ~1.5 MP and stored as WebP in the session transcript, so prefer the original. The phone is on adb and
  ChatGPT saves to `/sdcard/Pictures/file_*.png`: list recent files with `adb shell find /sdcard/Pictures
  -mmin -60 -name "*.png"` and `adb pull` the newest. Keep it as `story/sheets/<scene>_vN.png`.
- `preview` pages accept `#land-line3` (or `#port-line3`) to jump to a dialogue line with its page built,
  which is how to check a page layout without touching the phone.
- The export includes a scene as soon as it has any panel art; missing panels show as dark placeholder
  boxes in the game, so a scene stays playable while its sheet is being regenerated.
- **Acting is mandatory.** A panel scene needs a `- staging:` line (fixed screen direction for the whole
  scene) and, under each panel, an indented acting line for every cast member in it: where their eyes
  point, their expression, their body. The `## Beat` paragraph is sent to ChatGPT as the situation, so
  write it for an artist. Without these ChatGPT draws catalogue poses: characters facing different ways
  with the reference sheet's neutral face, which is how the first two-shot failed.
- **Review before slicing.** When the owner sends a generated sheet, check it against the checklist in
  `story/out/<name>.chatgpt.md` (gaze, expression, screen direction, design, hands, sheet hygiene; see
  "Review checklist" in `STYLE.md`) and say what fails before running `slice`. Reject on gaze or
  expression even when the art is good, and give the owner the one-panel correction to paste back.
- Dialogue lines may carry a reveal tag, `- Lyra [2]: text`, naming the panel that appears with that
  line. Untagged lines reveal the next unseen panel. Several lines can hold on one panel.
- Raw generations live in `story/sheets/` (tracked) so anything can be re-cut later. `ingest` files
  them there itself, named for the package that asked for them (`ch01-halm-tiles.png`), before it
  cuts; a hand-driven `slice`/`cut` still wants a manual copy as `story/sheets/<scene>_vN.png`. The
  `returned.png` inside a package folder is gitignored scratch: overwrite it with the next attempt.
- Portrait and profile insets use a flat dark color from the scene's own palette. Reference sheets use
  a neutral grey portrait background, because ChatGPT copies a reference's background color into scenes.
- Generated reference sheets are the truth for a character: after saving one, edit the `look` line to
  match what was actually drawn. `story/refs/style.*` is gitignored (third-party screenshot).
- `./story_prompt.py build <scene>` is the older single-page mode (overlapping panels in one image) for
  generators without reference images. Its output cannot be sliced.
- **Writing the next scene:** `./story_prompt.py brief ["beat"]` prints the full writer's brief
  (rules, shot menu, cast, plot state, last two scenes). With no argument it uses `## Next beat`
  from `story/plot.md`. After the scene builds, update `plot.md` (beats so far, open threads,
  next beat) and any changed `status` in `characters.md`.
- To change the look, a shot, or a character design, edit `STYLE.md` / `characters.md`, then
  rebuild every scene with `build --all` so old prompts don't drift from new ones.
- Rule thresholds live in both `STYLE.md` (prose) and the top of `story_prompt.py` (constants). Change both.
- Dialogue is never rendered into images; the game draws it. `story/out/` is generated and gitignored;
  `story/refs/`, `story/panels/` and `story/packages/` are kept (the last is generated but tracked, so
  the owner can pick up any scene's package without running the tool).

### Field art — the template sheet (`tiles`, `props`, `walker`, `building`, `cut`)

The walking-around art in `FIELD.md` is generated the same way, except that **the tool draws the
sheet's layout itself** instead of asking ChatGPT to invent one. Each command writes three files to
`story/out/`: a `<name>.template.png` (flat magenta for props and walkers, black for tiles, with white
3-px bordered slots and a slot number painted in the gutter beside each corner), the usual
`<name>.chatgpt.md` package, and a `<name>.sheet.json` holding every slot box. The prompt is assembled
from the same locked blocks in `STYLE.md` — never hand-written — plus the per-slot descriptions and the
template rules: keep the borders and the numbers exactly where they are, draw nothing outside a slot,
leave the background untouched, no text.

- **What each slot is** comes from a data file, not from the command line. `story/field/tiles.md` has
  one `## <id>` per tile with a one-line description and `- kind: ground` (seen from straight above)
  or `- kind: wall` (seen front-on, repeats up a height step). `story/field/props.md` has the
  description and `- footprint: WxH`, the sprite's box in map cells at 64 px to the cell, which is
  what sizes its slot; `PROP_FOOTPRINTS` in the tool is the fallback table and an unknown id gets
  1x1 and a warning. A requested id with no entry is an error, as is a canvas too crowded to draw in.
- **Buildings are not sprites.** A house is map geometry — the `## buildings` section of a `.map` gives
  its footprint, height and rotation — wearing three textures, and `story/field/buildings.md` says what
  it is made of (`## <id>`, one-line description, `- map:`, optional `- wall:` and `- roof:` for the
  material samples). `building <id>...` draws three square slots per building and `cut` writes them to
  the engine's contract in `src/FIELD_NOTES.md` ("Face-texture contract"), which owns these numbers:
  `<id>_front.png` **128x128**, stretched *once* across the whole front wall, so a door and windows
  painted into that one square land where the artist put them whatever the building's width;
  `<id>_side.png` **128x128** seamless both ways, tiled per cell across and per world unit up on the
  back, ends and gables; `<id>_roof.png` **64x64** seamless, tiled per cell on each roof slope. All
  three are opaque. So the front slot is a whole wall seen square on and flat, and the side and roof
  slots are plain material with no opening and no landmark in them. Ids come from the maps; everything
  a person could walk around, the grain-yard wall included, stays a prop.
- **Walkers come from the cast.** `walker <Name>` takes the look from `characters.md` verbatim and
  attaches that character's reference sheet, so the sprite matches the portrait; a name with no
  reference sheet is an error, not a warning. A story name that has moved on (`story/v3/NAMES.md`:
  Falke was Bron) belongs on an `- alias:` line of the existing entry — never rename a handle — and
  the sprite is written under the name you typed. `story/field/walkers.md` is only for NPCs with no
  cast entry at all (the clerk, the villagers walking out of Halm).
- **Cutting needs no border detection**, because the tool made the boxes: `cut <sheet.json> <image>`
  scales them if ChatGPT returned a smaller image (it caps at ~1.5 MP), crops inside each border, keys
  the magenta to alpha with a soft fringe, and writes 64x64 tiles, props trimmed and scaled back to
  64 px per cell, one 128x192 walker sheet (rows S, W, E, N; columns stand, step-left, stand,
  step-right), and building faces resized to the contract sizes with any alpha dropped. `slice`
  redirects to it when the JSON is a template package. Nothing is written if the image is the wrong
  shape, or if its margins are not the template's background colour — that is what a file from the
  wrong package looks like.
- **Alpha is checked, not assumed.** Props and walkers are written RGBA (colour type 6) by
  construction, because the keying always produces four channels; what can still go wrong is a slot
  painted edge to edge, which keys nothing out and renders on the phone as a black rectangle. `cut`
  counts the transparent pixels in every prop and walker it writes and warns loudly when there are
  none, naming the file; `ingest` repeats that warning and counts it in its summary, so it cannot be
  swallowed. A real sprite comes out about a quarter transparent; a walk sheet about half.
- **Keying the magenta is by cast, not by distance** (the first real cut put a purple outline on the
  well and the cart). ChatGPT anti-aliases an object's edge into the key colour, and a black outline
  blended half and half with magenta is `rgb(126,0,128)` — *further* from pure magenta than a mid grey
  is, so no distance threshold can catch it without eating real colour. What measures it is the
  magenta cast, `min(r, b) - g`: for a blend with a roughly neutral foreground the cast is about 255
  times the magenta fraction, so `255 - cast` is the alpha. `cut` floods in from the background
  through every pixel that still carries a cast, sets each one's alpha from its cast, un-mattes the
  colour at that alpha, and then — because the engine alpha-tests at 0.5 and draws whatever survives
  at full strength — gives any pixel still looking purple the colour of its nearest clean opaque
  neighbour, keeping its own alpha. An object's interior is never touched, so paint may be any colour;
  only what the background reaches is cleaned. `--fringe N` erodes the alpha by N more pixels for a
  sheet that stays dirty anyway. On the thirteen Halm props this went from 3273 fringe pixels to 4
  (one with `--fringe 1`), with no holes punched in the sprites.
- **`story/field/manifest.md`** is regenerated on every one of those runs: every id ever requested,
  its kind, target size, the package that asked for it, and whether the file exists. Humans and agents
  read it; the engine does not.
- **Tile field (D18) — the main field-art path** (owner, 2026-09-19, final after a day of 2.5D,
  painted block-outs and painted screens: *"Sega style, top down, using tile sheets, grid walking. No
  more screen navmesh."*). `TILES.md` is the contract; **Screens and the 3D/painted-view field are
  parked** — the code stays, nothing new is built on them.
  A **tileset** is `story/field/tilesets/<set>/`: `tiles.md` (**the engine's file** — one `## <id>` per
  tile or stamp with `- index:`, `- layer:`, `- solid:`, `- desc:`), `atlas.png` (16 columns of 32x32
  tiles, index 0 empty), `atlas.json` and `cut/<sheet>.png`. The art pipeline never renames an entry,
  never changes a size or a solid row, and **never moves an index that is already written**; the one
  thing it writes back into `tiles.md` is a `- index:` line for an entry that has none.
  `./story_prompt.py tileset <set>` groups the entries into **as few sheets as it can** and writes one
  package per sheet under `story/packages/tilesets/<set>/<sheet>/`, listed **first** in the packages
  README. Two pools, not one per category: `terrain` (every ground tile **and** its fringes — same
  materials, one palette, so one **magenta** background with the ground slots painted *edge to edge*
  over it and only the fringes keyed) and `objects` (buildings, nature and props together, biggest
  first, each sheet then filled up with the single-tile stamps, so the barrels stand beside the guild
  hall). A `- sheet:` line still wins. Packing is a **skyline** — lowest then leftmost — with 20 px
  gutters, capped at **26 slots and 85% of the canvas**, numbered afterwards in reading order, and the
  canvas is **trimmed to the content** (ChatGPT returns ~1.5 MP whatever it is given, so a half-empty
  template throws half the detail away). This took valley from nine mostly-empty sheets to five at
  64-81% full. Slots stay at **exactly 4x**: a tile is a 128-px slot, a 4x3 stamp a 512x384 one, the
  white 3-px border is drawn *outside* the art area, and the prompt's loudest rule — its first
  paragraph — is that every art pixel must be a 4x4 block aligned to the slot. The prompt lists the
  slots grouped by kind (ground / fringe / stamp) with each one's size in tiles and the rules for that
  kind restated. A drawn sheet is frozen; new ids go to a fresh `<sheet>_2`.
  `ingest` cuts the return by taking each art pixel's **mode colour** over its exact region in the
  returned image — never an average, and never a resize first, because the sheet comes back at
  about 1.5 MP whatever was asked for and a non-integer resize puts the blocks half a pixel out of
  step. Keying is decided **per entry, never per sheet** (a terrain sheet holds opaque ground and
  keyed fringes over the same magenta): magenta is keyed on anything that is not opaque ground, and a
  ground tile that comes back with background showing through has those pixels filled from the nearest
  painted one — silently in the outermost ring, where it is only what a non-integer return does to a
  slot edge, and loudly anywhere further in. The alpha is **hard-thresholded at 0.5** (a tile is on or off; the engine alpha-tests), and each entry is packed into `atlas.png` at
  its own index without disturbing a cell that belongs to anything else. Ground tiles get a **seam
  check** — the mean colour difference across the wrap, warned above 24 — with `--heal-seams` to
  cross-blend two pixels at the edges, and `--palette N` to quantise a whole sheet by median cut.
  On a synthetic sheet a 1:1 return cuts back **pixel-exact**, and an 0.82x return to a mean absolute
  error of 0.1/255.
  **The fringe convention is fixed here and the engine rotates it** (written out in full in
  `story/field/tilesets/README.md`): a fringe tile carries **only its own terrain**, ragged, over
  transparency, and is laid on the neighbouring tile. `_edge` is a band along the **north** edge about
  10 of the tile's 32 px deep; `_corner_out` fills only the **north-east** corner about 10 px in from
  each; `_corner_in` is its inverse — everything except a ragged bite out of the **south-west**
  corner. Three tiles a terrain, rotated 90/180/270, instead of forty-seven.
  **Maps** are `story/field/tmaps/<map>.tmap`, the plain-text format in `TILES.md`.
  `./story_prompt.py tmap check <map>|--all` (also run by `check --all`) validates the legend against
  the tileset, every stamp's footprint and its `+` cells, overlaps, the spawn and every exit or door
  target standing on a non-solid tile, every `message`/`npc` id against `story/field/text.md`, and
  every exit's target map against the `.tmap` files or the `## todo` list in
  `story/field/tmaps/README.md`. `tmap preview <map>` renders the map from the atlas to
  `story/out/<map>.tmap.png`, drawing a flat labelled colour for any tile whose atlas cell is still
  empty, so a map can be judged on the Mac before any art exists.
  **`story/field/tilesets/**` and `story/field/tmaps/**` have to ship** — the push and bundle lists in
  `fast_reload.sh` and `deploy.sh` are the engine agent's to change; this side does not touch those
  scripts.
- **Screens (parked).** A screen was a top-down 16-bit map painted whole by ChatGPT with a
  green-on-black walkable mask traced over it, derived into `story/field/screens/<map>_<zone>.screen`
  (nav polygons, doors, exits) by `./story_prompt.py screen <map> <zone>`. The tooling still works and
  the packages still build; nothing new is authored against it.
- **Painted views (parked, the Final Fantasy VIII arrangement).** The 3D block-out stays —
  it is what collision, depth and the camera are computed from — and what the player sees is a
  painting laid over it. The engine captures a zone from the game's own camera to
  **`story/field/views/<map>_<zone>.png`** (1280x720 or 640x360, opaque); `./story_prompt.py view <map>
  <zone>` builds the package around that capture, and `ingest` writes the painting back to
  **`story/field/views/<map>_<zone>_paint.png` at exactly the capture's size**. Those two file names
  are the whole contract between the two streams. Without a capture the command exits non-zero and
  says so — there is nothing to paint over. The capture is attached **first** (it is the layout, not a
  reference), then the style sheet, then the cut sprites of what the map places, and the prompt's one
  hard rule is that nothing may move or change size, because a wall that shifted in the painting is a
  wall the player walks through. No people (the game draws those as sprites), no text, no interface,
  painted edge to edge. The painting is fitted with a **box filter**, not nearest: it comes back at
  about 1.5 MP whatever was asked for, so it is a small non-integer reduction, and nearest at 1.27x
  drops every fourth column and tears the straight edges that have to line up with the block-out.
  `--nearest` is there for a return that happens to be an exact integer multiple.
  **The push and bundle lists are the engine agent's to change** — `story/field/views/` is not in
  `fast_reload.sh` or `deploy.sh` yet, and this side does not touch those scripts.
- **Examine text is story, so it lives in the story tree**, not in the maps and not in the engine:
  `story/field/text.md`, one `## <map>.<id>` heading (`## halm.well`) per line of text, the text under
  it, tokens expected, and an optional `- name:` — an NPC's display name is story too, so it lives
  here and takes tokens like everything else. A map's `message` trigger names the id; `export` writes
  `src/field_text.h` (`struct FieldText { const char *id; const char *text; const char *name; }`,
  `FIELD_TEXT[]` sorted by id, `name` empty when the line has no speaker, `FIELD_TEXT_COUNT`) with the
  names substituted, and `check --all` validates it: two sentences at
  most, no unknown token, and nothing on the `BAN:` lists in `story/v3/STYLE.md` and
  `story/v3/SMELLS.md` — which the tool reads rather than copies, so "nobody" fails wherever it is
  written. A line left as `[halm.well: TODO]` is exported as it stands, so an unwritten line shows on
  the phone as a bracket rather than as silence.

## Index

- `src/` — all source. `star_logic.cpp` = the game, `host.cpp` = window/GL/hot reload, `cutscene_data.h` = generated, `game_logic.cpp` = old RPG (unbuilt)
- `shaders/` — GLSL 450 source; `compiled/` = generated, gitignored
- `assets/` — Roboto-Regular.ttf (also copied to android assets)
- `third_party/` — single-header libs (stb, par_shapes, FastNoiseLite, sokol)
- `android/` — Gradle project, SDLActivity, reuses root CMake
- `story/` — `STYLE.md` (locked art rules), `characters.md`, `plot.md`, `playlist.md` (what counts),
  `v3/NAMES.md` (the `{{TOKEN}}` table), `scenes/`, `panels/`, `refs/`, `portraits/` (generated from
  `refs/`, gitignored), `packages/` (the art tray: generated by `packages`, tracked, holds the owner's
  `returned.png` files); `story_prompt.py` builds the prompts, cuts what comes back, and exports
- `story/field/` — the walking-around art contract. **Main path (D18, `TILES.md`):** `tilesets/<set>/`
  (`tiles.md` the engine's list, `atlas.png`, `atlas.json`, `cut/`, and `README.md` with the fringe
  convention) and `tmaps/<map>.tmap` (the maps, plus `README.md` with the `## todo` map list).
  Parked: `screens.md` and `screens/` (painted maps and `.screen` files); the sprite path — `tiles.md`,
  `props.md`, `walkers.md`, `buildings.md` (what to draw), `maps/` (the maps themselves), `tiles/`,
  `props/`, `walkers/`, `buildings/` (the PNGs the game ships), `manifest.md` (generated)
