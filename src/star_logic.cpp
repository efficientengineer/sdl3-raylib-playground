// star_logic.cpp — the new game: manga-panel cutscenes, dialogue, adaptive music.
// Built as libgame_logic.so and hot-reloaded by host.cpp (see CLAUDE.md, "Hot Reload Architecture").
// Scene content is generated: edit story/scenes/*.md, run ./story_prompt.py export, hot reload.
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
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
#include "stb_image_write.h"   // the dialogue capture; the implementation lives in third_party_impl.c

#include "game_api.h"
#include "cutscene_data.h"
#include "field.h"
#include "tilefield.h"
#include "voxfield.h"
#include "dialogue.h"

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
#define MAX_FACES 12              // distinct speaker portraits cached per scene; party scenes run to 7
#define PANEL_IN 0.28f            // seconds for a panel to arrive
#define FACE_IN 0.22f             // seconds for a portrait to slide in when the speaker changes
#define TYPE_CPS 42.0f            // typewriter characters per second

struct Star {
    int screen, scene, line;
    int line_page;                // which page of this line is on screen (long lines paginate)
    float line_t;                 // seconds since this line started
    float fade;                   // 1 = black, 0 = clear
    int fade_to_scene;            // scene to cut to once faded out (-1 = none, CS_INTRO_COUNT = the field)
    bool to_field;                // this scene was started from the field, so it returns there when it ends
    bool from_field;              // set for the one fade that carries us from the field into that scene
    Field *field;                 // the parked 2.5D/painted world, still reachable from the Dev panel
    TileField *tf;                // the tile field (TILES.md): parked, one Dev button away
    VoxField *vx;                 // the voxel + sprite field (VOXFIELD_NOTES.md): what SCR_FIELD is
    bool old_field;               // Dev: "Old 3D field" sends SCR_FIELD back to field.cpp
    bool tile_field;              // Dev: "Old tile field" sends SCR_FIELD back to tilefield.cpp
    bool panel_on[MAX_PANELS];
    float panel_t[MAX_PANELS];
    int panel_z[MAX_PANELS], z_next;
    int page;                     // page currently on screen; revealing a panel from another page clears it
    Tex tex[MAX_PANELS];
    Tex face[MAX_FACES];          // speaker portraits, one per distinct file in the scene
    const char *face_file[MAX_FACES];
    Tex backdrop;                 // talk scenes: the panel shown dimmed behind the conversation
    int tex_scene;                // which scene's textures are loaded (-1 = none)
    const char *face_cur;         // portrait on screen, so a change can slide the new one in
    float face_t;
    float dpi_scale;
    float title_t;
    char cap_spec[320];           // FIELD_CAPTURE=<map>:<zone>:<scale>:<out.png>, desktop capture path
    int cap_tiles;                // 1 = TILE_CAPTURE=<map>:<scale>:<out.png> instead: the tile field
    int cap_vox;                  // 1 = VOX_CAPTURE=<map>:<w>:<h>:<out.png>: the voxel field
    int cap_state;                // 0 idle, 1 asked, 2 done -> quit
    char dlg_spec[160];           // DIALOG_CAPTURE=<scene>:<line>[:page][:n|:x], the box on the Mac
    int dlg_frames;
    int dlg_as_narr, dlg_no_port, dlg_small;  // capture-only: as a Narrator / no portrait / a short box
                                   // (the short box is how the paginator and the ▼▼ marker are shown working)
};

static void star_free_textures(Star *st) {
    for (int i = 0; i < MAX_PANELS; i++) if (st->tex[i].id) glDeleteTextures(1, &st->tex[i].id);
    for (int i = 0; i < MAX_FACES; i++) if (st->face[i].id) glDeleteTextures(1, &st->face[i].id);
    if (st->backdrop.id) glDeleteTextures(1, &st->backdrop.id);
    memset(st->tex, 0, sizeof(st->tex));
    memset(st->face, 0, sizeof(st->face));
    memset(st->face_file, 0, sizeof(st->face_file));
    st->backdrop = Tex{0, 0, 0};
    st->tex_scene = -1;
    st->face_cur = nullptr;
}

static bool same_file(const char *a, const char *b) { return a == b || (a && b && !strcmp(a, b)); }

#ifndef CS_NARRATOR
#define CS_NARRATOR "Narrator"    // older cutscene_data.h; ./story_prompt.py export writes this itself
#endif

// Document text and unattributed lines: a box with no speaker name and no portrait, text centred.
// Panel and talk scenes use it as freely as narration scenes do.
static bool line_is_narrator(const CsLine *ln) { return ln->speaker && !strcmp(ln->speaker, CS_NARRATOR); }

// The scene's portraits, loaded once in star_goto. Null file or an unknown one means "no portrait",
// and the dialogue box is then drawn exactly as it is without one.
static const Tex *face_tex(Star *st, const char *file) {
    if (!file) return nullptr;
    for (int i = 0; i < MAX_FACES; i++)
        if (same_file(st->face_file[i], file)) return st->face[i].id ? &st->face[i] : nullptr;
    return nullptr;
}

static void load_faces(Star *st, const CsScene *sc) {
    int n = 0;
    for (int i = 0; i < sc->line_count && n < MAX_FACES; i++) {
        const char *f = sc->lines[i].portrait;
        if (!f) continue;
        bool seen = false;
        for (int k = 0; k < n; k++) seen = seen || same_file(st->face_file[k], f);
        if (seen) continue;
        st->face_file[n] = f;
        st->face[n++] = tex_load(f);
    }
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
    const CsScene *sc = &CS_INTRO[scene];
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
    if (!same_file(st->face_cur, sc->lines[line].portrait)) {              // a new speaker slides in
        st->face_cur = sc->lines[line].portrait;
        st->face_t = instant ? FACE_IN : 0.0f;
    }
    reveal_panel(st, sc, sc->lines[line].reveal, instant);
    mus_start(sc->lines[line].mood);
}

static void star_advance(Star *st) {
    const CsScene *sc = &CS_INTRO[st->scene];
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
    const CsScene *sc = &CS_INTRO[st->scene];
    const CsLine *ln = &sc->lines[st->line];
    bool narr = line_is_narrator(ln) || st->dlg_as_narr;   // dlg_* are set only by DIALOG_CAPTURE
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
        dlg_paginate(font, size, width, dlg_max_lines(bot - top, size), ln->text, &pg);
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
        const Tex *fa = (narr || st->dlg_no_port) ? nullptr : face_tex(st, ln->portrait);
        if (fa) {
            st->face_t += dt;
            float inset = u * 2.0f, fh = (by1 - by0) - inset * 2.0f;
            float fw = fh * (float)fa->w / (float)fa->h, cap = (bx1 - bx0) * 0.3f;
            if (fw > cap) { fw = cap; fh = fw * (float)fa->h / (float)fa->w; }
            bool right = (ln->side == CS_RIGHT);
            float a = ease_out(st->face_t / FACE_IN);
            float fx = right ? bx1 - inset - fw : bx0 + inset;
            float fy = by0 + ((by1 - by0) - fh) * 0.5f;
            ImVec2 f0(fx + (1.0f - a) * fw * 0.4f * (right ? 1.0f : -1.0f), fy), f1(f0.x + fw, f0.y + fh);
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
        dlg_paginate(font, text_size, tw, dlg_max_lines(th_avail, text_size), ln->text, &pg);
        if (st->line_page >= pg.count) st->line_page = pg.count - 1;
        const char *pb = pg.beg[st->line_page], *pe = pg.end[st->line_page];
        int total = (int)(pe - pb);
        int chars = type_t <= 0 ? 0 : (int)(type_t * TYPE_CPS);
        typing = chars < total;
        more_pages = st->line_page + 1 < pg.count;
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
    const char *title = "THE FAIR COPY", *sub = "Chapter 1  -  Clear the Entrance";
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
        st->fade_to_scene = 0;
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
// Title -> intro -> field. The field owns its own renderer, controls and dialogue box (field.cpp);
// the only things it hands back are "play this talk scene" and "the player walked into an encounter
// zone", because those belong to the game, not the map.

static int scene_by_id(const char *id) {
    for (int i = 0; i < CS_INTRO_COUNT; i++) if (!strcmp(CS_INTRO[i].id, id)) return i;
    return -1;
}

static void enter_field(Star *st) {
    if (st->old_field) { if (!st->field) st->field = field_create(); }
    else if (st->tile_field) { if (!st->tf) st->tf = tf_create(); }
    else if (!st->vx) st->vx = vx_create();
    star_free_textures(st);                       // the page art goes; the field has its own
    st->screen = SCR_FIELD;
    st->to_field = false;
    st->from_field = false;
    mus_start(CS_WONDER);
}

// A field asked for a cutscene. If it is not in the playlist yet, say so in the field's own box
// rather than fading to a blank scene.
static void field_scene(Star *st, const char *id, TileField *tf) {
    int idx = scene_by_id(id);
    if (idx >= 0 && CS_INTRO[idx].line_count > 0) { st->fade_to_scene = idx; st->from_field = true; return; }
    char msg[160];
    snprintf(msg, sizeof(msg), "(scene \"%s\" is not in the playlist yet)", id);
    if (tf) tf_message(tf, msg);
    else if (st->vx && !st->old_field && !st->tile_field) vx_message(st->vx, msg);
    else if (st->field) field_message(st->field, msg);
}

static void draw_field(Star *st, int w, int h, float dt) {
    if (st->old_field) {                                  // the parked 2.5D field, on a Dev button
        if (!st->field) st->field = field_create();
        FieldEvent ev;
        field_tick(st->field, w, h, dt, dev.open, &ev);
        if (ev.kind == FE_SCENE) field_scene(st, ev.arg, nullptr);
        else if (ev.kind == FE_ZONE) { char m[128]; snprintf(m, sizeof(m), "encounter %s", ev.arg); dev_send(m); }
        char warn[192];
        if (field_take_warning(st->field, warn, sizeof(warn))) dev_send(warn);
        return;
    }
    if (st->tile_field) {                                 // the parked tile field, on a Dev button
        if (!st->tf) st->tf = tf_create();
        TfEvent ev;
        tf_tick(st->tf, w, h, dt, dev.open, &ev);
        if (ev.kind == TFE_SCENE) field_scene(st, ev.arg, st->tf);
        else if (ev.kind == TFE_ZONE) { char m[128]; snprintf(m, sizeof(m), "encounter %s", ev.arg); dev_send(m); }
        return;
    }
    if (!st->vx) st->vx = vx_create();
    VxEvent ev;
    vx_tick(st->vx, w, h, dt, dev.open, &ev);
    if (ev.kind == VXE_SCENE) field_scene(st, ev.arg, nullptr);
    else if (ev.kind == VXE_ZONE) { char m[128]; snprintf(m, sizeof(m), "encounter %s", ev.arg); dev_send(m); }
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
    if (ImGui::Button("Restart intro")) { st->fade_to_scene = 0; dev.open = false; }
    ImGui::SameLine();
    if (ImGui::Button("Next scene") && st->screen == SCR_INTRO) { st->fade_to_scene = st->scene + 1; dev.open = false; }
    ImGui::SameLine();
    if (ImGui::Button("Title")) { st->screen = SCR_TITLE; st->title_t = 0; dev.open = false; }
    ImGui::SameLine();
    if (ImGui::Button("Field")) { st->fade_to_scene = -1; st->fade = 0.0f; enter_field(st); dev.open = false; }
    ImGui::SameLine();
    if (ImGui::Button(st->old_field ? "Tile field" : "Old 3D field")) {   // the parked field.cpp world
        st->old_field = !st->old_field;
        st->fade_to_scene = -1; st->fade = 0.0f;
        enter_field(st);
        dev.open = false;
    }
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
    ImGui::Separator();
    if (st->screen == SCR_FIELD) {
        char line[224];
        bool ofld = st->old_field, tfld = st->tile_field;
        if (ImGui::Checkbox("Old 3D field", &ofld)) { st->old_field = ofld; if (ofld) st->tile_field = false; }
        ImGui::SameLine();
        if (ImGui::Checkbox("Old tile field", &tfld)) { st->tile_field = tfld; if (tfld) st->old_field = false; }
        if (!st->old_field && !st->tile_field && st->vx) { if (vx_dev_ui(st->vx, line, sizeof(line))) dev_send(line); ImGui::Separator(); }
        else if (!st->old_field && st->tile_field && st->tf) { if (tf_dev_ui(st->tf, line, sizeof(line))) dev_send(line); ImGui::Separator(); }
        else if (st->old_field && st->field) { if (field_dev_ui(st->field, line, sizeof(line))) dev_send(line); ImGui::Separator(); }
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

// ───────────────────────── GameAPI ─────────────────────────

static void *game_create(float dpi_scale) {
    Star *st = (Star *)calloc(1, sizeof(Star));
    // Desktop capture: no window interaction, no phone. Android never sets this, so it is inert there.
    // DIALOG_CAPTURE=<scene>:<line>[:page] — one dialogue box, rendered by the real player and written
    // to build_desktop/dialog_<scene>_l<line>_p<page>.png, so the box can be judged on the longest
    // real lines without a phone. <scene> is an index or a scene id from cutscene_data.h.
    const char *dspec = SDL_getenv("DIALOG_CAPTURE");
    if (dspec) snprintf(st->dlg_spec, sizeof(st->dlg_spec), "%s", dspec);
    const char *spec = SDL_getenv("FIELD_CAPTURE");
    if (spec) snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec);
    spec = SDL_getenv("TILE_CAPTURE");                 // <map>:<scale>:<out.png>, the whole map in one PNG
    if (spec) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_tiles = 1; }
    spec = SDL_getenv("VOX_CAPTURE");                  // <map>:<w>:<h>:<out.png>, one voxel-field frame
    if (spec) { snprintf(st->cap_spec, sizeof(st->cap_spec), "%s", spec); st->cap_vox = 1; st->cap_tiles = 0; }
    spec = SDL_getenv("VOX_SELFTEST");                 // load every map once and fail loudly
    if (spec && spec[0] == '1') st->cap_vox = 2;
    st->dpi_scale = dpi_scale;
    st->screen = SCR_TITLE;
    st->fade_to_scene = -1;
    st->tex_scene = -1;
    st->fade = 1.0f;                                   // open from black
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
    if (st->field) { field_destroy(st->field); st->field = nullptr; }
    if (st->tf) { tf_destroy(st->tf); st->tf = nullptr; }
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
    uint32_t magic; int32_t screen, scene, line, line_page, to_field, has_field; FieldSave field;
    int32_t has_tf, old_field; TfSave tf;
    int32_t has_vx, tile_field; VxSave vx;
};
#define RELOAD_MAGIC 0x43525453u   // 'STRC' — bumped for the Perf HUD mode in VxSave

static size_t game_serialize(void *state, void *buf, size_t buf_size) {
    Star *st = (Star *)state;
    ReloadBlob b;
    memset(&b, 0, sizeof(b));
    b.magic = RELOAD_MAGIC;
    b.screen = st->screen; b.scene = st->scene; b.line = st->line; b.line_page = st->line_page;
    b.to_field = st->to_field ? 1 : 0;
    if (st->field) { b.has_field = 1; field_save(st->field, &b.field); }
    if (st->tf) { b.has_tf = 1; tf_save(st->tf, &b.tf); }
    if (st->vx) { b.has_vx = 1; vx_save(st->vx, &b.vx); }
    b.old_field = st->old_field ? 1 : 0;
    b.tile_field = st->tile_field ? 1 : 0;
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
    st->old_field = b.old_field != 0;
    st->tile_field = b.tile_field != 0;
    bool want_field = (b.screen == SCR_FIELD) || b.to_field;
    if (b.has_vx && want_field && !st->old_field && !st->tile_field) {
        if (!st->vx) st->vx = vx_create();
        vx_restore(st->vx, &b.vx);                     // map and position survive the reload
    }
    if (b.has_tf && want_field && st->tile_field) {                      // tf_restore validates every field it reads
        if (!st->tf) st->tf = tf_create();
        tf_restore(st->tf, &b.tf);
    }
    if (b.has_field && want_field && st->old_field) {
        if (!st->field) st->field = field_create();
        field_restore(st->field, &b.field);
    }
    if (b.screen == SCR_INTRO && b.scene >= 0 && b.scene < CS_INTRO_COUNT && CS_INTRO[b.scene].line_count > 0) {
        int line = b.line < 0 ? 0 : b.line >= CS_INTRO[b.scene].line_count ? CS_INTRO[b.scene].line_count - 1 : b.line;
        star_goto(st, b.scene, line, true);
        // Keep the PAGE too, so a long line being edited comes back on the page you were reading.
        // star_goto validated the line; the page is clamped when the box next paginates, which is the
        // only place that knows how many pages the new text has.
        st->line_page = b.line_page < 0 ? 0 : b.line_page;
        st->to_field = (b.to_field && (st->tf || st->field));
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
        int scene = -1;
        for (int i = 0; i < CS_INTRO_COUNT; i++) if (!strcmp(CS_INTRO[i].id, id)) { scene = i; break; }
        if (scene < 0) { scene = atoi(id); if (scene < 0 || scene >= CS_INTRO_COUNT) scene = 0; }
        if (line < 0 || line >= CS_INTRO[scene].line_count) line = 0;
        if (st->dlg_frames == 0) { star_goto(st, scene, line, true); SDL_Log("dialog capture: scene %d (%s) line %d page %d",
                                                                            scene, CS_INTRO[scene].id, line, page); }
        st->screen = SCR_INTRO;
        st->fade = 0.0f; st->fade_to_scene = -1;
        st->line = line; st->line_page = page; st->line_t = 99.0f;      // typing finished, marker steady
        if (++st->dlg_frames >= 6) {
            char out[320];
            snprintf(out, sizeof(out), "build_desktop/dialog_%s_l%d_p%d%s.png", CS_INTRO[scene].id, line, page,
                     st->dlg_as_narr ? "_narrator" : st->dlg_no_port ? "_noportrait" : st->dlg_small ? "_smallbox" : "");
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
    if (st->cap_vox == 2 && st->cap_state < 2) {
        if (!st->vx) st->vx = vx_create();
        st->screen = SCR_FIELD;
        vx_selftest(st->vx);   // capture.sh --vox-selftest greps the SELFCHECK lines for the verdict
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
    } else if (st->cap_spec[0] && st->cap_tiles && st->cap_state < 2) {
        if (!st->tf) st->tf = tf_create();
        st->screen = SCR_FIELD;
        st->fade = 0.0f;
        st->fade_to_scene = -1;
        if (st->cap_state == 0) {
            char m[64] = "halm", out[256] = "capture.png";
            int sc = 1;
            sscanf(st->cap_spec, "%63[^:]:%d:%255s", m, &sc, out);
            const char *nw = SDL_getenv("TILE_CAPTURE_NO_WALKERS");
            const char *wh = SDL_getenv("TILE_CAPTURE_WHOLE");
            tf_capture_to(st->tf, m, sc, out, !(nw && nw[0] == '1'), wh && wh[0] == '1');
            st->cap_state = 1;
        } else if (tf_capture_done(st->tf)) st->cap_state = 2;
    } else if (st->cap_spec[0] && st->cap_state < 2) {
        if (!st->field) st->field = field_create();
        st->old_field = true;
        st->screen = SCR_FIELD;
        st->fade = 0.0f;
        st->fade_to_scene = -1;
        if (st->cap_state == 0) {
            char m[64] = "halm", z[64] = "base", out[256] = "capture.png";
            int sc = 2;
            sscanf(st->cap_spec, "%63[^:]:%63[^:]:%d:%255s", m, z, &sc, out);
            field_capture_to(st->field, m, z, sc, out);
            st->cap_state = 1;
        } else if (field_capture_done(st->field)) st->cap_state = 2;
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
            if (d) { SDL_free(d); if (sz) { SDL_Log("tilefield: map.flag from the title — entering the field"); enter_field(st); } }
        }
    }

    switch (st->screen) {
    case SCR_TITLE: draw_title(st, w, h, dt); break;
    case SCR_INTRO: draw_scene(st, w, h, dt); break;
    case SCR_END:   draw_end(st, w, h, dt); break;
    case SCR_FIELD: draw_field(st, w, h, dt); break;
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
                while (next < CS_INTRO_COUNT && CS_INTRO[next].line_count <= 0) next++;   // nothing to tap
                if (next < CS_INTRO_COUNT) { star_goto(st, next, 0, false); st->to_field = came; }
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
