#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#include <android/log.h>
#define LOG_TAG "QuestGlory"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#include <OpenGL/gl3.h>
#define LOGI(...) printf(__VA_ARGS__)
#endif

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#include "game_api.h"

#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>

struct HotReloader {
    void *lib_handle;
    GameAPI api;
    char lib_path[512];
    char flag_path[512];
    char *state_buf;
    size_t state_buf_size;
    size_t state_size;
    time_t last_mod_time;
    int reload_gen;

    bool load(const char *path) {
        lib_handle = dlopen(path, RTLD_NOW);
        if (!lib_handle) {
            LOGI("dlopen failed: %s\n", dlerror());
            return false;
        }
        auto get_api = (GetGameAPIFunc)dlsym(lib_handle, "get_game_api");
        if (!get_api) {
            LOGI("dlsym get_game_api failed: %s\n", dlerror());
            dlclose(lib_handle);
            lib_handle = nullptr;
            return false;
        }
        api = get_api();
        LOGI("Hot reload: loaded %s\n", path);
        return true;
    }

    void unload() {
        if (!lib_handle) return;
        dlclose(lib_handle);
        lib_handle = nullptr;
        memset(&api, 0, sizeof(api));
    }

    bool check_reload_flag() {
        SDL_IOStream *f = SDL_IOFromFile(flag_path, "rb");
        if (f) {
            Sint64 sz = SDL_GetIOSize(f);
            SDL_CloseIO(f);
            if (sz <= 0) return false;
            f = SDL_IOFromFile(flag_path, "wb");
            if (f) SDL_CloseIO(f);
            return true;
        }
        return false;
    }

    bool check_lib_modified() {
#ifndef __ANDROID__
        struct stat st;
        if (stat(lib_path, &st) == 0) {
            if (st.st_mtime != last_mod_time) {
                last_mod_time = st.st_mtime;
                return true;
            }
        }
#endif
        return false;
    }

    // Returns the size of the file at `path`, or -1 if it can't be opened.
    static Sint64 file_size(const char *path) {
        SDL_IOStream *f = SDL_IOFromFile(path, "rb");
        if (!f) return -1;
        Sint64 sz = SDL_GetIOSize(f);
        SDL_CloseIO(f);
        return sz;
    }

    bool try_reload(float dpi_scale) {
        bool should_reload = check_reload_flag();
#ifndef __ANDROID__
        if (!should_reload) should_reload = check_lib_modified();
#endif
        if (!should_reload) return false;

        LOGI("Hot reload triggered!\n");

        // Refuse to touch the running game if the new lib isn't there. This
        // happens when the flag lands before the .so (or the push failed).
        Sint64 new_size = file_size(lib_path);
        if (new_size <= 0) {
            LOGI("Hot reload SKIPPED: %s missing or empty\n", lib_path);
            return false;
        }

        // Serialize current state
        state_size = 0;
        if (api.state && api.serialize)
            state_size = api.serialize(api.state, state_buf, state_buf_size);

        // Load the new lib *before* destroying the old game so a bad .so
        // leaves the running game untouched instead of a blank screen.
        void *old_handle = lib_handle;
        GameAPI old_api = api;
#ifdef __ANDROID__
        // On Android, copy .so to a unique name and dlopen that instead of
        // dlclose+dlopen the same path. dlclose can cause the app to lose
        // foreground focus. Old copies leak memory but that's fine for dev.
        // The name includes the pid so a restarted process never reuses a
        // name the linker may still have cached from a previous run.
        char reload_path[512];
        snprintf(reload_path, sizeof(reload_path), "%s.%d.%d", lib_path, (int)getpid(), ++reload_gen);
        Sint64 copied = 0;
        {
            SDL_IOStream *src = SDL_IOFromFile(lib_path, "rb");
            if (src) {
                SDL_IOStream *dst = SDL_IOFromFile(reload_path, "wb");
                if (dst) {
                    char copybuf[16384];
                    size_t n;
                    while ((n = SDL_ReadIO(src, copybuf, sizeof(copybuf))) > 0)
                        copied += (Sint64)SDL_WriteIO(dst, copybuf, n);
                    SDL_CloseIO(dst);
                }
                SDL_CloseIO(src);
            }
        }
        if (copied != new_size) {
            LOGI("Hot reload FAILED: copy %lld/%lld bytes to %s\n",
                 (long long)copied, (long long)new_size, reload_path);
            return false;
        }
        lib_handle = nullptr;
        memset(&api, 0, sizeof(api));
        if (!load(reload_path)) {
            LOGI("Hot reload FAILED: keeping previous game logic\n");
            lib_handle = old_handle;
            api = old_api;
            return false;
        }
#else
        unload();
        if (!load(lib_path)) {
            LOGI("Hot reload FAILED: keeping previous game logic\n");
            lib_handle = old_handle;
            api = old_api;
            return false;
        }
#endif
        (void)old_handle;

        if (old_api.state && old_api.destroy)
            old_api.destroy(old_api.state);

        api.state = api.create(dpi_scale);
        if (api.state && api.deserialize && state_size > 0)
            api.deserialize(api.state, state_buf, state_size);

        LOGI("Hot reload SUCCESS (%lld bytes, gen %d)\n", (long long)new_size, reload_gen);
        return true;
    }
};

// UI scale from the SHORTER screen edge so portrait and landscape agree
// (1080x2400 phone -> 3.0 either way). Desktop windows stay at 1.0.
static float compute_dpi_scale(int w, int h) {
    int m = w < h ? w : h;
    return (m > 1000) ? m / 360.0f : 1.0f;
}
// Base font size before the game's FontGlobalScale. 10px * scale reproduces
// the preferred look (~30px on the phone), which the game scales x1.3.
static float font_px(float dpi_scale) { return 10.0f * dpi_scale; }

int main(int argc, char *argv[]) {
    // Keep the SDL loop running while backgrounded so hot reloads land
    // immediately instead of waiting for the app to return to the foreground.
    // The loop skips rendering while in_background is set (no EGL surface).
    SDL_SetHint(SDL_HINT_ANDROID_BLOCK_ON_PAUSE, "0");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __ANDROID__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *win = SDL_CreateWindow("fungame", 800, 600,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_FULLSCREEN);
    SDL_GLContext gl_ctx = SDL_GL_CreateContext(win);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    float dpi_scale = compute_dpi_scale(w, h);

#ifdef __ANDROID__
    const char *font_path = "Roboto-Regular.ttf";
#else
    const char *font_path = "assets/Roboto-Regular.ttf";
#endif
    // Font is added at a nominal size only; the live size is set every frame
    // via style.FontSizeBase (ImGui 1.92+ dynamic fonts), so it tracks
    // rotation and the transient sizes SDL reports during startup.
    float font_size = font_px(dpi_scale);
    {
        int lw, lh; SDL_GetWindowSize(win, &lw, &lh);
        LOGI("Startup size: pixels %dx%d, logical %dx%d, dpi_scale %.3f, font_px %.1f\n",
             w, h, lw, lh, dpi_scale, font_size);
    }
    size_t font_data_size = 0;
    void *font_data = SDL_LoadFile(font_path, &font_data_size);
    if (font_data) {
        ImFontConfig fc;
        fc.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF(font_data, (int)font_data_size, font_size, &fc);
    }

#ifdef __ANDROID__
    ImGui_ImplSDL3_InitForOpenGL(win, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 300 es");
#else
    ImGui_ImplSDL3_InitForOpenGL(win, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 330");
#endif

    // Set up hot reloader
    HotReloader reloader = {};
    reloader.state_buf_size = 1024 * 1024;
    reloader.state_buf = (char *)malloc(reloader.state_buf_size);
    const char *pref = SDL_GetPrefPath("com.playground", "questglory");
    LOGI("SDL_GetPrefPath returned: [%s]\n", pref ? pref : "NULL");

#ifdef __ANDROID__
    snprintf(reloader.lib_path, sizeof(reloader.lib_path),
             "%slibgame_logic.so", pref);
    // Try hot-reload path first, then bundled APK lib
    if (!reloader.load(reloader.lib_path)) {
        // Bundled .so is loaded by short name (Android linker finds it)
        snprintf(reloader.lib_path, sizeof(reloader.lib_path),
                 "libgame_logic.so");
        reloader.load(reloader.lib_path);
        // But set lib_path to the hot-reload location for future reloads
        snprintf(reloader.lib_path, sizeof(reloader.lib_path),
                 "%slibgame_logic.so", pref);
    }
#else
    // On desktop, load from build directory
    const char *exe_dir = SDL_GetBasePath();
    snprintf(reloader.lib_path, sizeof(reloader.lib_path),
             "%slibgame_logic.dylib", exe_dir);
    reloader.load(reloader.lib_path);
#endif

    snprintf(reloader.flag_path, sizeof(reloader.flag_path),
             "%sreload.flag", pref);
    LOGI("Hot reload lib_path: [%s]\n", reloader.lib_path);
    LOGI("Hot reload flag_path: [%s]\n", reloader.flag_path);

    if (!reloader.api.create) {
        LOGI("FATAL: Could not load game logic library\n");
        return 1;
    }

    // Initialize game state
    reloader.api.state = reloader.api.create(dpi_scale);

    int frame_counter = 0;
    bool running = true;

    // SDL3 does NOT queue the app lifecycle events (SDL_EVENT_*_BACKGROUND /
    // _FOREGROUND); SDL_SendAppEvent hands them only to event watchers, so
    // checking them in the SDL_PollEvent loop never fires. The watcher runs on
    // the SDL thread from inside SDL_PumpEvents, so plain fields are safe.
    struct Lifecycle { bool in_background; bool just_foregrounded; HotReloader *reloader; } lc = { false, false, &reloader };
    SDL_AddEventWatch([](void *ud, SDL_Event *e) -> bool {
        Lifecycle *lc = (Lifecycle *)ud;
        if (e->type == SDL_EVENT_DID_ENTER_BACKGROUND || e->type == SDL_EVENT_TERMINATING) {
            LOGI("Entered background\n");
            if (lc->reloader->api.state && lc->reloader->api.on_save_event)
                lc->reloader->api.on_save_event(lc->reloader->api.state);
            lc->in_background = true;
        } else if (e->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
            LOGI("Entered foreground\n");
            lc->in_background = false;
            lc->just_foregrounded = true;
        }
        return true;
    }, &lc);

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) {
                if (reloader.api.state && reloader.api.on_save_event)
                    reloader.api.on_save_event(reloader.api.state);
                running = false;
            }
        }

        if (lc.just_foregrounded) {
            lc.just_foregrounded = false;
            reloader.try_reload(dpi_scale);
            frame_counter = 0;
        }

        if (lc.in_background) {
            // No surface to draw on. Poll the reload flag ~4x/s and idle.
            // Reload only swaps game logic (no GL), so it's safe here.
            if (reloader.try_reload(dpi_scale))
                LOGI("Hot reload applied while backgrounded\n");
            SDL_Delay(250);
            continue;
        }

        // Check for hot reload every 60 frames (~1 second)
        if (++frame_counter >= 60) {
            frame_counter = 0;
            reloader.try_reload(dpi_scale);
        }

        // Check if game wants to quit
        if (reloader.api.state && reloader.api.wants_quit &&
            reloader.api.wants_quit(reloader.api.state)) {
            if (reloader.api.on_save_event)
                reloader.api.on_save_event(reloader.api.state);
            running = false;
        }

        SDL_GetWindowSizeInPixels(win, &w, &h);
        dpi_scale = compute_dpi_scale(w, h);
        font_size = font_px(dpi_scale);
        ImGui::GetStyle().FontSizeBase = font_size;
        {
            static int last_w = -1, last_h = -1;
            if (w != last_w || h != last_h) {
                int lw, lh; SDL_GetWindowSize(win, &lw, &lh);
                LOGI("Size changed: pixels %dx%d, logical %dx%d, dpi_scale %.3f, font_px %.1f\n",
                     w, h, lw, lh, dpi_scale, font_size);
                last_w = w; last_h = h;
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        if (reloader.api.state && reloader.api.tick)
            reloader.api.tick(reloader.api.state, w, h, dpi_scale);

        ImGui::Render();
        glViewport(0, 0, w, h);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(win);
    }

    if (reloader.api.state && reloader.api.destroy)
        reloader.api.destroy(reloader.api.state);
    reloader.unload();
    free(reloader.state_buf);

    if (font_data) SDL_free(font_data);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
