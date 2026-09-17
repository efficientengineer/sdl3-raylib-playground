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
  `./story_prompt.py export` writes `src/cutscene_data.h` (text, panel layout, reveal order, music
  mood per line). `fast_reload.sh` and `deploy.sh` both run the export first, so editing a scene file
  and hot reloading is the whole loop. Never edit `cutscene_data.h` by hand.
- **Panel art** comes from `story/panels/`. `fast_reload.sh` pushes new or changed PNGs to the phone's
  `files/cutscenes/`; `deploy.sh` bundles them into the APK assets. The game looks in `files/` first.
  A playlist scene whose panels aren't all generated yet is skipped by the export, with a warning.
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
./dev_msg.sh "message"        # send dev message to in-game Messages tab
./watch_bugs.sh               # monitor bug reports on device
./story_prompt.py sheet <scene>  # scene file → ChatGPT package (prompt + refs to attach) in story/out/
./story_prompt.py slice <sheet.json> <img>  # cut the generated shot sheet into story/panels/
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
- **Reloads land while backgrounded.** Host sets `SDL_HINT_ANDROID_BLOCK_ON_PAUSE=0` so the SDL loop keeps running when the app is hidden; it skips rendering and polls the flag 4x/s, logging `Hot reload applied while backgrounded`. No need to foreground the app.
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

- **Never hand-write a final image prompt**, and never paste style text into one. Write a scene
  file in `story/scenes/NNN_slug.md` (copy `000_TEMPLATE.md`), then run the tool. It inserts the locked
  style blocks and each character's `look` verbatim, and exits non-zero if the scene breaks a
  composition rule (panel count, shot menu only, alternate scale, vary shape, anchor + punch, known
  cast, no style words, no text). Fix the scene; don't bypass the tool.
- **ChatGPT path (normal).** Images are generated by the owner in ChatGPT, one *shot sheet* per
  generation: all of a scene's panels as separate white-bordered rectangles on pure black, never
  overlapping, so they can be cut apart. The game layers the panels into the manga page itself.
  1. `./story_prompt.py refsheet <Name>` once per character → owner generates it and saves it at the
     character's `ref:` path in `characters.md` (`story/refs/<name>.png`). Optional style reference
     at `story/refs/style.png`.
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
- Keep the raw generations in `story/sheets/<scene>_vN.png` so a scene can be re-sliced later.
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
  `story/refs/` and `story/panels/` are kept.

## Index

- `src/` — all source. `star_logic.cpp` = the game, `host.cpp` = window/GL/hot reload, `cutscene_data.h` = generated, `game_logic.cpp` = old RPG (unbuilt)
- `shaders/` — GLSL 450 source; `compiled/` = generated, gitignored
- `assets/` — Roboto-Regular.ttf (also copied to android assets)
- `third_party/` — single-header libs (stb, par_shapes, FastNoiseLite, sokol)
- `android/` — Gradle project, SDLActivity, reuses root CMake
- `story/` — `STYLE.md` (locked art rules), `characters.md`, `plot.md`, `scenes/`; `story_prompt.py` builds prompts
