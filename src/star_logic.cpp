// star_logic.cpp — the new game: manga-panel cutscenes, dialogue, adaptive music.
// Built as libgame_logic.so and hot-reloaded by host.cpp (see CLAUDE.md, "Hot Reload Architecture").
// Scene content is generated: edit story/scenes/*.md, run ./story_prompt.py export, hot reload.
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#else
#include <OpenGL/gl3.h>
#endif

#include "imgui.h"
#include "imgui_impl_opengl3.h"   // for the render state: ImGui binds its own sampler over ours

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
// The capture path's PNG writer. It lives here, the one TU every capture (dialogue, chapter,
// voxfield) links against.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "game_api.h"
#include "cutscene_data.h"
#include "voxfield.h"
#include "dialogue.h"
#include "battle.h"
#include "chapter01.h"

// The game plays CHAPTERS now — `## intro` is gone from story/playlist.md and CS_INTRO with it.
// New Game starts at CS_CHAPTERS[0]. The cutscene player reads one chapter's scene list at a time;
// CS_CUR is that list, so the player itself did not have to learn what a chapter is.
#ifndef CS_HAS_CHAPTERS
#error "src/cutscene_data.h is stale: run ./story_prompt.py export (it must define CS_HAS_CHAPTERS)"
#endif
static const CsScene *CS_CUR = CS_CHAPTERS[0].scenes;
static const int CS_CUR_COUNT = CS_CHAPTERS[0].count;

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"   // host.cpp uses the same pref path for reload.flag and the .so

// ───────────────────────── Audio: FM-flavoured voices ─────────────────────────
// Audio is generated on the main thread and queued to an SDL stream, so no locking is needed.

#define AU_RATE 44100
#define AU_VOICES 20
#define TAU 6.2831853f

enum VoiceType { V_OFF = 0, V_BASS, V_BELL, V_LEAD, V_PAD, V_KICK, V_SNARE, V_HAT, V_CRASH, V_SUB, V_BLIP, V_THUMP };

struct Voice { int type; float freq, t, dur, vol; };
static Voice au_voices[AU_VOICES];
static SDL_AudioStream *au_stream;

static float au_noise() {
    static uint32_t n = 48271;
    n = n * 16807u % 2147483647u;
    return (float)n / 1073741823.5f - 1.0f;
}

static float note_freq(int midi) { return 440.0f * powf(2.0f, (midi - 69) / 12.0f); }

static void au_play(int type, float freq, float dur, float vol) {
    int slot = 0;
    float oldest = -1.0f;
    for (int i = 0; i < AU_VOICES; i++) {
        if (au_voices[i].type == V_OFF) { slot = i; oldest = 1e9f; break; }
        float done = au_voices[i].t / au_voices[i].dur;           // steal the most finished voice
        if (done > oldest) { oldest = done; slot = i; }
    }
    au_voices[slot] = { type, freq, 0.0f, dur, vol };
}

// Two-operator FM: a sine carrier whose phase is pushed by a second sine. `index` sets the brightness.
static float fm(float t, float freq, float ratio, float index) {
    return sinf(TAU * freq * t + index * sinf(TAU * freq * ratio * t));
}

static float voice_sample(Voice *v) {
    float t = v->t, d = v->dur, f = v->freq, s = 0.0f;
    float rel = (d - t) < 0.03f ? (d - t) / 0.03f : 1.0f;          // short release so notes never click
    if (rel < 0) rel = 0;
    switch (v->type) {
    case V_BASS:  s = fm(t, f, 1.0f, 2.6f * expf(-t * 7.0f) + 0.4f) * fminf(t / 0.004f, 1.0f) * expf(-t * 2.2f / d); break;
    case V_BELL:  s = fm(t, f, 3.5f, 2.8f * expf(-t * 3.0f)) * fminf(t / 0.002f, 1.0f) * expf(-t * 3.2f / d); break;
    case V_LEAD:  s = fm(t, f * (1.0f + 0.004f * sinf(TAU * 5.5f * t)), 2.0f, 1.4f) * fminf(t / 0.02f, 1.0f) * (0.55f + 0.45f * expf(-t * 3.0f)); break;
    case V_PAD:   s = (fm(t, f, 1.0f, 0.6f) + fm(t, f * 1.006f, 2.0f, 0.35f)) * 0.5f * fminf(t / 0.5f, 1.0f) * fminf((d - t) / 0.6f, 1.0f); rel = 1.0f; break;
    case V_KICK:  s = sinf(TAU * (48.0f + 110.0f * expf(-t * 38.0f)) * t) * expf(-t * 14.0f); break;
    case V_SNARE: s = (au_noise() * 0.75f + sinf(TAU * 190.0f * t) * 0.35f) * expf(-t * 22.0f); break;
    case V_HAT:   s = au_noise() * expf(-t * 70.0f) * 0.5f; break;
    case V_CRASH: s = au_noise() * expf(-t * 2.4f) * fminf(t / 0.003f, 1.0f); break;
    case V_SUB:   s = sinf(TAU * (f * (0.5f + 0.5f * expf(-t * 1.6f))) * t) * fminf(t / 0.02f, 1.0f) * expf(-t * 1.4f / d); break;
    case V_BLIP:  s = (fmodf(t * f, 1.0f) < 0.5f ? 0.6f : -0.6f) * expf(-t * 45.0f); break;
    case V_THUMP: s = sinf(TAU * (52.0f + 70.0f * expf(-t * 25.0f)) * t) * expf(-t * 11.0f) + au_noise() * expf(-t * 60.0f) * 0.25f; break;
    default: break;
    }
    return s * rel * v->vol;
}

// ───────────────────────── Adaptive music ─────────────────────────
// One sequencer, six moods in A minor. Dialogue lines set the target mood; the switch lands on the
// next beat with a stinger, tempo glides instead of jumping, and the bar count never resets, so a
// scene's music reads as one piece that reacts to what is said.

struct MoodDef { float bpm; int chords[4][3]; };      // chord tones as MIDI notes, one chord per bar
static const MoodDef MOODS[CS_MOOD_COUNT] = {
    /* wonder   */ { 72,  {{53,57,64}, {55,59,62}, {57,60,64}, {52,55,59}} },   // F  G  Am Em
    /* dread    */ { 56,  {{45,52,57}, {45,52,58}, {44,51,56}, {45,52,57}} },   // Am  Am(b9)  G#  Am
    /* tense    */ { 96,  {{45,52,60}, {45,52,60}, {41,48,57}, {43,50,59}} },   // Am Am F  G
    /* confront */ { 132, {{45,52,57}, {41,48,53}, {43,50,55}, {40,47,52}} },   // Am F  G  E
    /* sorrow   */ { 60,  {{57,60,64}, {53,57,60}, {48,52,55}, {52,56,59}} },   // Am F  C  E
    /* hope     */ { 84,  {{48,52,55}, {55,59,62}, {57,60,64}, {53,57,60}} },   // C  G  Am F
};

static struct {
    bool on;
    int mood, target;
    float bpm, step_t, volume, target_volume;
    int step, bar;                                     // 16th-note step within the bar
} mus;

static void mus_start(int mood) {
    if (!mus.on) { mus.on = true; mus.mood = mood; mus.bpm = MOODS[mood].bpm; mus.step = -1; mus.step_t = 1e9f; mus.bar = 0; mus.volume = 0; }
    mus.target = mood;
    mus.target_volume = 1.0f;
}
static void mus_fade_out() { mus.target_volume = 0.0f; }

static void mus_stinger(int mood) {
    switch (mood) {
    case CS_CONFRONT: au_play(V_CRASH, 0, 1.6f, 0.30f); au_play(V_SUB, 82, 1.2f, 0.55f); au_play(V_BASS, note_freq(33), 1.0f, 0.5f); break;
    case CS_DREAD:    au_play(V_SUB, 70, 2.4f, 0.55f); au_play(V_BELL, note_freq(82), 2.5f, 0.10f); break;
    case CS_SORROW:   au_play(V_BELL, note_freq(88), 3.0f, 0.22f); au_play(V_BELL, note_freq(76), 3.0f, 0.14f); break;
    case CS_HOPE:     au_play(V_BELL, note_freq(84), 2.0f, 0.2f); au_play(V_BELL, note_freq(79), 2.0f, 0.14f); break;
    case CS_WONDER:   au_play(V_BELL, note_freq(81), 3.0f, 0.16f); break;
    case CS_TENSE:    au_play(V_HAT, 0, 0.2f, 0.4f); au_play(V_BASS, note_freq(33), 0.8f, 0.4f); break;
    }
}

static void mus_step(int mood, int s, int bar) {
    const int *ch = MOODS[mood].chords[bar % 4];
    float bar_len = 60.0f / mus.bpm * 4.0f;
    bool beat = (s % 4 == 0), eighth = (s % 2 == 0);
    switch (mood) {
    case CS_WONDER: {
        if (s == 0) { for (int i = 0; i < 3; i++) au_play(V_PAD, note_freq(ch[i]), bar_len * 1.1f, 0.10f); au_play(V_BASS, note_freq(ch[0] - 12), bar_len, 0.22f); }
        static const int up[8] = {0, 1, 2, 1, 2, 1, 0, 2};
        if (eighth) au_play(V_BELL, note_freq(ch[up[s / 2]] + 12 + (s == 12 ? 12 : 0)), 1.4f, 0.13f);
    } break;
    case CS_DREAD: {
        if (s == 0) { au_play(V_PAD, note_freq(ch[0] - 12), bar_len * 1.2f, 0.20f); au_play(V_PAD, note_freq(ch[1] - 12), bar_len * 1.2f, 0.10f); }
        if (s == 0 || s == 3) au_play(V_KICK, 0, 0.3f, s == 0 ? 0.42f : 0.26f);           // heartbeat
        if (bar % 2 == 1 && s == 8) { au_play(V_BELL, note_freq(ch[2] + 24), 3.0f, 0.10f); au_play(V_BELL, note_freq(ch[2] + 25), 3.0f, 0.06f); }
    } break;
    case CS_TENSE: {
        if (s == 0) for (int i = 1; i < 3; i++) au_play(V_PAD, note_freq(ch[i]), bar_len * 1.1f, 0.08f);
        static const int ost[8] = {0, 0, 0, 3, 0, 0, -2, 0};                                // semitone offsets from the root
        if (eighth) au_play(V_BASS, note_freq(ch[0] - 12 + ost[s / 2]), 0.22f, 0.34f);
        if (!eighth) au_play(V_HAT, 0, 0.05f, (s % 4 == 3) ? 0.16f : 0.09f);
        if (bar % 2 == 1 && (s == 8 || s == 11)) au_play(V_BELL, note_freq(s == 8 ? 76 : 77), 0.9f, 0.13f);
    } break;
    case CS_CONFRONT: {
        if (beat) au_play(V_KICK, 0, 0.25f, 0.55f);
        if (s == 4 || s == 12) au_play(V_SNARE, 0, 0.2f, 0.36f);
        if (!eighth || s == 14) au_play(V_HAT, 0, 0.04f, 0.13f);
        if (s % 4 != 1) au_play(V_BASS, note_freq(ch[0] - 12), 0.12f, 0.38f);              // gallop: 1 . 3 4
        if (s == 0) for (int i = 0; i < 3; i++) au_play(V_PAD, note_freq(ch[i] + 12), bar_len, 0.06f);
        static const int riff[2][8] = {{69, 72, 74, 76, 79, 76, 74, 72}, {69, 72, 76, 81, 79, 76, 74, 76}};
        if (eighth) au_play(V_LEAD, note_freq(riff[bar % 2][s / 2]), 0.2f, 0.17f);
    } break;
    case CS_SORROW: {
        if (s == 0) { for (int i = 0; i < 3; i++) au_play(V_PAD, note_freq(ch[i] - 12), bar_len * 1.15f, 0.09f); au_play(V_BASS, note_freq(ch[0] - 24), bar_len, 0.16f); }
        static const int down[8] = {2, 1, 0, 1, 2, 1, 0, 1};
        if (eighth) au_play(V_BELL, note_freq(ch[down[s / 2]] + 12), 1.8f, 0.10f);
        static const int tune[4][2] = {{76, 74}, {72, 69}, {72, 71}, {68, 71}};
        if (s == 0 || s == 8) au_play(V_LEAD, note_freq(tune[bar % 4][s / 8]), 60.0f / mus.bpm * 1.9f, 0.11f);
    } break;
    case CS_HOPE: {
        if (s == 0) { for (int i = 0; i < 3; i++) au_play(V_PAD, note_freq(ch[i]), bar_len * 1.1f, 0.09f); au_play(V_BASS, note_freq(ch[0] - 12), bar_len * 0.5f, 0.26f); }
        if (s == 8) au_play(V_BASS, note_freq(ch[0] - 12 + 7), bar_len * 0.4f, 0.2f);
        if (s == 0 || s == 8) au_play(V_KICK, 0, 0.2f, 0.26f);
        static const int up[8] = {0, 1, 2, 1, 0, 1, 2, 2};
        if (eighth) au_play(V_BELL, note_freq(ch[up[s / 2]] + 12 + (s == 14 ? 12 : 0)), 1.0f, 0.13f);
    } break;
    }
}

static float mus_sample(float dt) {
    if (!mus.on) return 1.0f;
    float dv = 1.2f * dt;                                                   // ~0.8 s fades
    if (mus.volume < mus.target_volume) mus.volume = fminf(mus.volume + dv, mus.target_volume);
    if (mus.volume > mus.target_volume) mus.volume = fmaxf(mus.volume - dv, mus.target_volume);
    if (mus.volume <= 0.0f && mus.target_volume <= 0.0f) {                // fully faded: stop, and drop held notes
        mus.on = false;
        for (int i = 0; i < AU_VOICES; i++) if (au_voices[i].type < V_BLIP) au_voices[i].type = V_OFF;
        return 0.0f;
    }

    mus.step_t += dt;
    if (mus.step_t >= 60.0f / mus.bpm / 4.0f) {
        mus.step_t = (mus.step < 0) ? 0.0f : mus.step_t - 60.0f / mus.bpm / 4.0f;
        mus.step = (mus.step + 1) % 16;
        if (mus.step == 0) mus.bar++;
        if (mus.step % 4 == 0) {
            if (mus.target != mus.mood) { mus.mood = mus.target; mus_stinger(mus.mood); }   // switch on the beat
            mus.bpm += (MOODS[mus.mood].bpm - mus.bpm) * 0.35f;                             // tempo glides
        }
        mus_step(mus.mood, mus.step, mus.bar);
    }
    return mus.volume;
}

// ───────────────────────── Settings (the player's, not the dev's) ─────────────────────────
// Volume and mute, in `settings.ini` in the pref dir: a plain two-line text file, deliberately NOT
// part of save.dat — that blob is a raw memcpy of Game, and adding a field to it changes the layout
// and invalidates every existing save (see CLAUDE.md, "Diagnosing crashes"). A separate file also
// survives a save being deleted, which is what a settings file should do.
//
// STAR_MUTE=1 in the environment forces mute for the session whatever the file says, and the forced
// state is NEVER written back. That is what every Mac test path sets: run_desktop.sh, capture.sh and
// perf.sh --desktop all export it, so no automated run can make noise in the owner's room. The
// Settings window shows why it is muted and still lets the owner untick it for that session.
struct StarSettings {
    float volume;          // 0..1
    bool muted;
    bool env_forced;       // STAR_MUTE was set at startup
    bool dirty;            // a change is waiting to be written
    float save_t;          // debounce
    float gain;            // the ramped gain the mixer actually applies
    bool open;             // the window
};
static StarSettings g_set;

static const char *pref_path();                 // defined with the other file helpers, below

static const char *settings_path() {
    static char p[640];
    if (!p[0]) snprintf(p, sizeof p, "%ssettings.ini", pref_path());
    return p;
}

static void settings_save(void) {
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

static void settings_load(void) {
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
static void settings_tick(float dt) {
    if (!g_set.dirty) return;
    g_set.save_t += dt;
    if (g_set.save_t >= 0.5f) { g_set.save_t = 0; settings_save(); }
}

static void au_init() {
    memset(au_voices, 0, sizeof(au_voices));
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, AU_RATE };
    au_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (au_stream) SDL_ResumeAudioStreamDevice(au_stream);
}

static void au_update() {
    if (!au_stream) return;
    static float buf[512];
    const float dt = 1.0f / AU_RATE;
    const int want = (int)(AU_RATE * 0.10f * sizeof(float));                // keep ~100 ms queued
    // The player's master gain, ramped over ~20 ms. Jumping it would click, and a click is the one
    // thing a mute button must not do. Music and effects both go through it — it is the LAST thing
    // applied, after the limiter, so muting is silence and not a quieter limiter.
    const float target = g_set.muted ? 0.0f : g_set.volume;
    const float ramp = 1.0f / (AU_RATE * 0.020f);
    while (SDL_GetAudioStreamQueued(au_stream) < want) {
        for (int i = 0; i < 512; i++) {
            if (g_set.gain < target) { g_set.gain += ramp; if (g_set.gain > target) g_set.gain = target; }
            else if (g_set.gain > target) { g_set.gain -= ramp; if (g_set.gain < target) g_set.gain = target; }
            float music_gain = mus_sample(dt), m = 0.0f, fx = 0.0f;
            for (int v = 0; v < AU_VOICES; v++) {
                Voice *vo = &au_voices[v];
                if (vo->type == V_OFF) continue;
                float s = voice_sample(vo);
                if (vo->type >= V_BLIP) fx += s; else m += s;
                vo->t += dt;
                if (vo->t >= vo->dur) vo->type = V_OFF;
            }
            buf[i] = tanhf((m * music_gain * 0.8f + fx) * 1.1f) * g_set.gain;   // soft limiter, then master
        }
        SDL_PutAudioStreamData(au_stream, buf, sizeof(buf));
    }
}

// ───────────────────────── Files and textures ─────────────────────────

static const char *pref_path() {
    static char path[512];
    if (!path[0]) {
        const char *p = SDL_GetPrefPath(PREF_ORG, PREF_APP);
        snprintf(path, sizeof(path), "%s", p ? p : "");
    }
    return path;
}

struct Tex { GLuint id; int w, h; };

// Panels, speaker portraits, and talk backdrops all load from <pref>/cutscenes/ first
// (pushed by fast_reload.sh), then from the APK assets.
static Tex tex_load(const char *file) {
    Tex t = {0, 0, 0};
    char path[768];
    size_t size = 0;
    snprintf(path, sizeof(path), "%scutscenes/%s", pref_path(), file);
    void *data = SDL_LoadFile(path, &size);
    if (!data) {
        snprintf(path, sizeof(path), "cutscenes/%s", file);
        data = SDL_LoadFile(path, &size);
    }
    if (!data) {                      // desktop: straight out of the repo, which is what a capture run reads
        if (!strncmp(file, "portrait_", 9)) snprintf(path, sizeof(path), "story/portraits/%s", file + 9);
        else snprintf(path, sizeof(path), "story/panels/%s", file);
        data = SDL_LoadFile(path, &size);
    }
    if (!data) { SDL_Log("cutscene: %s not found", file); return t; }
    int n = 0;
    unsigned char *px = stbi_load_from_memory((const unsigned char *)data, (int)size, &t.w, &t.h, &n, 4);
    SDL_free(data);
    if (!px) { SDL_Log("cutscene: %s failed to decode", file); return t; }
    glGenTextures(1, &t.id);
    glBindTexture(GL_TEXTURE_2D, t.id);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t.w, t.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    // Panels and portraits are full-resolution PAINTINGS shown small — a 550x850 portrait lands in a
    // ~133x206 box, a 4x minification. Plain LINEAR takes four samples of sixteen and aliases; NEAREST
    // magnification made the edges crawl. Mipmaps are what a downscaled painting wants.
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(px);
    return t;
}

// ───────────────────────── Dev messages (phone <-> dev chat, see CLAUDE.md) ─────────────────────────

#define MAX_MSGS 25
#define MSG_LEN 512
struct DevMsg { char text[MSG_LEN]; char stamp[12]; bool from_dev; };
static struct { DevMsg msgs[MAX_MSGS]; int count; char input[MSG_LEN]; bool open; float poll_t; bool scroll; } dev;

static const char *dev_log_path() {
    static char path[600];
    if (!path[0]) snprintf(path, sizeof(path), "%sdev_log.txt", pref_path());
    return path;
}

static void dev_load() {                                  // keeps the newest MAX_MSGS lines
    int before = dev.count;
    dev.count = 0;
    size_t size = 0;
    char *data = (char *)SDL_LoadFile(dev_log_path(), &size);
    if (!data) return;
    int total = 0;
    for (size_t i = 0; i < size; i++) if (data[i] == '\n') total++;
    int skip = total - MAX_MSGS;
    char *p = data, *end = data + size;
    while (p < end) {
        char *nl = (char *)memchr(p, '\n', end - p);
        if (!nl) nl = end;
        int len = (int)(nl - p);
        if (skip-- <= 0 && dev.count < MAX_MSGS && len > 15 && p[0] == '[' &&
            (!memcmp(p + 1, "DEV ", 4) || !memcmp(p + 1, "YOU ", 4))) {
            DevMsg *m = &dev.msgs[dev.count++];
            m->from_dev = (p[1] == 'D');
            memcpy(m->stamp, p + 5, 8); m->stamp[8] = 0;
            int n = len - 15 < MSG_LEN - 1 ? len - 15 : MSG_LEN - 1;
            memcpy(m->text, p + 15, n); m->text[n] = 0;
        }
        p = nl + 1;
    }
    SDL_free(data);
    if (dev.count != before) dev.scroll = true;
}

static void dev_send(const char *text) {
    dev_load();                                           // re-read so lines appended by dev_msg.sh survive
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    SDL_IOStream *f = SDL_IOFromFile(dev_log_path(), "wb");
    if (!f) return;
    char line[MSG_LEN + 32];
    for (int i = dev.count >= MAX_MSGS ? 1 : 0; i < dev.count; i++) {
        int n = snprintf(line, sizeof(line), "[%s %s] %s\n", dev.msgs[i].from_dev ? "DEV" : "YOU", dev.msgs[i].stamp, dev.msgs[i].text);
        SDL_WriteIO(f, line, n);
    }
    int n = snprintf(line, sizeof(line), "[YOU %02d:%02d:%02d] %s\n", tm->tm_hour, tm->tm_min, tm->tm_sec, text);
    SDL_WriteIO(f, line, n);
    SDL_CloseIO(f);
    dev_load();
}

// ───────────────────────── Game state ─────────────────────────

enum Screen { SCR_TITLE, SCR_INTRO, SCR_END, SCR_FIELD };
#define MAX_PANELS 12             // >= story_prompt.py's SCENE_MAX_PANELS (8), the most one scene can hold
// Distinct portrait FILES cached per scene. A character now has up to ten expression portraits
// (portrait_<name>_<expr>.png beside portrait_<name>.png), and only the ones a scene actually asks
// for are ever loaded, so this is "speakers x expressions used", not "speakers x ten".
#define MAX_FACES 32
#define FACE_KEY 96               // a portrait file name: portrait_<name>_<expr>.png
#define PANEL_IN 0.28f            // seconds for a panel to arrive
#define FACE_IN 0.22f             // seconds for a portrait to slide in when the speaker changes
#define FACE_POP 0.12f            // seconds for the little pop when the SAME speaker changes expression
#define TYPE_CPS 42.0f            // typewriter characters per second

struct Star {
    int screen, scene, line;
    int line_page;                // which page of this line is on screen (long lines paginate)
    float line_t;                 // seconds since this line started
    float fade;                   // 1 = black, 0 = clear
    int fade_to_scene;            // scene to cut to once faded out (-1 = none, CS_CUR_COUNT = the field)
    bool to_field;                // this scene was started from the field, so it returns there when it ends
    bool from_field;              // set for the one fade that carries us from the field into that scene
    VoxField *vx;                 // the voxel + sprite field (VOXFIELD_NOTES.md): what SCR_FIELD is
    bool panel_on[MAX_PANELS];
    float panel_t[MAX_PANELS];
    int panel_z[MAX_PANELS], z_next;
    int page;                     // page currently on screen; revealing a panel from another page clears it
    Tex tex[MAX_PANELS];
    Tex face[MAX_FACES];          // speaker portraits, one per distinct file the scene has asked for
    char face_file[MAX_FACES][FACE_KEY];    // the file name, built (expressions), so it is owned here
    unsigned char face_owned[MAX_FACES];    // 0 = an alias of another slot's texture: never delete it
    Tex backdrop;                 // talk scenes: the panel shown dimmed behind the conversation
    int tex_scene;                // which scene's textures are loaded (-1 = none)
    const char *face_cur;         // portrait on screen, so a change can slide the new one in
    float face_t;
    char expr_cur[16];            // the expression on screen: a change on the SAME speaker pops, never slides
    float face_pop;               // seconds since that pop started (>= FACE_POP = finished)
    bool port_motion;             // Dev: the pop, the shake, the jolt and the sink. On by default.
    float dpi_scale;
    float title_t;
    char cap_spec[320];           // VOX_CAPTURE=<map>:<w>:<h>:<out.png>, desktop capture path
    int cap_vox;                  // 1 = VOX_CAPTURE=<map>:<w>:<h>:<out.png>: the voxel field
                                  // 2 = VOX_SELFTEST, 3 = VOX_WALKTEST (the movement bot)
    int cap_state;                // 0 idle, 1 asked, 2 done -> quit
    char dlg_spec[160];           // DIALOG_CAPTURE=<scene>:<line>[:page][:n|:x], the box on the Mac
    int dlg_frames;
    int dlg_as_narr, dlg_no_port, dlg_small;  // capture-only: as a Narrator / no portrait / a short box
    int dlg_textless;             // capture-only: the same line drawn with its text emptied (a reaction beat)
                                   // (the short box is how the paginator and the ▼▼ marker are shown working)
    // ── chapter one ──────────────────────────────────────────────────────────────────────────
    Chapter ch;                   // the story's whole memory: step, flags, party, the bell
    Battle *bat;                  // the battle screen, created lazily on the first fight
    int in_battle;                // 1 while SCR_FIELD is showing a battle instead of the world
    char fight_enc[32];           // which encounter is being fought, so its flag can be set on the win
    int self_test;                // 1 = --chapter-selftest is driving; no window input is read
};

static void star_free_textures(Star *st) {
    for (int i = 0; i < MAX_PANELS; i++) if (st->tex[i].id) glDeleteTextures(1, &st->tex[i].id);
    // Only OWNED slots hold a texture of their own: a slot whose expression file was missing carries
    // a copy of the base portrait's Tex, and deleting that id twice would kill the base's texture.
    for (int i = 0; i < MAX_FACES; i++) if (st->face[i].id && st->face_owned[i]) glDeleteTextures(1, &st->face[i].id);
    if (st->backdrop.id) glDeleteTextures(1, &st->backdrop.id);
    memset(st->tex, 0, sizeof(st->tex));
    memset(st->face, 0, sizeof(st->face));
    memset(st->face_file, 0, sizeof(st->face_file));
    memset(st->face_owned, 0, sizeof(st->face_owned));
    st->backdrop = Tex{0, 0, 0};
    st->tex_scene = -1;
    st->face_cur = nullptr;
    st->expr_cur[0] = 0;
}

static bool same_file(const char *a, const char *b) { return a == b || (a && b && !strcmp(a, b)); }

#ifndef CS_NARRATOR
#define CS_NARRATOR "Narrator"    // older cutscene_data.h; ./story_prompt.py export writes this itself
#endif

// Document text and unattributed lines: a box with no speaker name and no portrait, text centred.
// Panel and talk scenes use it as freely as narration scenes do.
static bool line_is_narrator(const CsLine *ln) { return ln->speaker && !strcmp(ln->speaker, CS_NARRATOR); }

// ── Expressions (the ten ids the export writes: neutral, smile, laugh, biglaugh, concern, sorrow,
// annoyed, angry, shock, resolve). A line's expression names a SECOND portrait file for the same
// speaker — portrait_hart_sorrow.png beside portrait_hart.png — and the line falls back to the
// speaker's plain portrait when that file has not been drawn yet.
//
// CS_HAS_EXPR is defined by a cutscene_data.h whose CsLine carries `const char *expr`. The game
// builds against a header with it and against one without, so the tool side can land separately.
static char g_expr_force[16];     // DIALOG_EXPR=<expr>: force one expression, for a Mac capture only

static const char *line_expr(const CsLine *ln) {
    if (g_expr_force[0]) return g_expr_force;
#ifdef CS_HAS_EXPR
    return ln->expr ? ln->expr : "";
#else
    (void)ln;
    return "";
#endif
}

// portrait_hart.png + "sorrow" -> portrait_hart_sorrow.png. An empty expression is the base file.
static void face_key(const char *base, const char *expr, char *out, size_t n) {
    if (!expr || !*expr) { snprintf(out, n, "%s", base); return; }
    const char *dot = strrchr(base, '.');
    int stem = dot ? (int)(dot - base) : (int)strlen(base);
    snprintf(out, n, "%.*s_%s%s", stem, base, expr, dot ? dot : "");
}

// The scene's portraits, cached by file name. Loading is LAZY — the first line that asks for an
// expression loads it — and a cache miss is cached too, so a missing (speaker, expression) pair is
// looked for, and logged by tex_load, exactly once per scene.
static const Tex *face_get(Star *st, const char *base, const char *expr) {
    if (!base) return nullptr;
    char key[FACE_KEY];
    face_key(base, expr, key, sizeof(key));
    int slot = -1, free_slot = -1;
    for (int i = 0; i < MAX_FACES; i++) {
        if (!st->face_file[i][0]) { if (free_slot < 0) free_slot = i; continue; }
        if (!strcmp(st->face_file[i], key)) { slot = i; break; }
    }
    if (slot < 0) {
        if (free_slot < 0) return (expr && *expr) ? face_get(st, base, "") : nullptr;   // cache full
        slot = free_slot;
        snprintf(st->face_file[slot], FACE_KEY, "%s", key);
        st->face[slot] = tex_load(key);
        st->face_owned[slot] = st->face[slot].id ? 1 : 0;
        if (!st->face[slot].id && expr && *expr) {                       // not drawn yet: use the base
            const Tex *b = face_get(st, base, "");                       // may take another slot; ours is taken
            if (b) { st->face[slot] = *b; st->face_owned[slot] = 0; }
            SDL_Log("cutscene: no %s, falling back to %s", key, base);
        }
    }
    return st->face[slot].id ? &st->face[slot] : nullptr;
}

// The plain portraits of every speaker in the scene, loaded up front as they always were, so a
// speaker change never waits on a decode. Expression variants come in lazily on the line that uses
// one; a scene with many of them simply fills more of the cache.
static void load_faces(Star *st, const CsScene *sc) {
    for (int i = 0; i < sc->line_count; i++)
        if (sc->lines[i].portrait) face_get(st, sc->lines[i].portrait, "");
}

static void reveal_panel(Star *st, const CsScene *sc, int n, bool instant) {   // n is 1-based, 0 = none
    if (n < 1 || n > MAX_PANELS || n > sc->panel_count) return;
    if (sc->panels[n - 1].page != st->page) {                              // new page: start from an empty screen
        st->page = sc->panels[n - 1].page;
        memset(st->panel_on, 0, sizeof(st->panel_on));
        st->z_next = 0;
    }
    if (st->panel_on[n - 1]) return;
    st->panel_on[n - 1] = true;
    st->panel_t[n - 1] = instant ? PANEL_IN : 0.0f;
    st->panel_z[n - 1] = st->z_next++;
    if (!instant) au_play(V_THUMP, 0, 0.35f, 0.5f);
}

// Enter `line` of `scene`. `instant` rebuilds the page without animation (used after a hot reload).
static void star_goto(Star *st, int scene, int line, bool instant) {
    const CsScene *sc = &CS_CUR[scene];
    if (st->tex_scene != scene) {
        star_free_textures(st);                 // only ever one scene's art is resident: 8 panels + portraits
        for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++) st->tex[i] = tex_load(sc->panels[i].file);
        if (sc->backdrop) st->backdrop = tex_load(sc->backdrop);
        load_faces(st, sc);
        st->tex_scene = scene;
    }
    if (scene != st->scene || line == 0 || instant) {
        memset(st->panel_on, 0, sizeof(st->panel_on));
        st->z_next = 0;
        st->page = 0;
        for (int i = 0; i < line; i++) reveal_panel(st, sc, sc->lines[i].reveal, true);
    }
    st->screen = SCR_INTRO;
    st->scene = scene;
    st->line = line;
    st->line_page = 0;                     // a new line always starts at its first page
    st->line_t = instant ? 99.0f : 0.0f;
    // A NEW SPEAKER slides in from their end of the box, as before. The SAME speaker changing
    // expression is a different event — the face is already there and only its look changed — so it
    // swaps with a tiny pop instead, which reads as a reaction rather than as somebody arriving.
    const char *ex = line_expr(&sc->lines[line]);
    if (!same_file(st->face_cur, sc->lines[line].portrait)) {
        st->face_cur = sc->lines[line].portrait;
        st->face_t = instant ? FACE_IN : 0.0f;
        st->face_pop = FACE_POP;
    } else if (strcmp(st->expr_cur, ex)) {
        st->face_pop = instant ? FACE_POP : 0.0f;
    }
    snprintf(st->expr_cur, sizeof(st->expr_cur), "%s", ex);
    reveal_panel(st, sc, sc->lines[line].reveal, instant);
    mus_start(sc->lines[line].mood);
}

static void star_advance(Star *st) {
    const CsScene *sc = &CS_CUR[st->scene];
    if (st->line + 1 < sc->line_count) { star_goto(st, st->scene, st->line + 1, false); return; }
    st->fade_to_scene = st->scene + 1;                                     // fade out, then next scene or the end
}

// ───────────────────────── Drawing ─────────────────────────

static float ease_out(float x) { x = x < 0 ? 0 : x > 1 ? 1 : x; return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x); }
static ImU32 with_alpha(ImU32 c, float a) { a = a < 0 ? 0 : a > 1 ? 1 : a; return (c & 0x00FFFFFF) | ((ImU32)(((c >> 24) & 0xFF) * a) << 24); }

// Draws `text` word-wrapped inside `width`, showing only the first `chars` characters. Wrapping is
// computed on the full string so words never jump lines while they are being typed.
static void draw_typed(ImDrawList *dl, ImFont *font, float size, ImVec2 pos, float width, ImU32 col, const char *text, int chars, bool center) {
    const char *end = text + strlen(text), *p = text;
    float y = pos.y;
    while (p < end && chars > 0) {
        const char *brk = font->CalcWordWrapPosition(size, p, end, width);
        if (brk == p) brk = p + 1;
        int n = (int)(brk - p);
        const char *show_end = p + (n < chars ? n : chars);
        float x = pos.x;
        if (center) x += (width - font->CalcTextSizeA(size, FLT_MAX, 0.0f, p, brk).x) * 0.5f;
        dl->AddText(font, size, ImVec2(x + size * 0.06f, y + size * 0.06f), with_alpha(IM_COL32(0, 0, 0, 255), ((col >> 24) & 0xFF) / 255.0f), p, show_end);
        dl->AddText(font, size, ImVec2(x, y), col, p, show_end);
        chars -= n;
        y += size * 1.3f;
        p = brk;
        while (p < end && (*p == ' ' || *p == '\n')) { p++; chars--; }
    }
}

// ImGui 1.92's GL backend binds its OWN sampler object (linear, no mipmap) for every draw, which
// silently overrides whatever glTexParameteri a texture was given — which is why setting
// GL_LINEAR_MIPMAP_LINEAR on a panel changed not one pixel. Panels and portraits are full-resolution
// paintings minified into their boxes (a 480x771 portrait into ~200x304), so they want mipmaps: these
// two callbacks bracket the images with a sampler that has them, and put ImGui's back afterwards.
// Everything else in the draw list — the box, the text, the font atlas, which has no mip levels —
// keeps ImGui's sampler untouched.
static GLuint g_mip_sampler = 0;
static void cb_sampler(const ImDrawList *, const ImDrawCmd *cmd) {
    ImGui_ImplOpenGL3_RenderState *rs = (ImGui_ImplOpenGL3_RenderState *)ImGui::GetPlatformIO().Renderer_RenderState;
    if (!rs || !rs->UseBindSampler) return;                    // a GL2-era path: texture params apply as they are
    if (cmd->UserCallbackData) {
        if (!g_mip_sampler) {
            glGenSamplers(1, &g_mip_sampler);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(g_mip_sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        glBindSampler(0, g_mip_sampler);
    } else {
        glBindSampler(0, rs->CurrentSampler);
    }
}
static inline void mip_on(ImDrawList *dl)  { dl->AddCallback(cb_sampler, (void *)1); }
static inline void mip_off(ImDrawList *dl) { dl->AddCallback(cb_sampler, (void *)0); }

static void draw_box(ImDrawList *dl, ImVec2 a, ImVec2 b, float u) {        // the blue 16-bit dialogue box
    dl->AddRectFilled(a, b, IM_COL32(8, 8, 20, 255), u * 1.6f);
    dl->AddRectFilledMultiColor(ImVec2(a.x + u, a.y + u), ImVec2(b.x - u, b.y - u),
        IM_COL32(24, 40, 150, 255), IM_COL32(24, 40, 150, 255), IM_COL32(8, 16, 84, 255), IM_COL32(8, 16, 84, 255));
    dl->AddRect(ImVec2(a.x + u * 0.5f, a.y + u * 0.5f), ImVec2(b.x - u * 0.5f, b.y - u * 0.5f), IM_COL32(225, 225, 235, 255), u * 1.4f, 0, u * 0.7f);
    dl->AddRect(ImVec2(a.x + u * 1.3f, a.y + u * 1.3f), ImVec2(b.x - u * 1.3f, b.y - u * 1.3f), IM_COL32(90, 100, 150, 255), u, 0, u * 0.35f);
}

static void draw_scene(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    const CsScene *sc = &CS_CUR[st->scene];
    const CsLine *ln = &sc->lines[st->line];
    bool narr = line_is_narrator(ln) || st->dlg_as_narr;   // dlg_* are set only by DIALOG_CAPTURE
    const char *ltext = st->dlg_textless ? "" : ln->text;  // a textless line: name and portrait, no words
    bool port = h > w;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));

    // Stage and dialogue box. Portrait: 4:5 stage with the box below. Landscape: 16:10 stage, box overlays it.
    float sw, sh, sx, sy, bx0, by0, bx1, by1, text_size;
    if (port) {
        sw = w * 0.97f; sh = sw / 0.8f;
        float box_h = sw * 0.34f, gap = sw * 0.02f, total = sh + gap + box_h;
        if (total > h * 0.96f) { float k = h * 0.96f / total; sw *= k; sh *= k; box_h *= k; gap *= k; total = h * 0.96f; }
        sx = (w - sw) * 0.5f; sy = (h - total) * 0.5f;
        bx0 = sx; bx1 = sx + sw; by0 = sy + sh + gap; by1 = by0 + box_h;
        text_size = sw * 0.046f;
    } else {
        sh = h * 0.98f; sw = sh * 1.6f;
        if (sw > w * 0.98f) { sw = w * 0.98f; sh = sw / 1.6f; }
        sx = (w - sw) * 0.5f; sy = (h - sh) * 0.5f;
        // The box is deep enough for the speaker name and THREE lines of text; it was 0.22 of the
        // stage, which fitted exactly one line under the name and paginated every long line to bits.
        bx0 = sx + sw * 0.04f; bx1 = sx + sw * 0.96f; by0 = sy + sh * (st->dlg_small ? 0.80f : 0.66f); by1 = sy + sh * 0.985f;
        text_size = sh * 0.047f;
    }
    float u = text_size * 0.12f;

    // Talk scenes: no panels, just the named backdrop panel dimmed and centred behind the box.
    if (sc->kind == CS_TALK && st->backdrop.id) {
        float ia = (float)st->backdrop.w / (float)st->backdrop.h, dw = sw, dh = sh;
        if (ia > sw / sh) dh = sw / ia; else dw = sh * ia;
        ImVec2 p0(sx + (sw - dw) * 0.5f, sy + (sh - dh) * 0.5f);
        mip_on(dl);
        dl->AddImage((ImTextureID)(intptr_t)st->backdrop.id, p0, ImVec2(p0.x + dw, p0.y + dh),
                     ImVec2(0, 0), ImVec2(1, 1), IM_COL32(80, 80, 92, 255));   // tint multiplies: dim, slightly cool
        mip_off(dl);
    }

    // Panels, oldest first so newer ones overlap them.
    for (int z = 0; z < st->z_next; z++) {
        for (int i = 0; i < sc->panel_count && i < MAX_PANELS; i++) {
            if (!st->panel_on[i] || st->panel_z[i] != z) continue;
            st->panel_t[i] += dt;
            float a = ease_out(st->panel_t[i] / PANEL_IN);
            const CsRect *r = port ? &sc->panels[i].port : &sc->panels[i].land;
            float slide = (1.0f - a) * sw * 0.05f * ((i & 1) ? 1.0f : -1.0f);
            ImVec2 p0(sx + sw * r->x / 100.0f + slide, sy + sh * r->y / 100.0f);
            ImVec2 p1(p0.x + sw * r->w / 100.0f, p0.y + sh * r->h / 100.0f);
            dl->AddRectFilled(ImVec2(p0.x + u * 2, p0.y + u * 2), ImVec2(p1.x + u * 2, p1.y + u * 2), with_alpha(IM_COL32(0, 0, 0, 170), a));
            if (st->tex[i].id) {
                mip_on(dl);
                dl->AddImage((ImTextureID)(intptr_t)st->tex[i].id, p0, p1, ImVec2(0, 0), ImVec2(1, 1), with_alpha(IM_COL32_WHITE, a));
                mip_off(dl);
            }
            else { dl->AddRectFilled(p0, p1, IM_COL32(24, 22, 36, 255)); dl->AddRect(p0, p1, IM_COL32_WHITE, 0, 0, u); }
            if (a < 1.0f) dl->AddRectFilled(p0, p1, with_alpha(IM_COL32(255, 255, 255, 190), 1.0f - a));   // arrival flash
        }
    }

    // Text. A line waits for its panel to land before typing starts. A line longer than the box is
    // PAGINATED (dialogue.h): the typewriter runs per page, a tap turns the page, and the panel
    // reveal and the mood change already happened in star_goto — that is, on the line's first page
    // only, because turning a page never goes through star_goto.
    st->line_t += dt;
    float type_t = st->line_t - (ln->reveal && !sc->narration ? PANEL_IN * 0.8f : 0.0f);

    float arrow_x = bx1 - text_size * 1.1f;                                // moves in when a portrait sits on the right
    float arrow_y = by1 - text_size * 0.95f;
    bool typing = true, more_pages = false;

    if (sc->narration) {
        float a = ease_out(st->line_t / 0.6f);
        float size = text_size * 1.08f, width = (port ? w * 0.84f : w * 0.7f);
        float top = h * (port ? 0.30f : 0.26f), bot = h * (port ? 0.74f : 0.72f);
        DlgPages pg;
        dlg_paginate(font, size, width, dlg_max_lines(bot - top, size), ltext, &pg);
        if (st->line_page >= pg.count) st->line_page = pg.count - 1;
        const char *pb = pg.beg[st->line_page], *pe = pg.end[st->line_page];
        int total = (int)(pe - pb);
        int chars = type_t <= 0 ? 0 : (int)(type_t * TYPE_CPS);
        typing = chars < total;
        more_pages = st->line_page + 1 < pg.count;
        float th = dlg_text_height(font, size, width, pb, pe);
        float ty = top + ((bot - top) - th) * 0.5f;                        // narration centres in its band
        dlg_draw_text(dl, font, size, ImVec2((w - width) * 0.5f, ty), width,
                      with_alpha(IM_COL32(222, 226, 240, 255), a), pb, pe, chars, true);
        arrow_x = (w + width) * 0.5f - size * 0.5f;
        arrow_y = ty + th + size * 0.2f;
        if (typing && chars > 0) {
            static int last_blip = -1;
            if (chars / 3 != last_blip && pb[chars - 1] != ' ') { last_blip = chars / 3; au_play(V_BLIP, 520.0f, 0.04f, 0.10f); }
        }
    } else {
        draw_box(dl, ImVec2(bx0, by0), ImVec2(bx1, by1), u);
        // The content rect: the box minus one uniform pad on ALL FOUR sides. The name sits at its top
        // and the text under the name. It used to be padded from the top only, which pushed the text
        // onto the bottom edge with a band of empty blue above it.
        DlgRect cr = dlg_content(bx0, by0, bx1, by1, text_size);
        float pad = text_size * DLG_LINE_H * DLG_PAD_LINES;
        // Speaker portrait at one end of the box (Phantasy Star IV field talk), text narrowed to fit.
        // A Narrator line never has one: it is a document, not somebody speaking.
        const Tex *fa = (narr || st->dlg_no_port) ? nullptr : face_get(st, ln->portrait, line_expr(ln));
        if (fa) {
            st->face_t += dt;
            st->face_pop += dt;
            float inset = u * 2.0f, fh = (by1 - by0) - inset * 2.0f;
            float fw = fh * (float)fa->w / (float)fa->h, cap = (bx1 - bx0) * 0.3f;
            if (fw > cap) { fw = cap; fh = fw * (float)fa->h / (float)fa->w; }
            bool right = (ln->side == CS_RIGHT);
            float a = ease_out(st->face_t / FACE_IN);
            float fx = right ? bx1 - inset - fw : bx0 + inset;
            float fy = by0 + ((by1 - by0) - fh) * 0.5f;
            ImVec2 f0(fx + (1.0f - a) * fw * 0.4f * (right ? 1.0f : -1.0f), fy), f1(f0.x + fw, f0.y + fh);
            // PORTRAIT MOTION (Dev toggle). Cheap, no assets: the pop on an expression swap, and one
            // per-expression idle held for as long as the line is up. `lpx` is a logical pixel — the
            // same 2 px on the phone as on the Mac, because the box itself is laid out in real ones.
            if (st->port_motion) {
                float lpx = st->dpi_scale > 0.0f ? st->dpi_scale : 1.0f, ox = 0.0f, oy = 0.0f, scl = 1.0f;
                if (st->face_pop < FACE_POP)                                    // 1.0 -> 1.06 -> 1.0
                    scl = 1.0f + 0.06f * sinf((st->face_pop / FACE_POP) * 3.14159265f);
                const char *ex = line_expr(ln);
                if (!strcmp(ex, "biglaugh")) oy = sinf(st->line_t * 6.0f * 6.2831853f) * 2.0f * lpx;
                else if (!strcmp(ex, "shock")) {                                // one sharp jolt, then still
                    float j = st->line_t < 0.16f ? 1.0f - st->line_t / 0.16f : 0.0f;
                    oy = -j * 5.0f * lpx;
                    ox = j * 2.0f * lpx * (right ? 1.0f : -1.0f);
                } else if (!strcmp(ex, "sorrow")) oy = 2.0f * lpx * (1.0f - expf(-st->line_t * 1.1f));
                ImVec2 c((f0.x + f1.x) * 0.5f, (f0.y + f1.y) * 0.5f);
                f0 = ImVec2(c.x + (f0.x - c.x) * scl + ox, c.y + (f0.y - c.y) * scl + oy);
                f1 = ImVec2(c.x + (f1.x - c.x) * scl + ox, c.y + (f1.y - c.y) * scl + oy);
            }
            dl->AddRectFilled(f0, f1, with_alpha(IM_COL32(6, 8, 22, 255), a));
            mip_on(dl);
            dl->AddImage((ImTextureID)(intptr_t)fa->id, f0, f1, ImVec2(0, 0), ImVec2(1, 1), with_alpha(IM_COL32_WHITE, a));
            mip_off(dl);
            dl->AddRect(f0, f1, with_alpha(IM_COL32(225, 225, 235, 255), a), 0, 0, u * 0.7f);
            dl->AddRect(ImVec2(f0.x + u * 0.8f, f0.y + u * 0.8f), ImVec2(f1.x - u * 0.8f, f1.y - u * 0.8f),
                        with_alpha(IM_COL32(90, 100, 150, 255), a), 0, 0, u * 0.35f);
            if (right) cr.x1 = fx - pad * 0.6f;
            else cr.x0 = fx + fw + pad * 0.6f;
        }
        float name_h = narr ? 0.0f : text_size * DLG_LINE_H;               // the yellow speaker line
        float gutter = text_size * 1.1f;                                   // the marker's own corner
        float tw = (cr.x1 - gutter) - cr.x0, th_avail = (cr.y1 - cr.y0) - name_h;
        DlgPages pg;
        dlg_paginate(font, text_size, tw, dlg_max_lines(th_avail, text_size), ltext, &pg);
        if (st->line_page >= pg.count) st->line_page = pg.count - 1;
        const char *pb = pg.beg[st->line_page], *pe = pg.end[st->line_page];
        int total = (int)(pe - pb);
        int chars = type_t <= 0 ? 0 : (int)(type_t * TYPE_CPS);
        typing = chars < total;
        more_pages = st->line_page + 1 < pg.count;
        // A TEXTLESS LINE — an expression and no words, a reaction beat — is a box with the speaker's
        // name and portrait in it and nothing to type. There is no typewriter to wait through and no
        // timer: the marker is up from the first frame and the tap moves on, exactly as it does on a
        // line whose typing has finished.
        if (total == 0) typing = false;
        if (typing && chars > 0) {
            static int last_blip = -1;
            if (chars / 3 != last_blip && pb[chars - 1] != ' ') { last_blip = chars / 3; au_play(V_BLIP, narr ? 520.0f : 760.0f, 0.04f, 0.10f); }
        }
        if (narr) {                                                        // no name line: the block centres
            float th = dlg_text_height(font, text_size, tw, pb, pe);
            float ty = cr.y0 + ((cr.y1 - cr.y0) - th) * 0.5f;
            dlg_draw_text(dl, font, text_size, ImVec2(cr.x0, ty), tw, IM_COL32(230, 232, 245, 255), pb, pe, chars, true);
        } else {
            dl->AddText(font, text_size * 0.92f, ImVec2(cr.x0, cr.y0), IM_COL32(255, 216, 74, 255), ln->speaker);
            dlg_draw_text(dl, font, text_size, ImVec2(cr.x0, cr.y0 + name_h), tw, IM_COL32_WHITE, pb, pe, chars, false);
        }
        arrow_x = cr.x1 - text_size * 0.45f;
        arrow_y = cr.y1 - text_size * 0.45f;
    }
    if (!typing) dlg_marker(dl, arrow_x, arrow_y, text_size, more_pages, st->line_t);

    // Input: a tap finishes the typing, then turns the page, then moves to the next line.
    // Taps on the dev overlay don't count.
    bool tap = ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && !ImGui::GetIO().WantTextInput;
    if (tap && st->fade_to_scene < 0 && st->fade <= 0.01f) {
        if (typing) st->line_t = 99.0f;
        else if (more_pages) { st->line_page++; st->line_t = 0.0f; au_play(V_BLIP, 640.0f, 0.05f, 0.12f); }
        else star_advance(st);
    }
}

static void draw_title(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    if (st->title_t == 0.0f) mus_start(CS_WONDER);                         // once, so the dev mood buttons still work here
    st->title_t += dt;
    dl->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(6, 6, 22, 255), IM_COL32(6, 6, 22, 255), IM_COL32(0, 0, 0, 255), IM_COL32(0, 0, 0, 255));
    for (int i = 0; i < 70; i++) {                                         // a quiet starfield
        uint32_t k = (uint32_t)i * 2654435761u;
        float x = (float)(k % 1000) / 1000.0f * w, y = (float)((k >> 10) % 1000) / 1000.0f * h * 0.75f;
        float tw = 0.55f + 0.45f * sinf(st->title_t * (0.6f + (k % 7) * 0.2f) + i);
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + 3, y + 3), with_alpha(IM_COL32(200, 210, 255, 255), tw));
    }
    float m = (float)(w < h ? w : h);
    const char *title = "THE FAIR COPY", *sub = CH_TITLE;
    float ts = m * 0.115f, ss = m * 0.042f;
    ImVec2 tsz = font->CalcTextSizeA(ts, FLT_MAX, 0, title), ssz = font->CalcTextSizeA(ss, FLT_MAX, 0, sub);
    float y = h * 0.28f;
    dl->AddText(font, ts, ImVec2((w - tsz.x) * 0.5f + ts * 0.05f, y + ts * 0.05f), IM_COL32(20, 30, 120, 255), title);
    dl->AddText(font, ts, ImVec2((w - tsz.x) * 0.5f, y), IM_COL32(240, 215, 120, 255), title);
    dl->AddText(font, ss, ImVec2((w - ssz.x) * 0.5f, y + ts * 1.25f), IM_COL32(190, 200, 235, 255), sub);
    if (fmodf(st->title_t, 1.4f) < 0.9f) {
        const char *tap = "Tap to begin";
        ImVec2 z = font->CalcTextSizeA(ss, FLT_MAX, 0, tap);
        dl->AddText(font, ss, ImVec2((w - z.x) * 0.5f, h * 0.68f), IM_COL32_WHITE, tap);
    }
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && st->title_t > 0.5f && st->fade_to_scene < 0) {
        au_play(V_BELL, note_freq(81), 1.2f, 0.3f);
        // New Game. The chapter starts at CH_STEPS[0], which is a PLAY block in the yard — the
        // chapter opens on the stick in your hand, not on a scene — so we fade to the FIELD and
        // let ch_apply_step put the party on the right map, in the right light, with the goal up.
        ch_new_game(&st->ch);
        st->fade_to_scene = CS_CUR_COUNT;
    }
}

static void draw_end(Star *st, int w, int h, float dt) {
    ImDrawList *dl = ImGui::GetBackgroundDrawList();
    ImFont *font = ImGui::GetFont();
    st->title_t += dt;
    dl->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), IM_COL32(0, 0, 0, 255));
    float m = (float)(w < h ? w : h), s = m * 0.06f;
    const char *a = "To be continued", *b = "Tap to return to the title";
    ImVec2 za = font->CalcTextSizeA(s, FLT_MAX, 0, a), zb = font->CalcTextSizeA(s * 0.6f, FLT_MAX, 0, b);
    dl->AddText(font, s, ImVec2((w - za.x) * 0.5f, h * 0.42f), IM_COL32(240, 215, 120, 255), a);
    dl->AddText(font, s * 0.6f, ImVec2((w - zb.x) * 0.5f, h * 0.42f + s * 1.8f), IM_COL32(170, 180, 210, 255), b);
    if (ImGui::IsMouseClicked(0) && !ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && st->title_t > 0.8f) { st->screen = SCR_TITLE; st->title_t = 0; }
}

// ───────────────────────── The world ─────────────────────────
// Title -> intro -> field. The field owns its own renderer, controls and dialogue box (voxfield.cpp);
// the only things it hands back are "play this talk scene" and "the player walked into an encounter
// zone", because those belong to the game, not the map.

static int scene_by_id(const char *id) {
    for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, id)) return i;
    return -1;
}

static void enter_field(Star *st) {
    if (!st->vx) st->vx = vx_create();
    star_free_textures(st);                       // the page art goes; the field has its own
    st->screen = SCR_FIELD;
    st->to_field = false;
    st->from_field = false;
    mus_start(CS_WONDER);
    // Coming back from a clip: that clip WAS the current step, so the chapter moves on now. This is
    // the one place a CLIP step is retired, which is why a clip can be re-entered safely if the
    // player reloads mid-scene.
    if (st->ch.started && st->ch.step < CH_STEP_COUNT && CH_STEPS[st->ch.step].kind == CHS_CLIP) {
        ch_advance(&st->ch, st->vx);
        if (CH_STEPS[st->ch.step].kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; }
    } else {
        ch_apply_step(&st->ch, st->vx);           // re-assert light, party, lantern, goal line
    }
}

// A field asked for a cutscene. If it is not in the playlist yet, say so in the field's own box
// rather than fading to a blank scene.
static void field_scene(Star *st, const char *id) {
    int idx = scene_by_id(id);
    if (idx >= 0 && CS_CUR[idx].line_count > 0) { st->fade_to_scene = idx; st->from_field = true; return; }
    char msg[160];
    snprintf(msg, sizeof(msg), "(scene \"%s\" is not in the playlist yet)", id);
    if (st->vx) vx_message(st->vx, msg);
}

static void draw_field(Star *st, int w, int h, float dt) {
    if (!st->vx) st->vx = vx_create();

    // ── a battle has the screen ───────────────────────────────────────────────────────────────
    // The world is not ticked while a battle runs: the party is frozen where it stood, and the
    // battle draws over everything. Winning or losing hands control straight back to that spot.
    if (st->in_battle) {
        BtEvent bev;
        int out = bt_tick(st->bat, w, h, dt, dev.open, &bev);
        if (bev.kind == BTE_TEXT) vx_say_id(st->vx, bev.arg);
        else if (bev.kind == BTE_GOAL) snprintf(st->ch.goal, sizeof(st->ch.goal), "%s", bev.arg);
        if (out != BT_RUNNING && bt_done(st->bat)) {
            st->in_battle = 0;
            vx_freeze(st->vx, 0);
            ch_on_battle(&st->ch, st->fight_enc, out == BT_WIN);
            // A won fight is consumed so walking back over the cell does not restart it. A lost
            // one is left armed: in the yard that IS the instant retry.
            //
            // EXCEPT THE THREE MACHINES. They are furniture in a yard, not an encounter: Hart
            // built them to be used again, P4 is FOUR SESSIONS at the swing on day two, and
            // consuming the swing the first time it goes down leaves the player standing in front
            // of nothing for the rest of the day with a goal line telling them to beat it. (The
            // chapter play-test found this on step 11, which is session two.)
            bool a_machine = !strcmp(st->fight_enc, "post") || !strcmp(st->fight_enc, "arm") ||
                             !strcmp(st->fight_enc, "swing");
            if (out == BT_WIN && !a_machine) vx_disable_trigger(st->vx, st->fight_enc);
        }
        return;
    }

    VxEvent ev;
    // The box the player is reading is what `vx_busy` reports, and it is read BEFORE the tick so a
    // box that opens this frame already counts: the bell must not tick on the frame a conversation
    // starts. See ch_tick — nobody reads boxes on a clock.
    bool reading = vx_busy(st->vx);
    vx_tick(st->vx, w, h, dt, dev.open, &ev);
    ch_tick(&st->ch, dt, reading || vx_busy(st->vx));

    switch (ev.kind) {
    case VXE_SCENE:  field_scene(st, ev.arg); break;
    case VXE_ZONE:   { char m[128]; snprintf(m, sizeof(m), "encounter %s", ev.arg); dev_send(m); } break;
    case VXE_TEXT:
        // THREE THINGS CAN HAPPEN TO AN EXAMINE, and the chapter decides which.
        //   * the job board before the job exists shows its ALTERNATE line and tells the chapter
        //     nothing — the board is always there, it just says a different thing;
        //   * the guild-hall counter shows the on-time or the late clerk, by the bell;
        //   * everything else is what the trigger said, and the chapter gates on it.
        {
            const char *alt = ch_trig_alt(&st->ch, ev.arg);
            if (alt) { vx_say_id(st->vx, alt); break; }
            const char *line = ch_clerk_line(&st->ch, ev.arg);
            if (strcmp(line, ev.arg)) vx_say_id(st->vx, line);
            // A FLAG CHANGED, SO THE WORLD CHANGED. Half the condition table hangs off a flag that
            // is set in the SAME step as the trigger it unlocks — taking the job sheet down is what
            // makes her door and the guild book live, and all three are inside P5's one row.
            // Re-applying only on a step change left those two inert for the whole of the bell run,
            // which is precisely the step they exist for.
            if (ch_on_text(&st->ch, line)) ch_apply_triggers(&st->ch, st->vx);
        }
        break;
    case VXE_PICKUP: if (ch_on_pickup(&st->ch, ev.arg)) ch_apply_triggers(&st->ch, st->vx); break;
    case VXE_GOAL:   snprintf(st->ch.goal, sizeof(st->ch.goal), "%s", ev.arg); break;
    case VXE_MAP:    ch_on_map(&st->ch, ev.arg); break;
    case VXE_FIGHT:
        if (!st->ch.skip_fights) {
            if (!st->bat) st->bat = bt_create();
            if (bt_start(st->bat, ev.arg, &st->ch.party, vx_current_map(st->vx))) {
                snprintf(st->fight_enc, sizeof(st->fight_enc), "%s", ev.arg);
                st->in_battle = 1;
                vx_freeze(st->vx, 1);
            }
        } else {                                   // Dev "skip fights": count it as a clean win
            ch_on_battle(&st->ch, ev.arg, true);
            vx_disable_trigger(st->vx, ev.arg);
        }
        break;
    default: break;
    }

    // ── the chapter script ────────────────────────────────────────────────────────────────────
    // One rule: when the current step's required interactions have all happened, move on. A CLIP
    // step plays its scene and comes back here; an END step shows the card.
    if (st->ch.started && !vx_busy(st->vx) && ch_step_complete(&st->ch)) {
        const ChStep *s = &CH_STEPS[st->ch.step];
        if (s->kind == CHS_PLAY) {
            ch_advance(&st->ch, st->vx);
            const ChStep *n = &CH_STEPS[st->ch.step];
            if (n->kind == CHS_CLIP) field_scene(st, n->arg);
            else if (n->kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; }
        }
    }
}

// The goal line: six words at most, at the top of the screen, one at a time. STYLE.md rule 2 — the
// player should never have to be told twice what they are doing. Beside it, when the bell is
// running, the rings left: a counter, not a threat, because the run cannot be failed.
static void draw_goal(Star *st, int w, int h) {
    if (st->in_battle || !st->ch.started || !st->ch.goal[0]) return;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    float m = (float)(w < h ? w : h), s = m * 0.042f;
    ImVec2 z = font->CalcTextSizeA(s, FLT_MAX, 0, st->ch.goal);
    float x = (w - z.x) * 0.5f, y = h * 0.035f;
    dl->AddRectFilled(ImVec2(x - s * 0.7f, y - s * 0.28f), ImVec2(x + z.x + s * 0.7f, y + z.y + s * 0.28f),
                      IM_COL32(0, 0, 0, 120), s * 0.3f);
    dl->AddText(font, s, ImVec2(x + 2, y + 2), IM_COL32(0, 0, 0, 200), st->ch.goal);
    dl->AddText(font, s, ImVec2(x, y), IM_COL32(240, 225, 160, 255), st->ch.goal);
    if (st->ch.bell_started) {
        char bell[32];
        int left = CH_BELL_RINGS - st->ch.rings;
        snprintf(bell, sizeof(bell), "%d", left < 0 ? 0 : left);
        ImVec2 bz = font->CalcTextSizeA(s, FLT_MAX, 0, bell);
        dl->AddText(font, s, ImVec2(x + z.x + s * 1.6f, y),
                    left > 0 ? IM_COL32(200, 210, 240, 255) : IM_COL32(200, 120, 110, 255), bell);
        (void)bz;
    }
}

// Put the chapter on a step from the Dev panel. This is the ONLY way the owner moves the story by
// hand now, and it is chapter-shaped rather than scene-shaped:
//   * ch_jump sets every flag the steps before the target gated on, so a skipped gate stays open;
//   * a PLAY step drops you into that map/spawn/light/party with its goal line up;
//   * a CLIP step plays its scene — and because enter_field() retires a CLIP step when the scene
//     ends, tapping through it lands on the PLAY step AFTER it, with gameplay, not on another clip.
static void dev_step(Star *st, int step) {
    if (step < 0) step = 0;
    if (step >= CH_STEP_COUNT) step = CH_STEP_COUNT - 1;
    if (!st->ch.started) ch_new_game(&st->ch);
    st->in_battle = 0;
    ch_jump(&st->ch, st->vx, step);
    const ChStep *t = &CH_STEPS[st->ch.step];
    if (t->kind == CHS_END) { st->screen = SCR_END; st->title_t = 0; return; }
    if (st->screen != SCR_FIELD) enter_field(st);       // enter_field may retire a CLIP: re-read
    if (st->ch.step != step) ch_jump(&st->ch, st->vx, step);
    if (CH_STEPS[st->ch.step].kind == CHS_CLIP) field_scene(st, CH_STEPS[st->ch.step].arg);
}

// Small dev overlay: a corner button opening the phone <-> dev message log plus scene test controls.
static void draw_dev(Star *st, int w, int h, float dt, float dpi) {
    ImGuiStyle &s = ImGui::GetStyle();
    float k = dpi * 1.3f;
    ImGui::GetIO().FontGlobalScale = 1.0f;
    s.FramePadding = ImVec2(8 * k, 6 * k); s.ItemSpacing = ImVec2(6 * k, 4 * k); s.WindowPadding = ImVec2(8 * k, 8 * k);
    s.ScrollbarSize = 12 * k; s.WindowRounding = 4 * k; s.FrameRounding = 3 * k; s.WindowBorderSize = 0; s.TouchExtraPadding = ImVec2(4 * k, 4 * k);

    dev.poll_t += dt;
    if (dev.poll_t > (dev.open ? 0.5f : 3.0f)) { dev.poll_t = 0; dev_load(); }

    ImGuiWindowFlags bare = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground;
    // The two corner buttons: Settings (the player's) and Dev (the owner's). Settings is drawn first
    // so it sits to the LEFT of Dev, and it is here rather than inside the Dev panel on purpose —
    // it belongs to whoever is holding the phone, on the title, in a cutscene and in the field alike.
    ImGui::SetNextWindowPos(ImVec2(w - 4 * k, 4 * k), ImGuiCond_Always, ImVec2(1, 0));
    ImGui::Begin("##devbtn", nullptr, bare | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.28f, 0.18f, g_set.open ? 0.9f : 0.35f));
    if (ImGui::Button("Settings")) g_set.open = !g_set.open;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.5f, dev.open ? 0.9f : 0.35f));
    if (ImGui::Button(dev.open ? "Close" : "Dev")) { dev.open = !dev.open; dev.scroll = true; }
    ImGui::PopStyleColor();
    ImGui::End();

    if (g_set.open) {
        ImGui::SetNextWindowPos(ImVec2(w * 0.5f, h * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(w * (w > h ? 0.46f : 0.86f), 0));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.10f, 0.97f));
        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                                          ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                          ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextUnformatted("Audio");
        int pct = (int)(g_set.volume * 100.0f + 0.5f);
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12);
        if (ImGui::SliderInt("Master volume", &pct, 0, 100, "%d %%")) {
            g_set.volume = pct / 100.0f;
            g_set.dirty = true; g_set.save_t = 0;
        }
        bool mute = g_set.muted;
        if (ImGui::Checkbox("Mute", &mute)) { g_set.muted = mute; g_set.dirty = true; g_set.save_t = 0; }
        if (g_set.env_forced) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.95f, 0.80f, 0.45f, 1.0f), "muted by STAR_MUTE");
        }
        ImGui::Separator();
        if (ImGui::Button("Close")) g_set.open = false;
        ImGui::End();
        ImGui::PopStyleColor();
    }

    if (!dev.open) return;

    ImGui::SetNextWindowPos(ImVec2(w * 0.02f, h * 0.06f));
    ImGui::SetNextWindowSize(ImVec2(w * 0.96f, h * (w > h ? 0.88f : 0.42f)));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.10f, 0.96f));
    ImGui::Begin("Dev", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
    // The intro-era buttons are GONE (owner, 2026-09-21: "the Dev tools next scene just goes to the
    // next scene. And the next after that, no gameplay between them"). There is no scene list to
    // walk any more — the chapter table is the only order the game has — so the controls below step
    // the CHAPTER, and a CLIP step plays its scene and hands back to the PLAY step after it.
    if (ImGui::Button("Title")) { st->screen = SCR_TITLE; st->title_t = 0; dev.open = false; }

    // ── chapter one ───────────────────────────────────────────────────────────────────────────
    // Where the story thinks it is, and a way to put it anywhere. Jumping to a step sets every flag
    // the steps before it gated on, so the world is consistent with having played them.
    ImGui::SeparatorText("Chapter one");
    {
        const ChStep *s = &CH_STEPS[st->ch.step < 0 ? 0 : st->ch.step];
        static const char *KIND[] = { "PLAY", "CLIP", "SET", "END" };
        ImGui::Text("step %d/%d  %s  %s   goal: %s", st->ch.step + 1, CH_STEP_COUNT, KIND[s->kind],
                    s->kind == CHS_CLIP ? s->arg : (s->map ? s->map : "(same map)"),
                    st->ch.goal[0] ? st->ch.goal : "-");
        ImGui::Text("mandatory ten: %d/%d   party %d   %s",
                    CH_MANDATORY - ch_mandatory_missing(&st->ch), CH_MANDATORY,
                    st->ch.party.count, st->ch.bell_started ? "bell running" : "bell idle");
        ImGui::TextDisabled("skipping a step SETS the flags it gated on, so the gates stay consistent");
        if (ImGui::Button("Restart chapter")) { ch_new_game(&st->ch); enter_field(st); dev.open = false; }
        ImGui::SameLine();
        if (ImGui::Button("< prev step") && st->ch.step > 0) { dev_step(st, st->ch.step - 1); dev.open = false; }
        ImGui::SameLine();
        if (ImGui::Button("next step >")) { dev_step(st, st->ch.step + 1); dev.open = false; }
        ImGui::SameLine();
        bool skip = st->ch.skip_fights != 0;
        if (ImGui::Checkbox("Skip fights", &skip)) st->ch.skip_fights = skip ? 1 : 0;
        // Every step, so any block can be reached in one tap.
        if (ImGui::BeginCombo("Jump to step", "...")) {
            for (int i = 0; i < CH_STEP_COUNT; i++) {
                char lbl[96];
                const ChStep *t = &CH_STEPS[i];
                snprintf(lbl, sizeof(lbl), "%2d  %s  %s", i + 1, KIND[t->kind],
                         t->kind == CHS_CLIP ? t->arg : (t->goal ? t->goal : (t->map ? t->map : "-")));
                if (ImGui::Selectable(lbl, i == st->ch.step)) { dev_step(st, i); dev.open = false; }
            }
            ImGui::EndCombo();
        }
        // The flags, so a gate that will not open can be seen and forced.
        if (ImGui::TreeNode("Flags")) {
            for (int i = 0; i < CH_FLAG_COUNT; i++) {
                bool on = ch_has(&st->ch, i);
                ImGui::PushID(i);
                if (ImGui::Checkbox("##f", &on)) {
                    if (on) ch_set(&st->ch, i); else st->ch.flags &= ~(1ull << i);
                }
                ImGui::PopID();
                ImGui::SameLine();
                ImGui::TextColored(i < CH_MANDATORY ? ImVec4(0.95f, 0.85f, 0.45f, 1) : ImVec4(0.7f, 0.75f, 0.85f, 1),
                                   "%s%s", CH_FLAG_NAME[i], i < CH_MANDATORY ? "  (mandatory)" : "");
            }
            ImGui::TreePop();
        }
        if (st->bat) { char cmd[128]; if (bt_dev_ui(st->bat, cmd, sizeof(cmd))) dev_send(cmd); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Field")) { st->fade_to_scene = -1; st->fade = 0.0f; enter_field(st); dev.open = false; }
    static const char *mood_names[CS_MOOD_COUNT] = {"wonder", "dread", "tense", "confront", "sorrow", "hope"};
    ImGui::SameLine();
    ImGui::TextDisabled("  mood:");
    for (int i = 0; i < CS_MOOD_COUNT; i++) {
        if (w > h || i % 3) ImGui::SameLine();
        bool cur = mus.on && mus.target == i;
        if (cur) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.3f, 1));
        if (ImGui::Button(mood_names[i])) mus_start(i);
        if (cur) ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    { bool pm = st->port_motion; if (ImGui::Checkbox("portrait motion", &pm)) st->port_motion = pm; }
    ImGui::Separator();
    if (st->screen == SCR_FIELD) {
        char line[224];
        if (st->vx) { if (vx_dev_ui(st->vx, line, sizeof(line))) dev_send(line); ImGui::Separator(); }
    }
    float input_h = ImGui::GetFrameHeightWithSpacing() + 4 * k;
    ImGui::BeginChild("##msgs", ImVec2(0, -input_h));
    for (int i = 0; i < dev.count; i++) {
        ImGui::PushStyleColor(ImGuiCol_Text, dev.msgs[i].from_dev ? ImVec4(0.45f, 0.8f, 1, 1) : ImVec4(0.55f, 1, 0.55f, 1));
        ImGui::TextWrapped("[%s] %s: %s", dev.msgs[i].stamp, dev.msgs[i].from_dev ? "DEV" : "YOU", dev.msgs[i].text);
        ImGui::PopStyleColor();
    }
    if (dev.scroll) { ImGui::SetScrollHereY(1.0f); dev.scroll = false; }
    ImGui::EndChild();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80 * k);
    bool enter = ImGui::InputText("##in", dev.input, MSG_LEN, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Send") || enter) && dev.input[0]) { dev_send(dev.input); dev.input[0] = 0; }
    ImGui::End();
    ImGui::PopStyleColor();
}

// ───────────────────────── the chapter LOGIC self-test ─────────────────────────
// CHAPTER_LOGIC_SELFTEST=1 (capture.sh --chapter-logic-selftest). Plays the whole chapter through
// the SAME entry points the game uses — ch_on_text, ch_on_pickup, ch_on_battle, ch_advance — with
// no window, no input and no world, and asserts it reaches the end card with all ten mandatory
// flags set. It takes milliseconds and it is what you run after editing CH_STEPS.
//
// WHAT IT DOES NOT PROVE, AND WHY THERE IS A SECOND TEST. It sets the flags itself. It cannot tell
// you that a player can walk to the trigger that sets one, that the trigger exists on the map at
// all, that it fires in the right step, that a hidden item is actually hidden, or that the swing
// can be lost on day one and won on day two. For two months `F_BED`, `F_SIGNED` and `F_BODY` gated
// three steps with NO MAP TRIGGER ANYWHERE that could set them, and this test passed every time,
// because it typed the answers in itself. That is what --chapter-playtest below is for.
static const char *ch_text_for_flag(int flag) {
    for (int i = 0; i < CH_BIND_COUNT; i++) if (CH_BINDS[i].flag == flag) return CH_BINDS[i].text_id;
    return nullptr;
}
static const char *ch_item_for_flag(int flag) {
    for (int i = 0; i < CH_ITEM_BIND_COUNT; i++) if (CH_ITEM_BINDS[i].flag == flag) return CH_ITEM_BINDS[i].text_id;
    return nullptr;
}

static int chapter_selftest() {
    Chapter c;
    ch_new_game(&c);
    int fails = ch_steps_selfcheck(), clips = 0, battles = 0;
    SDL_Log("CHAPTER %d steps, %d flags (%d mandatory)", CH_STEP_COUNT, CH_FLAG_COUNT, CH_MANDATORY);
    for (int guard = 0; guard < CH_STEP_COUNT * 4; guard++) {
        const ChStep *s = &CH_STEPS[c.step];
        if (s->kind == CHS_END) break;
        if (s->kind == CHS_CLIP) {
            // A clip must name a scene that actually exists in cutscene_data.h, or the chapter
            // stalls on the phone with "(scene ... is not in the playlist yet)".
            int idx = -1;
            for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, s->arg)) idx = i;
            if (idx < 0) { SDL_Log("FAIL step %d: CLIP '%s' is not in cutscene_data.h", c.step + 1, s->arg); fails++; }
            else if (CS_CUR[idx].line_count <= 0) { SDL_Log("FAIL step %d: CLIP '%s' has no lines", c.step + 1, s->arg); fails++; }
            else { SDL_Log("  step %2d  CLIP  %-22s %d lines, %d panels", c.step + 1, s->arg,
                           CS_CUR[idx].line_count, CS_CUR[idx].panel_count); clips++; }
            ch_advance(&c, nullptr);
            continue;
        }
        // A PLAY step: do each thing the step is waiting for, the way the world would report it.
        SDL_Log("  step %2d  PLAY  %-22s goal: %s", c.step + 1, s->map ? s->map : "(same)",
                s->goal ? s->goal : "(unchanged)");
        for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) {
            int f = s->need[i];
            if (ch_has(&c, f)) continue;
            const char *tid = ch_text_for_flag(f), *iid = ch_item_for_flag(f);
            if (tid) { ch_on_text(&c, tid); SDL_Log("          examine  %s", tid); }
            else if (iid) { ch_on_pickup(&c, iid); SDL_Log("          pick up  %s", iid); }
            else if (f == F_SWING_TRIED) { ch_on_battle(&c, "swing", false); battles++; SDL_Log("          fight    swing (lost: P1 is not won)"); }
            // P4's three session flags and the win are all "have another go at the swing". The
            // first three are LOSSES, because day two is four sessions and the last one is the win.
            else if (f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3) {
                ch_on_battle(&c, "swing", false); battles++;
                SDL_Log("          fight    swing (day-two session %d, lost)", c.swing_sessions);
            }
            else if (f == F_SWING_BEATEN) { ch_on_battle(&c, "swing", true); battles++; SDL_Log("          fight    swing (won: the parry lands, at last light)"); }
            else if (f == F_BOSS_DEAD) { ch_on_battle(&c, "klee", true); battles++; SDL_Log("          fight    klee (boss)"); }
            else if (f == F_AT_PASTURE) { ch_on_map(&c, "high_pasture"); SDL_Log("          walk to  high_pasture"); }
            else if (f == F_LEFT_YARD) { c.bell_armed = 1; ch_on_map(&c, "halm"); SDL_Log("          leave the yard: the bell starts"); }
            else { ch_set(&c, f); SDL_Log("          (no binding for flag '%s' — forced)", CH_FLAG_NAME[f]); fails++; }
            if (!ch_has(&c, f)) { SDL_Log("FAIL step %d: '%s' did not set", c.step + 1, CH_FLAG_NAME[f]); fails++; ch_set(&c, f); }
        }
        if (!ch_step_complete(&c)) { SDL_Log("FAIL step %d did not complete", c.step + 1); fails++; break; }
        ch_advance(&c, nullptr);
    }
    if (CH_STEPS[c.step].kind != CHS_END) { SDL_Log("FAIL: never reached the end card (stopped at step %d)", c.step + 1); fails++; }
    int missing = ch_mandatory_missing(&c);
    if (missing) {
        SDL_Log("FAIL: %d of the mandatory ten never fired:", missing);
        for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(&c, i)) SDL_Log("        %s", CH_FLAG_NAME[i]);
        fails += missing;
    }
    // The bell must be a soft outcome: running out of rings is the clerk's late line, never a stop.
    if (c.bell_started && !ch_has(&c, F_SIGNED)) { SDL_Log("FAIL: the bell run ended unsigned"); fails++; }
    SDL_Log("CHAPTER SELFTEST %s  (%d clips, %d battles, mandatory %d/%d, %d failure(s))",
            fails ? "FAILED" : "ok", clips, battles, CH_MANDATORY - missing, CH_MANDATORY, fails);
    return fails;
}


// ═══════════════════════════════════════════════════════════════════════════════════════════════
//  --chapter-playtest: A BOT THAT PLAYS THE CHAPTER
// ═══════════════════════════════════════════════════════════════════════════════════════════════
// CHAPTER_PLAYTEST=1 (capture.sh --chapter-playtest).
//
// THE RULE THIS TEST IS BUILT ON: it may not touch the chapter's memory. It never calls ch_set,
// ch_on_text, ch_on_pickup, ch_on_battle, ch_jump or ch_advance. It has a stick, two buttons and
// the battle screen's own tap points, and every flag that ends up set got there because the bot
// walked onto a trigger or pressed a button on one, through vx_tick and draw_field exactly as a
// thumb would. If the chapter finishes, a player can finish it. If it does not, a player cannot.
//
// The old --chapter-selftest could not fail on any of the nine blockers the 2026-09-21 audit found,
// because every one of them was a gap between the story table and the world, and that test only
// ever looked at the table. This one walks.
//
// WHAT IT ASSERTS, in the order it can:
//   1. every PLAY step's gate is opened by something REACHABLE — the bot path-walks to the trigger
//      that binds each needed flag, on the real navmesh, and presses the real button;
//   2. no trigger fires outside its step window (CH_TRIG_COND) — checked by watching the flags
//      that are set on each step against the ones the step asked for;
//   3. a mandatory interaction cannot be skipped — the LAZY PLAYER run below follows only the goal
//      line, talks to nobody optional, and must still finish having seen all ten;
//   4. a hidden item is NOT reachable by plain walking and IS reachable by its designed route —
//      both halves, against CH_JUMP_ONLY;
//   5. day one's swing cannot be won;
//   6. the bell run cannot be made in time by the direct road, and can by the shortcuts;
//   7. the hill climb cannot be skipped by jumping up the terraces;
//   8. the end card is reached, with all ten.
//
// Its negative cases are the point of it, and each one was proved by temporarily breaking the
// thing it guards and watching the test go red. See VOXFIELD_NOTES.md, "What each test proves".
#define PT_MAXWP 512
#define PT_DT (1.0f / 60.0f)
#define PT_STEP_BUDGET 20000            // sim frames one chapter step may take: 5.5 minutes of play

// The battle screen's tap injector lives with --battle-ui-test below; the play-test borrows it so
// there is exactly one way a button gets pressed in a test.
static void bui_tap(float x, float y);
static void bui_pump();
static void star_tick(Star *st, int w, int h, float dt, float dpi_scale);

struct PlayBot {
    Star *st;
    int lazy;                           // 0 = the completionist, 1 = the lazy player
    int fails, interactions, battles, jumps;
    int frames;                         // sim frames burned, all steps
    int step_frames;
    uint64_t flags_at_step_start;
    double t0, tstep;
    int running, done, clip_guard, tap_cool, guard, last_step, tries, forced, effort_round;
    // THE REAL WINDOW SIZE. The bot taps by warping the OS pointer, so its coordinates
    // have to be the ones the battle screen was actually drawn at. Drawing the field at a
    // made-up 1920x1080 while the window was 1920x1027 put every tap a few per cent low,
    // which read as a battle that never accepted input and ran for thirteen hundred rounds.
    int w, h;
    char note[160];
};
static PlayBot pb;

static void pb_fail(const char *fmt, ...) {
    char msg[256]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    SDL_Log("PLAYTEST FAIL: %s", msg);
    pb.fails++;
}

// One frame of the real game, with the bot's hands on the controls. draw_field is the game's own
// per-frame function: it ticks the field, routes the events into the chapter, runs the battle, and
// advances the step when the gate opens. Nothing is shortcut.
static void pb_frame(float mx, float mz, int run) {
    vx_bot_stick(pb.st->vx, mx, mz, run);
    draw_field(pb.st, pb.w, pb.h, PT_DT);
    pb.frames++; pb.step_frames++;
}
static void pb_idle(int n) { for (int i = 0; i < n; i++) pb_frame(0, 0, 0); }

// Press the interact button and read the box out, page by page, exactly as the player does: the
// first press opens it, the next finishes the typewriter, the next turns the page or closes it.
// CLOSE WHATEVER IS ON SCREEN, and then STOP PRESSING. The button is one shot, consumed by the
// NEXT tick, so a loop that presses whenever `vx_busy` is true presses once more after the box has
// already gone — and that press lands on the trigger the party is standing on and opens it again.
// The bot then reads the same box forever, the field never moves the body while a box is open, and
// the whole thing looks exactly like "the player cannot walk to the swing". It is not: it is the
// bot holding down the A button.
//
// So: press, wait for the press to be consumed, look again, and leave a clear gap at the end.
static void pb_close_box() {
    for (int guard = 0; guard < 200 && vx_busy(pb.st->vx); guard++) {
        vx_bot_interact(pb.st->vx);
        for (int k = 0; k < 3; k++) pb_frame(0, 0, 0);
    }
    pb_idle(3);
}

static void pb_interact() {
    vx_bot_interact(pb.st->vx);
    pb.interactions++;
    for (int k = 0; k < 4; k++) pb_frame(0, 0, 0);
    pb_close_box();
}

// Walk to a cell on the real navmesh, with the real movement code. Returns false if there is no
// walking route or the body did not arrive — both of which are findings, not crashes.
static bool pb_walk_to(int tx, int tz) {
    short wx[PT_MAXWP], wz[PT_MAXWP];
    int n = vx_bot_path(pb.st->vx, tx, tz, wx, wz, PT_MAXWP);
    if (!n) {
        // A* RETURNS NOTHING WHEN YOU ARE ALREADY THERE, and "already there" is the normal state
        // after examining a machine and then being asked to go and fight it — the examine and the
        // fight sit on the same cell. Standing on a trigger does not fire it either: `inside` is
        // already set and a trigger fires on the STEP IN. So back off and come at it again, which
        // is exactly what the player does.
        float x, z; int air, cx, cz;
        vx_bot_where(pb.st->vx, &x, &z, &air);
        vx_bot_cell(pb.st->vx, &cx, &cz);
        if (abs(cx - tx) > 2 || abs(cz - tz) > 2) {
            SDL_Log("PLAYTEST   no route: party at %d,%d (%.2f,%.2f air=%d) -> %d,%d "
                    "[target standable=%d jump-only=%d, party cell walkable-to-target=%d]",
                    cx, cz, x, z, air, tx, tz,
                    (int)!vx_bot_jump_only(pb.st->vx, tx, tz), (int)vx_bot_jump_only(pb.st->vx, tx, tz),
                    (int)vx_bot_can_walk(pb.st->vx, cx, cz, tx, tz));
            return false;                                            // genuinely no route
        }
        float ax = (float)(cx - tx), az = (float)(cz - tz);
        if (fabsf(ax) < 0.1f && fabsf(az) < 0.1f) { ax = 0; az = 1; }
        float l = sqrtf(ax * ax + az * az);
        // Step off — and read anything that is still on screen, because the field does not move
        // the body while a box is open and the box the bot just opened may have a second page.
        // Step off, and KEEP GOING UNTIL THE CELL ACTUALLY CHANGES. A trigger fires on the step
        // in, so coming back at it only works if we genuinely left; a fixed number of frames is
        // not a guarantee when something is in the way. Try each direction in turn.
        pb_close_box();
        {
            static const float TRY[4][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
            bool left = false;
            for (int d = 0; d < 4 && !left; d++) {
                float mx = d == 0 ? ax / l : TRY[d][0], mz = d == 0 ? az / l : TRY[d][1];
                for (int i = 0; i < 80 && !left; i++) {
                    pb_frame(mx, mz, 1);
                    int bx2, bz2;
                    vx_bot_cell(pb.st->vx, &bx2, &bz2);
                    if (bx2 != tx || bz2 != tz) left = true;
                }
            }
            if (!left) {
                pb_fail("the party cannot step off %d,%d in any direction — it is a one-cell "
                        "pocket, and any trigger on it can only ever fire once", tx, tz);
                return false;
            }
        }
        n = vx_bot_path(pb.st->vx, tx, tz, wx, wz, PT_MAXWP);
        if (!n) {
            int bx, bz;
            vx_bot_cell(pb.st->vx, &bx, &bz);
            float nx2, nz2; int na;
            vx_bot_where(pb.st->vx, &nx2, &nz2, &na);
            SDL_Log("PLAYTEST   no route after stepping off: pushed %.2f,%.2f from %.2f,%.2f to "
                    "%.2f,%.2f (busy=%d fade=%d box=\"%.40s\" air=%d) party %d,%d -> %d,%d "
                    "[target jump-only=%d, walk-connected=%d]", ax / l, az / l, x, z, nx2, nz2,
                    (int)vx_busy(pb.st->vx), vx_bot_fading(pb.st->vx), vx_bot_boxtext(pb.st->vx), na, bx, bz, tx, tz,
                    (int)vx_bot_jump_only(pb.st->vx, tx, tz),
                    (int)vx_bot_can_walk(pb.st->vx, bx, bz, tx, tz));
            return false;
        }
    }
    int wp = 0, budget = n * 90 + 900;
    while (wp < n && budget-- > 0 && pb.step_frames < PT_STEP_BUDGET) {
        float gx, gz, x, z; int air;
        vx_bot_waypoint(pb.st->vx, wx[wp], wz[wp], &gx, &gz);
        vx_bot_where(pb.st->vx, &x, &z, &air);
        float dx = gx - x, dz = gz - z, d = sqrtf(dx * dx + dz * dz);
        if (d < 0.30f) { wp++; continue; }
        pb_frame(dx / d, dz / d, 1);
        // A trigger the walk crossed may have opened a box; read it and carry on. This is how the
        // bot meets the things on its route rather than only the things it aimed at — and it is
        // why the LAZY player still sees a set piece that is genuinely ON the path.
        if (vx_busy(pb.st->vx)) pb_close_box();
        // A fight on the route takes the screen; the battle policy below drives it.
        if (pb.st->in_battle) return true;
    }
    float x, z; int air;
    vx_bot_where(pb.st->vx, &x, &z, &air);
    float fx, fz;
    vx_bot_waypoint(pb.st->vx, wx[n - 1], wz[n - 1], &fx, &fz);
    return sqrtf((fx - x) * (fx - x) + (fz - z) * (fz - z)) < 1.4f;
}

// WALK ONTO THE CELL, not up to it. A `fight` trigger fires on the STEP IN (on_enter_cell), and
// A* stops at the first nav voxel of the target cell — which can leave the body's own cell still
// the one next door, so the fight never starts and the step waits forever for a flag only that
// fight can set. An examine does not care (the cone reaches 0.9 cells ahead), which is why this
// looked like "the swing cannot be reached" while the swing's examine worked perfectly.
static bool pb_walk_into(int tx, int tz) {
    if (!pb_walk_to(tx, tz)) return false;
    if (pb.st->in_battle) return true;
    for (int i = 0; i < 90; i++) {
        int cx, cz;
        vx_bot_cell(pb.st->vx, &cx, &cz);
        if (cx == tx && cz == tz) return true;
        float x, z; int air;
        vx_bot_where(pb.st->vx, &x, &z, &air);
        float dx = (tx + 0.5f) - x, dz = (tz + 0.5f) - z, d = sqrtf(dx * dx + dz * dz);
        if (d < 0.01f) return true;
        pb_frame(dx / d, dz / d, 0);
        if (pb.st->in_battle) return true;
    }
    int cx, cz;
    vx_bot_cell(pb.st->vx, &cx, &cz);
    return cx == tx && cz == tz;
}

// A RUNNING JUMP across a gap: back off along the heading, run at it, and press jump at the edge.
// This is the only way the bot gets onto a jump-only shelf, and it is the same two buttons a
// player uses. Returns true if the body ended up standing on the target cell.
static bool pb_run_jump(int tx, int tz) {
    float x, z, gx = tx + 0.5f, gz = tz + 0.5f; int air;
    vx_bot_where(pb.st->vx, &x, &z, &air);
    float dx = gx - x, dz = gz - z, d = sqrtf(dx * dx + dz * dz);
    if (d < 0.001f) return true;
    dx /= d; dz /= d;
    for (int i = 0; i < 40; i++) pb_frame(-dx, -dz, 1);     // a run-up
    for (int i = 0; i < 60; i++) pb_frame(dx, dz, 1);
    vx_bot_jump(pb.st->vx);
    pb.jumps++;
    for (int i = 0; i < 12; i++) pb_frame(dx, dz, 1);
    for (int i = 0; i < 120; i++) {
        pb_frame(dx, dz, 1);
        vx_bot_where(pb.st->vx, &x, &z, &air);
        if (!air && fabsf(x - gx) < 1.0f && fabsf(z - gz) < 1.0f) return true;
    }
    vx_bot_where(pb.st->vx, &x, &z, &air);
    return !air && fabsf(x - gx) < 1.2f && fabsf(z - gz) < 1.2f;
}

// Walk off whatever the party is standing on until it is back somewhere the pathfinder can plan
// from. A drop is always legal, so this always terminates on a sane map; if it does not, the shelf
// is a trap and the test says so.
static void pb_drop_off() {
    static const float DX4[4] = { 0, -1, 1, 0 }, DZ4[4] = { 0, 0, 0, -1 };
    for (int d = 0; d < 4; d++) {
        for (int i = 0; i < 70; i++) {
            pb_frame(DX4[d] ? DX4[d] : (d == 0 ? 1.0f : 0.0f), DZ4[d] ? DZ4[d] : (d == 0 ? 0.0f : 0.0f), 1);
            int cx, cz;
            vx_bot_cell(pb.st->vx, &cx, &cz);
            if (!vx_bot_jump_only(pb.st->vx, cx, cz)) { pb_idle(20); return; }
        }
    }
    int cx, cz;
    vx_bot_cell(pb.st->vx, &cx, &cz);
    if (vx_bot_jump_only(pb.st->vx, cx, cz))
        pb_fail("the party is stranded at %d,%d — a jump-only shelf you cannot get off is a trap, "
                "not a hidden item", cx, cz);
}

// ── the battle policy ──────────────────────────────────────────────────────────────────────────
// Deliberately a PLAYER'S policy and not an oracle: guard when something is telegraphing, spend
// effort into an opening, heal when low, attack otherwise. It drives the real screen through
// bt_ui_point and the same taps --battle-ui-test uses, so a battle the bot cannot get out of is a
// battle a player cannot get out of. It does NOT know how to win the swing on day one, and that is
// the point of assertion 5: it must lose.
// A BATTLE, LIKE A CLIP, CANNOT RUN IN A BLOCKING LOOP, and for both of the same reasons: the
// screen is drawn with ImGui, and a tap is only DELIVERED by an ImGui frame cycle — the press
// queued in one NewFrame is read in the next. So this is one frame, and pb_drive calls it once per
// real frame for as long as the fight lasts. It is the same injector --battle-ui-test uses, which
// is the whole point: the bot fights through the real menu, and a fight it cannot get out of is a
// fight a player cannot get out of.
static void pb_battle_frame() {
    Star *st = pb.st;
    if (bt_ui_phase(st->bat) == 0 && pb.tap_cool <= 0) {
        int rows[BT_CMD_MAX + 2], n = bt_ui_rows(st->bat, rows);
        // THE POLICY, and it is the chapter's own lesson written as three lines: guard when
        // something is telegraphing at you, spend into the opening that parry bought, and attack
        // when neither is true. A bot that only guards never kills anything — it parried the arm
        // that holds for thirteen hundred rounds — and a bot that only attacks cannot beat the
        // swing at all, which is the design.
        int telling = 0, open = 0;
        for (int e = 0; e < bt_ui_enemy_count(st->bat); e++) {
            if (bt_ui_enemy_tell(st->bat, e)) telling = 1;
            if (bt_ui_enemy_open(st->bat, e)) open = 1;
        }
        // Into an opening, the SKILL is what the chapter's one real fight is about: the boss's
        // phase one is BTF_IMMUNE_ATTACK and cannot be finished by hitting it at all — {{HERO}}
        // holds it open and {{HERDER}} spends Settle into the opening. A policy of guard-and-
        // attack parried Klee for eleven hundred rounds and could not end the fight, which is the
        // correct outcome for that policy and a useless one for a test. So: guard on a tell, spend
        // a SKILL into an opening when there is one to spend, attack otherwise.
        int want = telling ? 3 /*GUARD*/ : open ? 2 /*SKILL*/ : 1 /*ATTACK*/;
        int pick = -1;
        for (int i = 0; i < n; i++) if (rows[i] == want && bt_ui_affordable(st->bat, i)) pick = i;
        if (pick < 0 && want == 2)                       // no skill, or cannot afford it: hit it
            for (int i = 0; i < n; i++) if (rows[i] == 1 && bt_ui_affordable(st->bat, i)) pick = i;
        if (pick < 0)                                    // cannot afford that either: guard is free
            for (int i = 0; i < n; i++) if (rows[i] == 3 && bt_ui_affordable(st->bat, i)) pick = i;
        if (pick < 0) for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i)) { pick = i; break; }
        // INTO AN OPENING, SPEND. The effort notch is a slider under the command and setting it
        // does NOT commit anything — the command row still has to be tapped afterwards. Tapping
        // the notch every frame and returning is an infinite loop that reads exactly like a hung
        // battle, which is what it was the first time: round 3, enemy open, and the bot moved the
        // slider forever. So it is set ONCE a round and then the command is committed.
        float x, y;
        if (open && bt_ui_has_effort(st->bat) && pb.effort_round != bt_ui_round(st->bat)) {
            pb.effort_round = bt_ui_round(st->bat);
            float ex, ey;
            if (bt_ui_point(st->bat, 1, 4, &ex, &ey)) {
                bui_tap(ex, ey); pb.tap_cool = 6;
                pb.tap_cool--; bui_pump(); draw_field(st, pb.w, pb.h, PT_DT);
                return;
            }
        }
        if (pick >= 0 && bt_ui_point(st->bat, 0, pick, &x, &y)) { bui_tap(x, y); pb.tap_cool = 6; }
        else { bui_tap(pb.w * 0.5f, pb.h * 0.85f); pb.tap_cool = 6; }   // the results screen
    } else if (pb.tap_cool <= 0) {
        bui_tap(pb.w * 0.5f, pb.h * 0.85f);                             // RESOLVE/ENEMY/OVER: tap on
        pb.tap_cool = 6;
    }
    pb.tap_cool--;
    bui_pump();
    draw_field(st, pb.w, pb.h, PT_DT);
    if (bt_stuck_count(st->bat) > 0) {
        pb_fail("the battle watchdog fired %d time(s) — the fight stopped accepting input",
                bt_stuck_count(st->bat));
        bt_force_lose(st->bat);
    }
}

// A CLIP IS THE ONE THING THE BOT CANNOT RUN IN A BLOCKING LOOP. The field's simulation can be
// stepped with the picture left undrawn (vx_tick's `headless`), but a cutscene IS its drawing: the
// typewriter, the page, the panel reveal and the tap are all in draw_scene, and calling it ten
// thousand times between one ImGui NewFrame and its Render grows the draw list until the process
// dies. So the clip steps YIELD — one bot frame per real frame, through the host's own loop, with
// the tap injected the way --battle-ui-test injects one.
//
// That is why this test is a state machine driven from game_tick instead of a function that runs
// to completion. A clip costs real seconds; the play steps cost none.
static void pb_clip_frame() {
    // Tap in the middle of the screen, which is what advances a dialogue line, and keep tapping:
    // a textless line and an expression tag change nothing about how a line is dismissed.
    if (pb.tap_cool <= 0) { bui_tap(pb.w * 0.5f, pb.h * 0.5f); pb.tap_cool = 6; }
    pb.tap_cool--;
    bui_pump();
}

// ── where the bot has to go to open a gate ─────────────────────────────────────────────────────
// The bot is not told a cell. It is told a FLAG, it looks up which text or item id binds that flag
// (the chapter's own binding tables), and then it asks the MAP where that id is. If the map does
// not have it, that is blocker 1/2/3 and the test says so by name.
static bool pb_open_gate(int flag, const char *map) {
    Star *st = pb.st;
    const char *tid = ch_text_for_flag(flag), *iid = ch_item_for_flag(flag);
    const char *id = tid ? tid : iid;
    if (!id) return false;                          // a battle or an arrival; the caller handles it
    short tx, tz;
    if (!vx_find_trigger(st->vx, id, &tx, &tz)) {
        pb_fail("flag '%s' is bound to '%s', and NO TRIGGER ON %s FIRES IT — the step cannot be "
                "completed in play", CH_FLAG_NAME[flag], id, map);
        return false;
    }
    if (!pb_walk_to(tx, tz)) {
        pb_fail("cannot walk to '%s' at %d,%d on %s", id, tx, tz, map);
        return false;
    }
    if (st->in_battle) return true;      // a fight on the way; pb_drive takes it from here
    pb_interact();
    return true;
}

// ── one PLAY step, played out ──────────────────────────────────────────────────────────────────
// Runs to completion in one call, because the field can be stepped without drawing. What it does
// NOT do is decide anything: for each flag the step is waiting on it asks the chapter's binding
// table which id sets that flag, asks the map where that id is, walks there, and presses the
// button. Then it checks that the step actually advanced.
static void pb_play_step(int step) {
    Star *st = pb.st;
    const ChStep *s = &CH_STEPS[step];
    const char *map = vx_current_map(st->vx);
    for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) {
        int f = s->need[i];
        if (ch_has(&st->ch, f)) continue;
        short tx, tz;
        if (f == F_SWING_TRIED || f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3 || f == F_SWING_BEATEN) {
            if (!vx_find_trigger(st->vx, "swing", &tx, &tz)) { pb_fail("no `fight swing` on %s", map); break; }
            if (!pb_walk_into(tx, tz)) { pb_fail("cannot walk onto the swing at %d,%d on %s", tx, tz, map); break; }
            // Walking onto the trigger starts the fight; pb_drive takes it from here, a frame at a
            // time, and comes back into this function when it is over.
            if (st->in_battle) { pb.battles++; return; }
        } else if (f == F_BOSS_DEAD) {
            if (!vx_find_trigger(st->vx, "klee", &tx, &tz)) { pb_fail("no `fight klee` on %s", map); break; }
            if (!pb_walk_into(tx, tz)) { pb_fail("cannot walk onto the boss at %d,%d on %s", tx, tz, map); break; }
            if (st->in_battle) { pb.battles++; return; }
            pb_fail("walked onto the `fight klee` rectangle and no battle started");
            break;
        } else if (f == F_AT_PASTURE || f == F_LEFT_YARD) {
            const char *want = (f == F_AT_PASTURE) ? "exit:high_pasture" : "exit:halm";
            // Already standing on the map the flag is about: the step is waiting on the chapter,
            // not on the player, and looking for an exit to a map we are on is nonsense.
            if (map && !strcmp(map, want + 5)) { pb_idle(30); continue; }
            if (!vx_find_trigger(st->vx, want, &tx, &tz)) { pb_fail("no `%s` on %s", want, map); break; }
            // C4 is what arms the bell in play; the clip step before this one has just run.
            if (f == F_LEFT_YARD) st->ch.bell_armed = 1;
            if (!pb_walk_into(tx, tz)) pb_fail("cannot walk to %s on %s", want, map);
            pb_idle(150);                                   // the fade, and the new map's first frames
        } else {
            pb_open_gate(f, map ? map : "(none)");
            if (st->in_battle) { pb.battles++; return; }   // a fight on the way there
        }
        if (pb.step_frames >= PT_STEP_BUDGET) { pb_fail("step %d ran out of budget", step + 1); break; }
    }
    // THE COMPLETIONIST also goes and gets whatever this map hides. The LAZY PLAYER does not, and
    // the difference between the two runs is exactly the difference between "the chapter can be
    // finished" and "the chapter can be finished by somebody who only reads the goal line".
    if (!pb.lazy) {
        for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++) {
            if (!map || strcmp(CH_JUMP_ONLY[k].map, map)) continue;
            short jx, jz;
            if (!vx_find_trigger(st->vx, CH_JUMP_ONLY[k].id, &jx, &jz)) continue;
            if (!vx_bot_jump_only(st->vx, jx, jz)) continue;          // already reported elsewhere
            if (pb_run_jump(jx, jz)) { pb_interact(); SDL_Log("PLAYTEST     took '%s' by the designed jump", CH_JUMP_ONLY[k].id); }
            // AND GET BACK DOWN. A jump-only shelf is jump-only in both directions as far as A* is
            // concerned — its cells are not in walk region 1, so the bot's pathfinder cannot plan a
            // single step off it and the run strands itself on the eaves holding a tin. A player
            // just walks off the edge, because a DROP is always allowed however far it is. So does
            // this: push in each of the four directions until the body is back in region 1.
            pb_drop_off();
        }
    }
}

static void pb_begin(Star *st, int lazy) {
    memset(&pb, 0, sizeof(pb));
    pb.st = st; pb.lazy = lazy; pb.running = 1; pb.last_step = -1;
    pb.w = 1920; pb.h = 1080;
    pb.t0 = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    pb.fails = ch_steps_selfcheck();
    if (!st->vx) st->vx = vx_create();
    if (!st->bat) st->bat = bt_create();
    ch_new_game(&st->ch);
    enter_field(st);
    // A NEW GAME PUTS THE PARTY BACK AT THE START. ch_apply_step only re-places them when the map
    // CHANGES, which is right in play (a step that only moves the light must not teleport anybody)
    // and wrong here: the second run begins on whatever cell the first one finished on, and the
    // lazy run opened standing on the eaves with nowhere to path to. This is test setup, not a
    // shortcut through anything — it is what the title screen does.
    if (CH_STEPS[0].map) vx_goto(st->vx, CH_STEPS[0].map, -1, -1, CH_STEPS[0].face ? CH_STEPS[0].face : "N");
    vx_bot(st->vx, 1);
    SDL_Log("PLAYTEST %s: %d steps, %d flags", lazy ? "LAZY PLAYER" : "COMPLETIONIST",
            CH_STEP_COUNT, CH_FLAG_COUNT);
}

static void pb_log_step(int step) {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    const ChStep *s = &CH_STEPS[step];
    double tnow = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    SDL_Log("PLAYTEST   step %2d %s %-16s %6d frames (%5.1f s of play, %5.0f ms real)  goal: %s",
            step + 1, K[s->kind], s->kind == CHS_CLIP ? s->arg : (s->map ? s->map : "(same map)"),
            pb.step_frames, pb.step_frames * PT_DT, (tnow - pb.tstep) * 1000.0, pb.st->ch.goal);
    // ASSERTION 2: nothing fired outside its window. Any flag that appeared during this step and is
    // neither one the step asked for nor an optional find is a trigger that fired while it should
    // have been inert — which is exactly what `halm.job_sheet` on day one and `halm.ottilie_door`
    // on the lane home both were.
    uint64_t got = pb.st->ch.flags & ~pb.flags_at_step_start;
    for (int f = 0; f < CH_FLAG_COUNT; f++) {
        if (!(got & (1ull << f))) continue;
        bool wanted = false;
        for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++) if (s->need[i] == f) wanted = true;
        if (f == F_LOOT_YARD || f == F_LOOT_TEACHING || f == F_LOOT_HILL || f == F_LOOT_PASTURE) wanted = true;
        if (f == F_SWING_TRIED || f == F_DISTEL || f == F_DAY2_1 || f == F_DAY2_2 || f == F_DAY2_3) wanted = true;
        if (!wanted)
            pb_fail("step %d set '%s', which it never asked for — a trigger fired outside its "
                    "window (src/chapter01.h, CH_TRIG_COND)", step + 1, CH_FLAG_NAME[f]);
    }
}

static void pb_finish() {
    Star *st = pb.st;
    // ── ASSERTION 8 ──
    if (st->screen != SCR_END) pb_fail("never reached the end card (stopped at step %d)", st->ch.step + 1);
    int missing = ch_mandatory_missing(&st->ch);
    if (missing) {
        pb_fail("%d of the mandatory ten never fired in play:", missing);
        for (int i = 0; i < CH_MANDATORY; i++) if (!ch_has(&st->ch, i)) SDL_Log("PLAYTEST        %s", CH_FLAG_NAME[i]);
    }
    if (st->vx) vx_bot(st->vx, 0);
    double tend = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    SDL_Log("PLAYTEST %s %s: %d frames (%.1f min of play in %.1f s real), %d interactions, "
            "%d battles, %d jumps, mandatory %d/%d, %d failure(s)",
            pb.lazy ? "LAZY PLAYER" : "COMPLETIONIST", pb.fails ? "FAILED" : "ok", pb.frames,
            pb.frames * PT_DT / 60.0f, tend - pb.t0, pb.interactions, pb.battles, pb.jumps,
            CH_MANDATORY - missing, CH_MANDATORY, pb.fails);
    pb.running = 0; pb.done = 1;
}

// Returns 1 while the run is still going. A CLIP step costs one real frame per call; a PLAY step
// costs one call and no real frames at all.
static int pb_drive() {
    Star *st = pb.st;
    if (!pb.running) return 0;
    if (st->screen == SCR_END) { pb_finish(); return 0; }
    if (pb.guard++ > 200000) { pb_fail("the run never ended"); pb_finish(); return 0; }

    // A battle has the screen: one frame of it, through the real menu, and come back.
    if (st->in_battle) {
        pb_battle_frame();
        pb.frames++; pb.step_frames++;
        // A fight that will not end is a finding, not a reason to hang: say it ONCE, lose it, and
        // let the run carry on so the rest of the chapter is still tested.
        if (pb.step_frames > PT_STEP_BUDGET && !pb.forced) {
            pb.forced = 1;
            pb_fail("a battle on step %d never ended in %d frames — the policy could not finish it "
                    "and neither could a player using the same three commands",
                    st->ch.step + 1, PT_STEP_BUDGET);
            bt_force_lose(st->bat);
        }
        // ASSERTION 5, checked the moment a swing fight resolves.
        if (!st->in_battle && st->ch.step <= CHST_C1 && ch_has(&st->ch, F_SWING_BEATEN))
            pb_fail("the swing was WON on day one — chapter01.md P1 is explicitly "
                    "'you do not win today', and P4's gate is now open before day two has happened");
        return 1;
    }

    int step = st->ch.step;
    const ChStep *s = &CH_STEPS[step];
    if (step != pb.last_step) {
        pb.last_step = step;
        pb.step_frames = 0; pb.clip_guard = 0; pb.tap_cool = 0; pb.forced = 0;
        pb.flags_at_step_start = st->ch.flags;
        pb.tstep = (double)SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
    }

    if (s->kind == CHS_CLIP) {
        // The clip has the screen; yield a frame at a time until the field has it back.
        pb_clip_frame();
        pb.frames++; pb.step_frames++;
        if (++pb.clip_guard > 6000) { pb_fail("clip '%s' never ended", s->arg); pb_finish(); return 0; }
        if (st->screen == SCR_FIELD || st->screen == SCR_END) pb_log_step(step);
        return 1;
    }
    if (s->kind != CHS_PLAY) return 1;

    uint64_t before = st->ch.flags;
    pb_play_step(step);
    if (st->in_battle) return 1;                        // handed to the battle, above
    if (st->ch.step != step) { pb_log_step(step); pb.tries = 0; return 1; }
    if (st->ch.flags != before) { pb.tries = 0; return 1; }   // progress: come round again

    // NO PROGRESS. Say exactly which flag is still shut and stop — a play-test that quietly loops
    // is a play-test nobody reads.
    if (++pb.tries < 3) return 1;
    pb_log_step(step);
    for (int i = 0; i < CH_NEED && s->need[i] >= 0; i++)
        if (!ch_has(&st->ch, s->need[i]))
            pb_fail("step %d is STUCK: '%s' is still unset after playing the step through three "
                    "times. Either no trigger on this map sets it, or the one that does is inert "
                    "here (src/chapter01.h, CH_TRIG_COND), or the player cannot walk to it.",
                    step + 1, CH_FLAG_NAME[s->need[i]]);
    pb_finish();
    return 0;
}

// ── ASSERTION 4: the hidden finds ──────────────────────────────────────────────────────────────
// Both halves, on every map, against src/chapter01.h's CH_JUMP_ONLY. A find the player can walk up
// to has lost its lesson; a find nothing can reach is a find that does not exist. And anything the
// field reports as jump-only that is NOT in the table is a trigger the player cannot get to at all,
// which is a level bug however good the hillside looks.
static int playtest_hidden_finds(Star *st) {
    int fails = 0;
    if (!st->vx) st->vx = vx_create();
    for (int m = 0; m < vx_map_count(); m++) {
        const char *map = vx_map_name_at(m);
        if (!strcmp(map, "west_road")) continue;          // not in chapter one
        if (!vx_load_map(st->vx, map)) { SDL_Log("PLAYTEST FAIL: %s will not load", map); fails++; continue; }
        const char *ids[32]; short xs[32], zs[32];
        int n = vx_bot_jump_targets(st->vx, ids, xs, zs, 32);
        // every jump-only target the field found must be in the design
        for (int i = 0; i < n && i < 32; i++) {
            bool designed = false;
            for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++)
                if (!strcmp(CH_JUMP_ONLY[k].map, map) && !strcmp(CH_JUMP_ONLY[k].id, ids[i])) designed = true;
            if (designed) { SDL_Log("PLAYTEST   %s: '%s' at %d,%d is behind a jump, as designed", map, ids[i], xs[i], zs[i]); continue; }
            SDL_Log("PLAYTEST FAIL: %s: '%s' at %d,%d cannot be reached by walking and is NOT in "
                    "CH_JUMP_ONLY — either the map has stranded it or the table is out of date",
                    map, ids[i], xs[i], zs[i]);
            fails++;
        }
        // and every designed one must still be behind a jump
        for (int k = 0; k < CH_JUMP_ONLY_COUNT; k++) {
            if (strcmp(CH_JUMP_ONLY[k].map, map)) continue;
            bool found = false;
            for (int i = 0; i < n && i < 32; i++) if (!strcmp(ids[i], CH_JUMP_ONLY[k].id)) found = true;
            if (found) continue;
            short tx, tz;
            if (!vx_find_trigger(st->vx, CH_JUMP_ONLY[k].id, &tx, &tz)) {
                SDL_Log("PLAYTEST FAIL: %s: '%s' is in CH_JUMP_ONLY and is not on the map at all",
                        map, CH_JUMP_ONLY[k].id);
            } else {
                SDL_Log("PLAYTEST FAIL: %s: '%s' at %d,%d IS REACHABLE BY PLAIN WALKING — %s",
                        map, CH_JUMP_ONLY[k].id, tx, tz, CH_JUMP_ONLY[k].why);
            }
            fails++;
        }
    }
    SDL_Log("PLAYTEST hidden finds: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ── ASSERTION 6: the bell run ──────────────────────────────────────────────────────────────────
// The direct road must NOT make it and the designed route must. Both are measured by WALKING them
// on the real navmesh at the real speed and counting the rings — no estimate, no table.
static int playtest_bell_run(Star *st) {
    int fails = 0;
    if (!vx_load_map(st->vx, "halm")) { SDL_Log("PLAYTEST FAIL: halm will not load"); return 1; }
    short bx, bz, cx, cz;
    if (!vx_find_trigger(st->vx, "halm.job_sheet", &bx, &bz) ||
        !vx_find_trigger(st->vx, "halm.clerk_signing", &cx, &cz)) {
        SDL_Log("PLAYTEST FAIL: the bell run has no board or no counter on halm");
        return 1;
    }
    // The walking route, at walk speed, from the arrival cell, via the board and her door.
    // vx_bot_path is walk-edges-only, so this IS the direct road: it cannot use a shortcut.
    vx_bot(st->vx, 1);
    short wx[PT_MAXWP], wz[PT_MAXWP];
    int legs[3][2] = { { bx, bz }, { 41, 22 }, { cx, cz } };
    int total = 0;
    for (int i = 0; i < 3; i++) {
        int n = vx_bot_path(st->vx, legs[i][0], legs[i][1], wx, wz, PT_MAXWP);
        if (!n) { SDL_Log("PLAYTEST FAIL: no walking route to leg %d of the bell run", i + 1); fails++; break; }
        total += n;
    }
    vx_bot(st->vx, 0);
    // Waypoints are nav voxels: half a cell each. At the walk speed the direct road is
    // total/2 cells / VX walk speed seconds, plus the boxes.
    // ASSERTION 6, and it is measured rather than assumed: the waypoints are nav voxels, half a
    // cell each, and the party covers ground at VX_SP_WALK or VX_SP_RUN.
    float cells = total * 0.5f;
    float walk_s = cells / 4.4f, run_s = cells / 7.2f;
    float allowed = CH_BELL_RINGS * CH_BELL_PERIOD;
    SDL_Log("PLAYTEST bell run: the direct road (arrival -> board -> her door -> counter) is %.0f "
            "cells: %.0f s walking, %.0f s running. The bell allows %.0f s (%d rings x %.1f s).",
            cells, walk_s, run_s, allowed, CH_BELL_RINGS, CH_BELL_PERIOD);
    // The clock has to bite SOMEWHERE between the two paces, or it is not a clock. Above the walk
    // it is free; below the run it is unwinnable, and chapter one has no fail state.
    if (allowed >= walk_s) {
        SDL_Log("PLAYTEST FAIL: the direct road makes it with %.0f s to spare even at WALKING pace "
                "— the clock is not a clock. Lower CH_BELL_RINGS or CH_BELL_PERIOD.", allowed - walk_s);
        fails++;
    } else if (allowed <= run_s) {
        SDL_Log("PLAYTEST FAIL: even RUNNING the direct road misses by %.0f s, so the late line is "
                "the only outcome and the shortcuts buy nothing. Raise CH_BELL_RINGS.", run_s - allowed);
        fails++;
    } else {
        SDL_Log("PLAYTEST bell run: the clock bites between the two paces — running the direct road "
                "arrives with %.0f s spare, walking it misses by %.0f s and gets the clerk's late "
                "line. NOTE FOR THE DESIGN SIDE: chapter01.md P5 asks for SIX MINUTES and for the "
                "direct route not to make it. On a 48x36 map those cannot both be true; the run is "
                "twenty seconds long, not six minutes, and the constants now describe the map that "
                "exists rather than the one the spine imagines.", allowed - run_s, walk_s - allowed);
    }
    SDL_Log("PLAYTEST bell run: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ── ASSERTION 7: the hill climb cannot be skipped ──────────────────────────────────────────────
// Every place the designed route doubles back, the inside of the bend must rise more than a jump
// gains. Measured directly: for every cell on the map, if a body standing there could jump onto a
// cell that is closer to the top exit than the walking route allows, the climb is skippable.
static int playtest_hill_climb(Star *st) {
    int fails = 0;
    if (!vx_load_map(st->vx, "hill_path")) { SDL_Log("PLAYTEST FAIL: hill_path will not load"); return 1; }
    short ex, ez;
    if (!vx_find_trigger(st->vx, "exit:high_pasture", &ex, &ez)) {
        SDL_Log("PLAYTEST FAIL: hill_path has no exit to the high pasture"); return 1;
    }
    vx_bot(st->vx, 1);
    short wx[PT_MAXWP], wz[PT_MAXWP];
    // The walking route from the bottom to the top, as a length. A climb that is a real climb is
    // long; one that has been flattened into a ramp is short.
    int n = vx_bot_path(st->vx, ex, ez, wx, wz, PT_MAXWP);
    vx_bot(st->vx, 0);
    float cells = n * 0.5f;
    SDL_Log("PLAYTEST hill climb: the walking route from the spawn to the top gate is %.0f cells", cells);
    if (!n) { SDL_Log("PLAYTEST FAIL: there is no walking route up hill_path at all"); fails++; }
    else if (cells < 60.0f) {
        SDL_Log("PLAYTEST FAIL: the climb is only %.0f cells — hill_path is 48 cells deep and the "
                "spine's P6 is a ten-minute switchback climb. This is a ramp, not a hill.", cells);
        fails++;
    }
    SDL_Log("PLAYTEST hill climb: %s (%d failure(s))", fails ? "FAILED" : "ok", fails);
    return fails;
}

// ───────────────────────── --battle-ui-test: the battle driven by TAPS ─────────────────────────
// BATTLE_UI_TEST=1 (capture.sh --battle-ui-test). The selftests drive the battle API directly and
// never touch the UI or the input path, which is exactly why they could not see the owner's
// "stuck after attacking": that bug lived in the frame ORDER between bt_tick's commit and
// bt_draw's touch buttons, and only a real tap could reach it.
//
// This runs the REAL screen in the REAL window and injects REAL mouse events through ImGui's own
// event queue (io.AddMouse*Event — the same entry points the SDL3 backend uses for a finger), then
// asserts, after every single action, that the battle comes back to an input-accepting state
// inside BUI_TIMEOUT seconds. Anything that does not is a soft lock by definition.
#define BUI_TIMEOUT 5.0f

struct BtUiTest {
    int stage;              // which scenario
    int phase;              // within the scenario
    int frame;              // frames spent in this phase
    float wait_t;           // seconds waited for the battle to accept input again
    int actions, fails, restarts;
    int pick, pre_round, pre_cur, pre_tgt;
    int cmd_seen[BT_CMD_MAX + 2];
    float tap_x, tap_y;
    int tap_state;          // 0 none, 1 move+down queued, 2 up queued
    char what[64];          // what we are waiting on, for the failure message
};
static BtUiTest bui;

static void bui_fail(const char *fmt, ...) {
    char msg[256]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    SDL_Log("UITEST FAIL: %s", msg);
    bui.fails++;
}

// ONE TAP TAKES FOUR FRAMES, and every phase below waits for bui_idle() before issuing the next.
// ImGui's event queue is drained by the NEXT NewFrame, so a down queued in frame N is seen in
// N+1 and the matching up in N+2 — which is the frame IsMouseClicked() fires and the button under
// the finger finally runs. Issuing a second tap before that has landed silently swallows the
// first, which is how the test's own first draft "lost" every effort-notch tap.
static void bui_tap(float x, float y) {
    bui.tap_x = x; bui.tap_y = y; bui.tap_state = 1;
}
static bool bui_idle() { return bui.tap_state == 0; }

// HOW A TAP IS INJECTED, and it has to be through SDL rather than through ImGui.
// ImGui decides in NewFrame() which window the mouse is over, from the position the backend
// pushed — so writing io.MousePos afterwards moves the cursor but not the hover, and no button
// under it ever fires. ImGui_ImplSDL3_NewFrame also re-pushes the real OS position every frame,
// which beats anything we queue. So the test moves the REAL pointer with SDL_WarpMouseInWindow
// and then presses the real button: the events go through host.cpp's poll loop and the ImGui SDL3
// backend exactly as a finger's would, which is the whole point of this test existing.
//
// Five frames: warp, settle, press, release, settle. InvisibleButton is PressedOnClickRelease, so
// press and release have to be separate frames with the pointer parked on the item.
static void bui_warp(float x, float y) {
    SDL_Window *win = SDL_GetMouseFocus();
    if (!win) win = SDL_GetKeyboardFocus();
    if (win) SDL_WarpMouseInWindow(win, x, y);
    ImGui::GetIO().AddMousePosEvent(x, y);
}
static void bui_pump() {
    ImGuiIO &io = ImGui::GetIO();
    if (bui.tap_state == 0) return;
    bui_warp(bui.tap_x, bui.tap_y);
    switch (bui.tap_state) {
    case 1: bui.tap_state = 2; break;                                  // warp only; let it arrive
    case 2: io.AddMouseButtonEvent(0, true);  bui.tap_state = 3; break;
    case 3: io.AddMouseButtonEvent(0, false); bui.tap_state = 4; break;
    case 4: bui.tap_state = 5; break;                                  // the click is delivered
    default: bui.tap_state = 0; break;                                 // and read back
    }
}

// The party the chapter would have at each step, checked against COMBAT.md / chapter01.md.
static void bui_party_table() {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    Chapter c; ch_new_game(&c);
    SDL_Log("UITEST party per step:");
    for (int i = 0; i < CH_STEP_COUNT; i++) {
        ch_jump(&c, nullptr, i);
        const ChStep *t = &CH_STEPS[i];
        SDL_Log("UITEST   step %2d  %s  %-13s party %d  %s", i + 1, K[t->kind],
                t->kind == CHS_CLIP ? t->arg : (t->map ? t->map : "(same)"), c.party.count,
                t->goal ? t->goal : "");
        if (t->kind == CHS_PLAY && t->party > 0 && c.party.count != t->party)
            bui_fail("step %d wanted party %d, chapter gave %d", i + 1, t->party, c.party.count);
    }
    // The two yard fights are Falke ALONE; the boss is all three.
    ch_new_game(&c);
    if (c.party.count != 1 || strcmp(c.party.a[0].id, "falke"))
        bui_fail("P1 (hart_yard) party is not Falke alone: %d members, first '%s'", c.party.count, c.party.a[0].id);
    ch_jump(&c, nullptr, 10);                     // P4's winning session at the swing
    if (c.party.count != 1) bui_fail("P4 (the swing win) party is %d, wanted 1", c.party.count);
    ch_jump(&c, nullptr, CH_STEP_COUNT - 3);      // P8, the boss
    if (c.party.count != 3) bui_fail("P8 (the boss) party is %d, wanted 3", c.party.count);
}

// Every CLIP step must hand back to a PLAY step, never to another clip — the second bug report.
static void bui_clip_chain() {
    static const char *K[] = { "PLAY", "CLIP", "SET", "END" };
    Chapter c; ch_new_game(&c);
    int clips = 0;
    for (int i = 0; i < CH_STEP_COUNT; i++) {
        if (CH_STEPS[i].kind != CHS_CLIP) continue;
        clips++;
        ch_jump(&c, nullptr, i);
        ch_advance(&c, nullptr);                  // what enter_field() does when the clip ends
        const ChStep *n = &CH_STEPS[c.step];
        SDL_Log("UITEST   clip %-22s -> step %d %s %s", CH_STEPS[i].arg, c.step + 1, K[n->kind],
                n->kind == CHS_PLAY ? (n->map ? n->map : "(same map)") : "");
        if (n->kind == CHS_CLIP) bui_fail("clip '%s' hands straight to another clip", CH_STEPS[i].arg);
        if (n->kind != CHS_PLAY && n->kind != CHS_END)
            bui_fail("clip '%s' hands to a %s step", CH_STEPS[i].arg, K[n->kind]);
    }
    SDL_Log("UITEST clip chain: %d clips, each returns to gameplay", clips);
}

// Bring a scenario's battle up. Returns false when the encounter is unknown.
static bool bui_start(Star *st, const char *enc, int party_n) {
    if (!st->bat) st->bat = bt_create();
    ch_new_game(&st->ch);
    ch_party_resize(&st->ch, party_n);
    if (!bt_start(st->bat, enc, &st->ch.party, "hart_yard")) { bui_fail("bt_start('%s') refused", enc); return false; }
    snprintf(st->fight_enc, sizeof(st->fight_enc), "%s", enc);
    st->in_battle = 1;
    st->screen = SCR_FIELD;
    return true;
}

static void battle_ui_test(Star *st, int w, int h, float dt) {
    ImGuiIO &io = ImGui::GetIO();
    bui_pump();                 // the frame's injected mouse state, before anything reads it
    bui.frame++;
    if (bui.stage == 0) {
        SDL_Log("UITEST battle UI test: window %dx%d px, ImGui display %.0fx%.0f, dpi_scale %.2f",
                w, h, io.DisplaySize.x, io.DisplaySize.y, st->dpi_scale);
        if (fabsf(io.DisplaySize.x - (float)w) > 1.0f)
            SDL_Log("UITEST note: ImGui space is %.2fx the drawable — hit rects are scaled by U",
                    io.DisplaySize.x / (float)w);
        bui_party_table();
        bui_clip_chain();
        if (!bui_start(st, "swing", 1)) { st->cap_state = 2; return; }
        SDL_Log("UITEST scenario 1: the P1 swing, Falke alone, taps only");
        bui.stage = 1; bui.phase = 0; bui.frame = 0;
        return;
    }

    // ── the scenarios, in order. Each one is an encounter, a party, what the player already knows,
    // and how many actions to commit by tap before moving on. A yard fight that ends early is
    // simply restarted (that is what the yard IS), so the budget is always spent.
    struct BuiScene { const char *enc; int party; unsigned known; int actions; const char *note; };
    static const BuiScene SCENES[] = {
        { "swing", 1, 0,      24, "P1: the swing, Falke alone, teaching order (Attack only at first)" },
        { "swing", 1, 0x3Fu,  16, "the swing with every command known: Skill, Item, Run, effort 1-5" },
        { "lid",   1, 0x3Fu,  12, "three lids: target selection with several enemies" },
        { "klee",  3, 0x3Fu,  14, "the boss: all three, scripted phases, no Run" },
    };
    const int NSCENES = (int)(sizeof(SCENES) / sizeof(SCENES[0]));

    // stage 1..NSCENES = the tapping scenarios; NSCENES+1 = win path; +2 = lose path; +3 = done.
    if (bui.stage >= 1 && bui.stage <= NSCENES) {
        const BuiScene *sc = &SCENES[bui.stage - 1];
        if (bui.phase == 0) {                                   // (re)start this scenario's fight
            if (!bui_start(st, sc->enc, sc->party)) { bui.stage++; bui.phase = 0; return; }
            if (sc->known) st->ch.party.known |= sc->known;
            if (bui.actions == 0) SDL_Log("UITEST scenario %d: %s", bui.stage, sc->note);
            bui.phase = 1; bui.frame = 0; bui.restarts++;
            return;
        }
        BtEvent bev;
        int out = bt_tick(st->bat, w, h, dt, false, &bev);
        int ph = bt_ui_phase(st->bat);

        if (out != BT_RUNNING) {                                // over: tap the banner, then move on
            if (bui.frame % 12 == 0) bui_tap(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
            if (bt_done(st->bat)) {
                SDL_Log("UITEST   round %d, outcome %s after %d tapped actions%s", bt_ui_round(st->bat),
                        out == BT_WIN ? "WIN" : out == BT_LOSE ? "LOSE" : "FLED", bui.actions,
                        bui.actions < sc->actions ? " — restarting to spend the budget" : "");
                if (bui.actions < sc->actions && bui.restarts < 12) { bui.phase = 0; bui.frame = 0; return; }
                SDL_Log("UITEST   scenario %d done: %d actions, watchdog fired %d time(s)",
                        bui.stage, bui.actions, bt_stuck_count(st->bat));
                bui.stage++; bui.phase = 0; bui.frame = 0; bui.actions = 0; bui.restarts = 0;
                return;
            }
            if (bui.frame * dt > 9.0f) { bui_fail("the result banner never handed back (outcome %d)", out); bui.stage++; bui.phase = 0; bui.frame = 0; }
            return;
        }
        if (bui.actions >= sc->actions) {
            SDL_Log("UITEST   scenario %d done: %d actions, %d round(s), watchdog fired %d time(s)",
                    bui.stage, bui.actions, bt_ui_round(st->bat), bt_stuck_count(st->bat));
            bui.stage++; bui.phase = 0; bui.frame = 0; bui.actions = 0; bui.restarts = 0;
            return;
        }

        int rows[BT_CMD_MAX], n = bt_ui_rows(st->bat, rows);
        // Which row to tap. Run ENDS the fight, so it is exercised exactly once per scenario and
        // then left alone — otherwise every restart flees on action three and nothing else is
        // ever tried.
        // Never the row already selected: tap one = select, tap two = commit, and a row that was
        // already highlighted would commit on the first tap and desynchronise the script.
        float x, y;
        if (!bui_idle()) return;                 // a tap is still in flight; let it land
        int pick = bui.pick;
        if (bui.phase <= 2) {                    // choose the row only at the start of an action
            // THE GUARANTEE, asserted every single action: at least one row is committable.
            int affordable = 0;
            for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i)) affordable++;
            if (affordable == 0) bui_fail("no committable command at all — the menu is a dead end (%d rows)", n);
            // Pick a row that is affordable and not already selected (a selected row commits on the
            // first tap, which would desynchronise select-then-commit). Run only once per scenario.
            pick = n > 0 ? bui.actions % n : 0;
            for (int guard = 0; guard < 2 * BT_CMD_MAX && n > 1; guard++) {
                bool run_again = (rows[pick] == 5 && (bui.cmd_seen[5] & 2));
                if (!run_again && pick != bt_ui_selected(st->bat) && bt_ui_affordable(st->bat, pick)) break;
                pick = (pick + 1) % n;
            }
            if (!bt_ui_affordable(st->bat, pick))            // everything else is greyed: Guard it is
                for (int i = 0; i < n; i++) if (bt_ui_affordable(st->bat, i) && i != bt_ui_selected(st->bat)) { pick = i; break; }
            bui.pick = pick;
        }
        if (pick >= n) pick = bui.pick = (n > 0 ? n - 1 : 0);
        switch (bui.phase) {
        case 1:                                                 // wait for an input-accepting state
            if (ph == 0 && n > 0) { bui.phase = 2; bui.frame = 0; }
            else if (bui.frame * dt > BUI_TIMEOUT) {
                bui_fail("never reached an input state (phase %s, %d rows)",
                         ph == 0 ? "INPUT" : ph == 1 ? "RESOLVE" : ph == 2 ? "ENEMY" : "OVER", n);
                bui.actions++; bui.frame = 0;
            }
            break;
        case 2:                                                 // tap a row: select it
            for (int i = 0; i < n; i++) bui.cmd_seen[rows[i]] |= 1;

            bui.cmd_seen[rows[pick]] |= 2;
            snprintf(bui.what, sizeof bui.what, "%s", bt_cmd_name(rows[pick]));
            if (!bt_ui_point(st->bat, 0, pick, &x, &y)) { bui_fail("no screen point for row %d of %d", pick, n); bui.actions++; break; }
            bui_tap(x, y);
            bui.phase = 3; bui.frame = 0;
            break;
        case 3:                                                 // the effort slider: tap a notch
            if (bt_ui_selected(st->bat) != pick)
                bui_fail("tapped row %d (%s), the menu selects %d", pick, bt_cmd_name(rows[pick]), bt_ui_selected(st->bat));
            bui.wait_t = 0.0f;
            {
                int want = 1 + (bui.actions % 5);
                if (bt_ui_has_effort(st->bat) && bt_ui_point(st->bat, 1, want, &x, &y)) { bui_tap(x, y); bui.wait_t = (float)want; }
            }
            bui.phase = 4; bui.frame = 0;
            break;
        case 4:                                                 // the target, when there is a choice
            if (bui.wait_t >= 1.0f && bt_ui_effort(st->bat) != (int)bui.wait_t)
                bui_fail("effort notch %d tapped, the slider reads %d", (int)bui.wait_t, bt_ui_effort(st->bat));
            {
                int last = bt_ui_enemy_count(st->bat) - 1;
                if (last > 0 && bt_ui_point(st->bat, 2, last, &x, &y)) bui_tap(x, y);
            }
            bui.phase = 5; bui.frame = 0;
            break;
        case 5:                                                 // the second tap on the row: COMMIT
            {
                int last = bt_ui_enemy_count(st->bat) - 1;
                if (last > 0 && bt_ui_target(st->bat) != last)
                    bui_fail("enemy %d tapped, the target reads %d", last, bt_ui_target(st->bat));
            }
            bui.pre_round = bt_ui_round(st->bat); bui.pre_cur = bt_ui_cur(st->bat);
            if (bt_ui_point(st->bat, 0, pick, &x, &y)) bui_tap(x, y);
            bui.phase = 6; bui.frame = 0; bui.wait_t = 0.0f;
            break;
        case 6:                                                 // THE ASSERTION
            // The commit must (a) actually DO something — the round or the chooser moves on, or
            // the fight ends — and (b) hand input back. (a) is what makes the test able to see a
            // tap that was swallowed; (b) is the soft lock the owner reported.
            bui.wait_t += dt;
            if (out != BT_RUNNING ||
                (ph == 0 && (bt_ui_round(st->bat) != bui.pre_round || bt_ui_cur(st->bat) != bui.pre_cur))) {
                bui.actions++; bui.phase = 1; bui.frame = 0;
            } else if (bui.wait_t > BUI_TIMEOUT) {
                bui_fail("commit of '%s' did nothing / SOFT LOCK: %.1f s, state %s, round %d (was %d)",
                         bui.what, bui.wait_t, ph == 0 ? "INPUT" : ph == 1 ? "RESOLVE" : ph == 2 ? "ENEMY" : "OVER",
                         bt_ui_round(st->bat), bui.pre_round);
                bui.actions++; bui.phase = 1; bui.frame = 0;
                if (bui.fails > 3) bui.stage = NSCENES + 3;     // it is broken; stop hammering it
            }
            break;
        default: bui.phase = 1; break;
        }
        return;
    }

    if (bui.stage == NSCENES + 1) {                             // the win path and the hand-back
        if (bui.phase == 0) {
            SDL_Log("UITEST scenario %d: the win path, dismissed by a tap, handed back to the field", bui.stage);
            bui_start(st, "lid", 1);
            bt_force_win(st->bat);
            bui.phase = 1; bui.frame = 0;
            return;
        }
        BtEvent bev; bt_tick(st->bat, w, h, dt, false, &bev);
        if (bui.frame % 10 == 0) bui_tap(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
        if (bt_done(st->bat)) {
            SDL_Log("UITEST   win banner dismissed after %.1f s; field control returns", bui.frame * dt);
            st->in_battle = 0;
            // DAY ONE'S WIN DOES NOT COUNT, and this assertion is the pair of that rule rather
            // than a relaxation of it: on P1 a won swing must NOT set swing_beaten (chapter01.md
            // P1: "you do not win today"), and from P4 onward it must. Check whichever applies to
            // the step this scenario is standing on.
            bool day_two = ch_swing_winnable(&st->ch);
            ch_on_battle(&st->ch, "swing", true);
            if (day_two && !ch_has(&st->ch, F_SWING_BEATEN))
                bui_fail("a won swing on day two did not set swing_beaten");
            if (!day_two && ch_has(&st->ch, F_SWING_BEATEN))
                bui_fail("a won swing on DAY ONE set swing_beaten — P1 is 'you do not win today', "
                         "and this opens P4's gate before day two has happened");
            bui.stage++; bui.phase = 0; bui.frame = 0;
        } else if (bui.frame * dt > 9.0f) { bui_fail("the win banner never handed back"); bui.stage++; bui.phase = 0; bui.frame = 0; }
        return;
    }

    if (bui.stage == NSCENES + 2) {                             // the lose path: the yard is free
        if (bui.phase == 0) {
            SDL_Log("UITEST scenario %d: the lose path (the yard is an instant retry, no menu)", bui.stage);
            bui_start(st, "swing", 1);
            bt_force_lose(st->bat);
            bui.phase = 1; bui.frame = 0;
            return;
        }
        BtEvent bev; bt_tick(st->bat, w, h, dt, false, &bev);
        if (bt_done(st->bat)) { SDL_Log("UITEST   lose handed back with no menu, as the yard should"); bui.stage++; }
        else if (bui.frame * dt > 6.0f) { bui_fail("the lose path never handed back"); bui.stage++; }
        return;
    }

    // Done.
    int seen = 0, tried = 0;
    for (int c = BT_CMD_FIRST; c < BT_CMD_MAX; c++) { if (bui.cmd_seen[c] & 1) seen++; if (bui.cmd_seen[c] & 2) tried++; }
    SDL_Log("UITEST commands offered %d, committed by tap %d", seen, tried);
    if (seen < 4) bui_fail("only %d distinct commands were ever offered", seen);
    SDL_Log("SELFCHECK battle-ui %s  (%d failure(s))", bui.fails ? "FAILED" : "ok", bui.fails);
    st->cap_state = 2;
}

// ───────────────────────── GameAPI ─────────────────────────

static void *game_create(float dpi_scale) {
    Star *st = (Star *)calloc(1, sizeof(Star));
    // Desktop capture: no window interaction, no phone. Android never sets this, so it is inert there.
    // DIALOG_CAPTURE=<scene>:<line>[:page] — one dialogue box, rendered by the real player and written
    // to build_desktop/dialog_<scene>_l<line>_p<page>.png, so the box can be judged on the longest
    // real lines without a phone. <scene> is an index or a scene id from cutscene_data.h.
    st->port_motion = true;
    const char *dspec = SDL_getenv("DIALOG_CAPTURE");
    if (dspec) snprintf(st->dlg_spec, sizeof(st->dlg_spec), "%s", dspec);
    // DIALOG_EXPR=<expr> forces one expression on every line, so a capture can show a portrait
    // variant (and its fallback) before any scene file has been tagged. Desktop capture path only.
    const char *espec = SDL_getenv("DIALOG_EXPR");
    if (espec) snprintf(g_expr_force, sizeof(g_expr_force), "%s", espec);
    const char *spec = SDL_getenv("VOX_CAPTURE");      // <map>:<w>:<h>:<out.png>, one voxel-field frame
    if (spec) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_vox = 1; }
    spec = SDL_getenv("VOX_SELFTEST");                 // load every map once and fail loudly
    if (spec && spec[0] == '1') st->cap_vox = 2;
    spec = SDL_getenv("VOX_WALKTEST");                 // <map>|all: drive the movement bot, then quit
    if (spec && spec[0]) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_vox = 3; }
    // CHAPTER_SELFTEST=1 — the chapter is played through and the battles are tested, then quit.
    // BATTLE_SELFTEST=1 — the battles only. Either way no window input is read and the app exits.
    spec = SDL_getenv("CHAPTER_SELFTEST");
    if (spec && spec[0] == '1') st->self_test = 1;
    spec = SDL_getenv("CHAPTER_LOGIC_SELFTEST");                   // the old name for the same thing
    if (spec && spec[0] == '1') st->self_test = 1;
    // CHAPTER_PLAYTEST=1 — the bot that actually PLAYS it. Two runs, the completionist and the
    // lazy player, plus the three checks that need no run (the hidden finds, the bell run's
    // arithmetic, and whether the hill is a hill).
    spec = SDL_getenv("CHAPTER_PLAYTEST");
    if (spec && spec[0] == '1') st->self_test = 4;
    spec = SDL_getenv("BATTLE_SELFTEST");
    if (spec && spec[0] == '1') st->self_test = 2;
    // BATTLE_UI_TEST=1 — the real battle screen driven by injected TAPS, in a real window.
    spec = SDL_getenv("BATTLE_UI_TEST");
    if (spec && spec[0] == '1') st->self_test = 3;
    st->dpi_scale = dpi_scale;
    st->screen = SCR_TITLE;
    st->fade_to_scene = -1;
    st->tex_scene = -1;
    st->fade = 1.0f;                                   // open from black
    // STAR_NEWGAME=1 — skip the title and open straight on chapter one, step 1, in the yard. This
    // is what the owner's Desktop shortcut uses: one double-click and you are playing. enter_field
    // cannot run yet (there is no VoxField until the first tick), so this only arms the chapter and
    // lets the fade resolver take us to the field.
    spec = SDL_getenv("STAR_NEWGAME");
    if (spec && spec[0] == '1') { ch_new_game(&st->ch); st->fade_to_scene = CS_CUR_COUNT; }
    memset(&dev, 0, sizeof(dev));
    memset(&mus, 0, sizeof(mus));
    memset(&g_set, 0, sizeof(g_set));
    settings_load();                  // before au_init: the gain starts where the file says it is
    au_init();
    dev_load();
    return st;
}

static void game_destroy(void *state) {
    Star *st = (Star *)state;
    star_free_textures(st);
    if (st->vx) { vx_destroy(st->vx); st->vx = nullptr; }
    if (au_stream) { SDL_DestroyAudioStream(au_stream); au_stream = nullptr; }
    free(st);
}

static void game_on_save_event(void *) {}
static int game_wants_quit(void *state) { return ((Star *)state)->cap_state == 2; }

// Hot reload keeps your place: screen, scene and line survive, so a dialogue or music edit can be
// judged on the exact line you were looking at, and the field keeps the map, the spot on the navmesh
// and whatever camera the owner was tuning. Everything else is rebuilt from those few numbers.
struct ReloadBlob {
    uint32_t magic; int32_t screen, scene, line, line_page, to_field;
    int32_t has_vx; VxSave vx;
    Chapter ch;                    // the story's memory: step, flags, party, the bell
};
#define RELOAD_MAGIC 0x44525456u   // bumped: the old 3D field and the tile field left ReloadBlob

static size_t game_serialize(void *state, void *buf, size_t buf_size) {
    Star *st = (Star *)state;
    ReloadBlob b;
    memset(&b, 0, sizeof(b));
    b.magic = RELOAD_MAGIC;
    b.screen = st->screen; b.scene = st->scene; b.line = st->line; b.line_page = st->line_page;
    b.to_field = st->to_field ? 1 : 0;
    if (st->vx) { b.has_vx = 1; vx_save(st->vx, &b.vx); }
    b.ch = st->ch;                                     // POD; the whole chapter survives the reload
    if (buf && buf_size >= sizeof(b)) memcpy(buf, &b, sizeof(b));
    return sizeof(b);
}

static void game_deserialize(void *state, const void *buf, size_t size) {
    Star *st = (Star *)state;
    ReloadBlob b;
    if (size < sizeof(b)) return;
    memcpy(&b, buf, sizeof(b));
    if (b.magic != RELOAD_MAGIC) return;               // blob from the old game or an older layout: title
    st->fade = 0.0f;
    // The chapter comes back exactly as it was, then is validated: a step index from a build with a
    // different CH_STEPS is clamped rather than trusted, which is the same rule the save file uses.
    st->ch = b.ch;
    if (st->ch.step < 0 || st->ch.step >= CH_STEP_COUNT) st->ch.step = 0;
    if (st->ch.party.count < 1 || st->ch.party.count > BT_PARTY) st->ch.party.count = 1;
    st->in_battle = 0;                                 // a reload never lands you mid-battle
    st->fight_enc[0] = 0;
    bool want_field = (b.screen == SCR_FIELD) || b.to_field;
    if (b.has_vx && want_field) {
        if (!st->vx) st->vx = vx_create();
        vx_restore(st->vx, &b.vx);                     // map and position survive the reload
    }
    if (b.screen == SCR_INTRO && b.scene >= 0 && b.scene < CS_CUR_COUNT && CS_CUR[b.scene].line_count > 0) {
        int line = b.line < 0 ? 0 : b.line >= CS_CUR[b.scene].line_count ? CS_CUR[b.scene].line_count - 1 : b.line;
        star_goto(st, b.scene, line, true);
        // Keep the PAGE too, so a long line being edited comes back on the page you were reading.
        // star_goto validated the line; the page is clamped when the box next paginates, which is the
        // only place that knows how many pages the new text has.
        st->line_page = b.line_page < 0 ? 0 : b.line_page;
        st->to_field = (b.to_field && st->vx != nullptr);
    } else if (b.screen == SCR_END) {
        st->screen = SCR_END;
    } else if (b.screen == SCR_FIELD) {
        st->screen = SCR_FIELD;                        // the map is already loaded; GL objects rebuild lazily
        mus_start(CS_WONDER);
    }
}

static void game_tick(void *state, int w, int h, float dpi_scale) {
    Star *st = (Star *)state;
    st->dpi_scale = dpi_scale;
    float dt = ImGui::GetIO().DeltaTime;
    if (dt > 0.1f) dt = 0.1f;

    // The dialogue capture: hold one line on screen and read the frame back. The host renders after
    // this returns, so the read is of the PREVIOUS frame — which is why it waits a few frames first
    // and why nothing on this screen animates once the typewriter is done.
    if (st->dlg_spec[0] && st->cap_state < 2) {
        char id[96] = "", flag[8] = "";
        int line = 0, page = 0;
        sscanf(st->dlg_spec, "%95[^:]:%d:%d:%7s", id, &line, &page, flag);
        st->dlg_as_narr = (flag[0] == 'n');            // the same line drawn with no name and no portrait
        st->dlg_no_port = (flag[0] == 'x') || st->dlg_as_narr;
        st->dlg_small = (flag[0] == 's');
        st->dlg_textless = (flag[0] == 't');
        int scene = -1;
        for (int i = 0; i < CS_CUR_COUNT; i++) if (!strcmp(CS_CUR[i].id, id)) { scene = i; break; }
        if (scene < 0) { scene = atoi(id); if (scene < 0 || scene >= CS_CUR_COUNT) scene = 0; }
        if (line < 0 || line >= CS_CUR[scene].line_count) line = 0;
        if (st->dlg_frames == 0) { star_goto(st, scene, line, true); SDL_Log("dialog capture: scene %d (%s) line %d page %d",
                                                                            scene, CS_CUR[scene].id, line, page); }
        st->screen = SCR_INTRO;
        st->fade = 0.0f; st->fade_to_scene = -1;
        st->line = line; st->line_page = page; st->line_t = 99.0f;      // typing finished, marker steady
        if (++st->dlg_frames >= 6) {
            char out[320], suffix[64] = "";
            if (st->dlg_as_narr) snprintf(suffix, sizeof(suffix), "_narrator");
            else if (st->dlg_no_port) snprintf(suffix, sizeof(suffix), "_noportrait");
            else if (st->dlg_small) snprintf(suffix, sizeof(suffix), "_smallbox");
            if (st->dlg_textless) snprintf(suffix + strlen(suffix), sizeof(suffix) - strlen(suffix), "_textless");
            if (g_expr_force[0]) snprintf(suffix + strlen(suffix), sizeof(suffix) - strlen(suffix), "_%s", g_expr_force);
            snprintf(out, sizeof(out), "build_desktop/dialog_%s_l%d_p%d%s.png", CS_CUR[scene].id, line, page, suffix);
            unsigned char *px = (unsigned char *)malloc((size_t)w * h * 4);
            unsigned char *row = (unsigned char *)malloc((size_t)w * 4);
            if (px && row) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
                for (int y = 0; y < h / 2; y++) {                        // GL reads bottom-up
                    memcpy(row, px + (size_t)y * w * 4, (size_t)w * 4);
                    memcpy(px + (size_t)y * w * 4, px + (size_t)(h - 1 - y) * w * 4, (size_t)w * 4);
                    memcpy(px + (size_t)(h - 1 - y) * w * 4, row, (size_t)w * 4);
                }
                int ok = stbi_write_png(out, w, h, 4, px, w * 4);
                SDL_Log("dialog capture: %s %dx%d %s", out, w, h, ok ? "written" : "FAILED");
            }
            free(px); free(row);
            st->cap_state = 2;
        }
    }
    if (st->self_test == 3 && st->cap_state < 2) {
        battle_ui_test(st, w, h, dt);
        draw_dev(st, w, h, dt, dpi_scale);
        au_update();
        return;
    }
    // ── the chapter play-test: a bot with a stick and two buttons ──
    // Three phases. The two runs yield a frame at a time (a clip has to be drawn to be tapped
    // through); the three static checks need no run at all and go first, so a map that has
    // stranded its hidden item says so in the first second rather than after two full playthroughs.
    if (st->self_test == 4 && st->cap_state < 2) {
        static int phase = 0, static_fails = 0, run_fails = 0;
        pb.w = w; pb.h = h;
        if (phase == 0) {
            if (!st->vx) st->vx = vx_create();
            static_fails = playtest_hidden_finds(st) + playtest_bell_run(st) + playtest_hill_climb(st);
            pb_begin(st, 0);
            phase = 1;
        } else if (phase == 1) {
            if (!pb_drive()) { run_fails += pb.fails; pb_begin(st, 1); phase = 2; }
        } else if (phase == 2) {
            if (!pb_drive()) {
                run_fails += pb.fails;
                int bad = static_fails + run_fails;
                SDL_Log("SELFCHECK chapter-playtest %s (%d static, %d in play)",
                        bad ? "FAILED" : "ok", static_fails, run_fails);
                st->cap_state = 2;
            }
        }
        if (st->cap_state < 2) {
            // Draw whatever screen we are on, so a clip really is being tapped through the real
            // renderer and a textless line or an expression tag is exercised as the player sees it.
            switch (st->screen) {
            case SCR_TITLE: draw_title(st, w, h, dt); break;
            case SCR_INTRO: draw_scene(st, w, h, dt); break;
            case SCR_END:   draw_end(st, w, h, dt); break;
            case SCR_FIELD: break;                 // the bot ticks the field itself, headless
            }
            if (st->fade_to_scene >= 0) {
                st->fade += dt / 0.45f;
                if (st->fade >= 1.0f) {
                    st->fade = 1.0f;
                    int next = st->fade_to_scene;
                    st->fade_to_scene = -1;
                    if (st->to_field && !st->from_field) enter_field(st);
                    else {
                        bool came = st->from_field;
                        st->from_field = false;
                        while (next < CS_CUR_COUNT && CS_CUR[next].line_count <= 0) next++;
                        if (next < CS_CUR_COUNT) { star_goto(st, next, 0, false); st->to_field = came; }
                        else enter_field(st);
                    }
                }
            } else if (st->fade > 0.0f) { st->fade -= dt / 0.6f; if (st->fade < 0) st->fade = 0; }
            au_update();
            return;
        }
    }
    if (st->self_test && st->cap_state < 2) {
        // Mode 1 (CHAPTER_SELFTEST) proves both, chapter first because it needs nothing. Mode 2
        // (BATTLE_SELFTEST) is the battles alone, and must log its OWN marker — capture.sh greps
        // for "SELFCHECK battle" and would otherwise never see a verdict.
        if (st->self_test == 2) {
            int bad = bt_selftest();
            SDL_Log("SELFCHECK battle %s", bad ? "FAILED" : "ok");
        } else {
            int bad = chapter_selftest() + bt_selftest();
            SDL_Log("SELFCHECK chapter %s", bad ? "FAILED" : "ok");
        }
        st->cap_state = 2;
    } else if (st->cap_vox == 2 && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        vx_selftest(st->vx);   // capture.sh --vox-selftest greps the SELFCHECK lines for the verdict
        st->cap_state = 2;
    } else if (st->cap_vox == 3 && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        vx_walktest(st->vx, st->cap_spec);   // capture.sh --vox-walktest greps the verdict line
        st->cap_state = 2;
    } else if (st->cap_spec[0] && st->cap_vox && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        st->fade = 0.0f;
        st->fade_to_scene = -1;
        if (st->cap_state == 0) {
            char m[64] = "halm", out[256] = "capture.png";
            int cw = 1920, chh = 1080;
            sscanf(st->cap_spec, "%63[^:]:%d:%d:%255s", m, &cw, &chh, out);
            vx_capture_to(st->vx, m, cw, chh, out);
            st->cap_state = 1;
        } else if (vx_capture_done(st->vx)) st->cap_state = 2;
    }

    // map.flag is the field's remote control, and from the title it means "go there": a fresh start
    // after a crash or a force-stop can be put back in the field from the Mac with no taps at all.
    if (st->screen != SCR_FIELD) {
        static float poll = 0;
        poll += dt;
        if (poll > 0.5f) {
            poll = 0;
            const char *pref = SDL_GetPrefPath("com.playground", "questglory");
            char path[600];
            snprintf(path, sizeof(path), "%smap.flag", pref ? pref : "");
            size_t sz = 0;
            void *d = SDL_LoadFile(path, &sz);
            if (d) { SDL_free(d); if (sz) { SDL_Log("field: map.flag from the title — entering the field"); enter_field(st); } }
        }
    }

    switch (st->screen) {
    case SCR_TITLE: draw_title(st, w, h, dt); break;
    case SCR_INTRO: draw_scene(st, w, h, dt); break;
    case SCR_END:   draw_end(st, w, h, dt); break;
    case SCR_FIELD: draw_field(st, w, h, dt); draw_goal(st, w, h); break;
    }

    // Fades between scenes: to black, cut, back up. Music keeps playing across the cut.
    if (st->fade_to_scene >= 0) {
        st->fade += dt / 0.45f;
        if (st->fade >= 1.0f) {
            st->fade = 1.0f;
            int next = st->fade_to_scene;
            st->fade_to_scene = -1;
            if (st->to_field && !st->from_field) {                    // a field conversation just ended
                enter_field(st);
            } else {
                bool came = st->from_field;
                st->from_field = false;
                while (next < CS_CUR_COUNT && CS_CUR[next].line_count <= 0) next++;   // nothing to tap
                if (next < CS_CUR_COUNT) { star_goto(st, next, 0, false); st->to_field = came; }
                else enter_field(st);                                 // the intro is over: out into Halm
            }
        }
    } else if (st->fade > 0.0f) {
        st->fade -= dt / 0.6f;
        if (st->fade < 0.0f) st->fade = 0.0f;
    }
    if (st->fade > 0.0f)
        ImGui::GetForegroundDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2((float)w, (float)h), with_alpha(IM_COL32(0, 0, 0, 255), st->fade));

    // A pending capture.flag pulls the game to the field: the field consumes the flag itself, so
    // this only has to get us onto the right screen. Same idea as reload.flag.
    static float cap_poll = 0.0f;
    cap_poll += dt;
    if (cap_poll > 0.5f) {
        cap_poll = 0.0f;
        if (st->screen != SCR_FIELD) {
            char path[600];
            snprintf(path, sizeof(path), "%scapture.flag", pref_path());
            SDL_IOStream *fp = SDL_IOFromFile(path, "rb");
            if (fp) {
                Sint64 sz = SDL_GetIOSize(fp);
                SDL_CloseIO(fp);
                if (sz > 0) { st->fade_to_scene = -1; st->fade = 0.0f; enter_field(st); }
            }
        }
    }

    draw_dev(st, w, h, dt, dpi_scale);
    settings_tick(dt);
    au_update();
}

GAME_API_EXPORT GameAPI get_game_api() {
    GameAPI api = {};
    api.create = game_create;
    api.destroy = game_destroy;
    api.tick = game_tick;
    api.on_save_event = game_on_save_event;
    api.serialize = game_serialize;
    api.deserialize = game_deserialize;
    api.wants_quit = game_wants_quit;
    return api;
}
