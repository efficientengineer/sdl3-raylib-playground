// settings.cpp — the PLAYER's settings (volume and mute), not the dev's.
//
//   owns      settings.ini beside the save: reading it, the debounced write, the STAR_MUTE override
//             that every Mac test run uses, and the ramped gain the mixer applies.
//   never     touches the Dev panel's state or the audio device — it sets g_set and audio.cpp reads
//             it, so a bad settings file can never silence the mixer permanently.
//   exposes   g_set, settings_save/load/tick and settings_path.
//   tested by ./capture.sh --robustness (D: volume and mute persist across a relaunch).
#include "star_internal.h"

// Volume and mute, in `settings.ini` in the pref dir: a plain two-line text file, deliberately NOT
// part of save.dat — that blob is a raw memcpy of Game, and adding a field to it changes the layout
// and invalidates every existing save (see CLAUDE.md, "Diagnosing crashes"). A separate file also
// survives a save being deleted, which is what a settings file should do.
//
// STAR_MUTE=1 in the environment forces mute for the session whatever the file says, and the forced
// state is NEVER written back. That is what every Mac test path sets: run_desktop.sh, capture.sh and
// perf.sh --desktop all export it, so no automated run can make noise in the owner's room. The
// Settings window shows why it is muted and still lets the owner untick it for that session.
StarSettings g_set;
const char *settings_path() {
    static char p[640];
    if (!p[0]) snprintf(p, sizeof p, "%ssettings.ini", pref_path());
    return p;
}

void settings_save(void) {
    // The forced-mute state is a property of the run, not of the owner's preference, so it is never
    // persisted: what goes in the file is the mute the owner actually chose.
    char buf[128];
    int n = snprintf(buf, sizeof buf, "volume=%.3f\nmuted=%d\n", g_set.volume,
                     (g_set.muted && !g_set.env_forced) ? 1 : 0);
    SDL_IOStream *io = SDL_IOFromFile(settings_path(), "w");
    if (!io) return;
    SDL_WriteIO(io, buf, (size_t)n);
    SDL_CloseIO(io);
    g_set.dirty = false;
}

void settings_load(void) {
    const char *e = SDL_getenv("STAR_MUTE");
    g_set.env_forced = e && e[0] && e[0] != '0';
    // Defaults: the phone ships unmuted at 80 %. The Mac ships MUTED, because every desktop run here
    // is a test run and the owner should never be startled by one.
#ifdef __ANDROID__
    g_set.volume = 0.80f; g_set.muted = false;
#else
    g_set.volume = 0.80f; g_set.muted = true;
#endif
    size_t sz = 0;
    char *text = (char *)SDL_LoadFile(settings_path(), &sz);
    if (text) {
        for (char *line = text; line && *line; ) {
            char *nl = strchr(line, '\n');
            if (nl) *nl = 0;
            if (!strncmp(line, "volume=", 7)) {
                float f = (float)atof(line + 7);
                if (f >= 0.0f && f <= 1.0f) g_set.volume = f;
            } else if (!strncmp(line, "muted=", 6)) {
                g_set.muted = atoi(line + 6) != 0;
            }
            line = nl ? nl + 1 : nullptr;
        }
        SDL_free(text);
    }
    if (g_set.env_forced) g_set.muted = true;
    g_set.gain = (g_set.muted ? 0.0f : g_set.volume);     // no ramp at startup: start where we are
    g_set.dirty = false; g_set.save_t = 0;
    SDL_Log("audio: volume=%.0f%% muted=%s%s", g_set.volume * 100.0f, g_set.muted ? "yes" : "no",
            g_set.env_forced ? " (forced by env STAR_MUTE)" : "");
}

// Debounced write, called once a frame. A slider drag is dozens of changes a second; one file write
// half a second after the last of them is the whole point.
void settings_tick(float dt) {
    if (!g_set.dirty) return;
    g_set.save_t += dt;
    if (g_set.save_t >= 0.5f) { g_set.save_t = 0; settings_save(); }
}
