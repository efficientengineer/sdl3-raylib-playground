# CLAUDE.md

SDL3 + OpenGL playground. C11 own code, C++ deps allowed.

## Session startup

At the start of every session, ask the user if they want to start polling (bug reports + dev messages from the Android device).

Always hot reload (`./fast_reload.sh`) after making code changes — don't wait to be asked.

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
- `fast_reload.sh` compiles directly with NDK clang++, skipping gradle entirely (~0.7s vs ~3s).
- `hot_reload.sh android` uses gradle (slower but guaranteed correct flags).
- Reload flag: file with content = trigger reload; host truncates to 0 bytes after consuming.
- Host also checks the flag on `SDL_EVENT_DID_ENTER_FOREGROUND`, so a reload pushed while the app is backgrounded lands the moment it's brought back.
- Font size is fixed at startup (`22 * dpi_scale` in host.cpp); only style/padding can change via hot reload. `setup_touch_style` runs every tick so a bad deserialized `dpi_scale` can't stick.
- Serialization copies `Game` by `sizeof`; changing the struct between reloads misaligns the trailing fields. Restart the app after struct changes.
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
3. **Remember the hot-reload copies.** The app loads `files/libgame_logic.so`, not the APK's copy. After a
   struct change, `fast_reload.sh` alone is unsafe: the serialized state won't match. Force-stop and relaunch.
   If the app isn't running, the reload flag does nothing — relaunch with the `monkey` command above.
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

## Index

- `src/` — all C source (text.h/text.c = font renderer)
- `shaders/` — GLSL 450 source; `compiled/` = generated, gitignored
- `assets/` — Roboto-Regular.ttf (also copied to android assets)
- `third_party/` — single-header libs (stb, par_shapes, FastNoiseLite, sokol)
- `android/` — Gradle project, SDLActivity, reuses root CMake
