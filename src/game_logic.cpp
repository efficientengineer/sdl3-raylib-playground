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
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

// ── Procedural Audio ──
#define SFX_SAMPLE_RATE 44100
#define SFX_MAX_VOICES 8
#define SFX_BUFFER_FRAMES 2048

enum SfxType {
    SFX_NONE = 0,
    SFX_HIT,        // sword hit: short noise burst + low thump
    SFX_CRIT,       // critical: sharper, longer hit
    SFX_MISS,       // whoosh: filtered noise sweep
    SFX_HEAL,       // ascending arpeggio
    SFX_FIREBALL,   // noise + descending tone
    SFX_RAGE,       // distorted low growl
    SFX_DEFEND,     // metallic clank
    SFX_ENEMY_HIT,  // thud
    SFX_DEATH,      // descending tone
    SFX_VICTORY,    // major chord fanfare
    SFX_LEVELUP,    // ascending chime
    SFX_BUTTON,     // subtle click
    SFX_COIN,       // shop purchase jingle
    SFX_REINCARNATE // ethereal shimmer
};

struct SfxVoice {
    SfxType type;
    float t;
    float duration;
    float volume;
};

static SfxVoice sfx_voices[SFX_MAX_VOICES];
static SDL_AudioStream *sfx_stream;

static float sfx_noise() {
    static uint32_t ns = 48271;
    ns = ns * 16807 % 2147483647;
    return (float)ns / 1073741823.5f - 1.0f;
}

static float sfx_env(float t, float attack, float decay, float dur) {
    if (t < attack) return t / attack;
    float rel = (t - attack) / (dur - attack);
    return (1.0f - rel) * expf(-rel * decay);
}

static float sfx_synth(SfxVoice *v) {
    float t = v->t;
    float s = 0.0f;
    switch (v->type) {
    case SFX_HIT: {
        float env = sfx_env(t, 0.005f, 8.0f, v->duration);
        s = (sfx_noise() * 0.4f + sinf(t * 180.0f * 6.28f) * 0.6f) * env;
    } break;
    case SFX_CRIT: {
        float env = sfx_env(t, 0.003f, 5.0f, v->duration);
        s = (sfx_noise() * 0.5f + sinf(t * 250.0f * 6.28f) * 0.5f) * env;
        s += sinf(t * 500.0f * 6.28f) * env * 0.3f;
    } break;
    case SFX_MISS: {
        float env = sfx_env(t, 0.01f, 4.0f, v->duration);
        float freq = 800.0f - t * 2000.0f;
        s = sinf(t * freq * 6.28f) * 0.3f * env + sfx_noise() * 0.2f * env;
    } break;
    case SFX_HEAL: {
        float env = sfx_env(t, 0.02f, 3.0f, v->duration);
        int note = (int)(t * 8.0f);
        float freqs[] = {523.25f, 659.25f, 783.99f, 1046.5f};
        float f = freqs[note % 4];
        s = sinf(t * f * 6.28f) * 0.5f * env;
        s += sinf(t * f * 2.0f * 6.28f) * 0.2f * env;
    } break;
    case SFX_FIREBALL: {
        float env = sfx_env(t, 0.01f, 3.0f, v->duration);
        float freq = 400.0f - t * 300.0f;
        s = sfx_noise() * 0.6f * env + sinf(t * freq * 6.28f) * 0.4f * env;
    } break;
    case SFX_RAGE: {
        float env = sfx_env(t, 0.01f, 4.0f, v->duration);
        float base = sinf(t * 80.0f * 6.28f);
        s = (base > 0 ? 1.0f : -1.0f) * 0.5f * env;
        s += sfx_noise() * 0.3f * env;
    } break;
    case SFX_DEFEND: {
        float env = sfx_env(t, 0.001f, 12.0f, v->duration);
        s = sinf(t * 2000.0f * 6.28f * expf(-t * 20.0f)) * env;
    } break;
    case SFX_ENEMY_HIT: {
        float env = sfx_env(t, 0.005f, 10.0f, v->duration);
        s = sinf(t * 120.0f * 6.28f) * 0.7f * env + sfx_noise() * 0.3f * env;
    } break;
    case SFX_DEATH: {
        float env = sfx_env(t, 0.01f, 2.0f, v->duration);
        float freq = 300.0f - t * 250.0f;
        s = sinf(t * freq * 6.28f) * 0.6f * env;
    } break;
    case SFX_VICTORY: {
        float env = sfx_env(t, 0.05f, 1.5f, v->duration);
        s = (sinf(t * 523.25f * 6.28f) + sinf(t * 659.25f * 6.28f) +
             sinf(t * 783.99f * 6.28f)) * 0.25f * env;
    } break;
    case SFX_LEVELUP: {
        float env = sfx_env(t, 0.02f, 2.0f, v->duration);
        float freq = 400.0f + t * 600.0f;
        s = sinf(t * freq * 6.28f) * 0.5f * env;
        s += sinf(t * freq * 2.0f * 6.28f) * 0.2f * env;
    } break;
    case SFX_BUTTON: {
        float env = sfx_env(t, 0.001f, 20.0f, v->duration);
        s = sinf(t * 1200.0f * 6.28f) * 0.2f * env;
    } break;
    case SFX_COIN: {
        float env = sfx_env(t, 0.005f, 6.0f, v->duration);
        s = sinf(t * 1400.0f * 6.28f) * 0.3f * env;
        s += sinf(t * 1800.0f * 6.28f) * 0.2f * env * (t > 0.08f ? 1.0f : 0.0f);
    } break;
    case SFX_REINCARNATE: {
        float env = sfx_env(t, 0.1f, 1.0f, v->duration);
        float freq = 300.0f + sinf(t * 3.0f * 6.28f) * 100.0f;
        s = sinf(t * freq * 6.28f) * 0.3f * env;
        s += sinf(t * freq * 1.5f * 6.28f) * 0.2f * env;
        s += sfx_noise() * 0.05f * env;
    } break;
    default: break;
    }
    return s * v->volume;
}

static float sfx_duration(SfxType type) {
    switch (type) {
    case SFX_HIT: return 0.15f;
    case SFX_CRIT: return 0.25f;
    case SFX_MISS: return 0.2f;
    case SFX_HEAL: return 0.5f;
    case SFX_FIREBALL: return 0.4f;
    case SFX_RAGE: return 0.3f;
    case SFX_DEFEND: return 0.12f;
    case SFX_ENEMY_HIT: return 0.12f;
    case SFX_DEATH: return 0.6f;
    case SFX_VICTORY: return 1.0f;
    case SFX_LEVELUP: return 0.5f;
    case SFX_BUTTON: return 0.04f;
    case SFX_COIN: return 0.2f;
    case SFX_REINCARNATE: return 1.2f;
    default: return 0.1f;
    }
}

static void sfx_play(SfxType type, float volume = 0.5f) {
    for (int i = 0; i < SFX_MAX_VOICES; i++) {
        if (sfx_voices[i].type == SFX_NONE) {
            sfx_voices[i] = {type, 0.0f, sfx_duration(type), volume};
            return;
        }
    }
    sfx_voices[0] = {type, 0.0f, sfx_duration(type), volume};
}

// forward declarations for music system (defined below)
enum MusicScene { MUS_NONE = 0, MUS_TITLE, MUS_TOWN, MUS_COMBAT, MUS_VICTORY };
static struct MusicState {
    MusicScene scene;
    MusicScene pending_scene; // scene to switch to after fade-out
    float bpm, beat_t, volume, target_volume, fade_speed;
    int beat, bar;
    struct { float freq, t, duration, volume; int type; } notes[6];
} music;
static float mus_generate_sample();
static void mus_set_scene(MusicScene scene);

static void sfx_generate(float *buf, int frames) {
    float dt = 1.0f / SFX_SAMPLE_RATE;
    for (int i = 0; i < frames; i++) {
        float sample = 0.0f;
        for (int v = 0; v < SFX_MAX_VOICES; v++) {
            if (sfx_voices[v].type == SFX_NONE) continue;
            sample += sfx_synth(&sfx_voices[v]);
            sfx_voices[v].t += dt;
            if (sfx_voices[v].t >= sfx_voices[v].duration)
                sfx_voices[v].type = SFX_NONE;
        }
        sample += mus_generate_sample();
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        buf[i] = sample;
    }
}

static void sfx_init() {
    memset(sfx_voices, 0, sizeof(sfx_voices));
    SDL_AudioSpec spec;
    spec.freq = SFX_SAMPLE_RATE;
    spec.format = SDL_AUDIO_F32;
    spec.channels = 1;
    sfx_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (sfx_stream) SDL_ResumeAudioStreamDevice(sfx_stream);
}

static void sfx_stop_all() {
    for (int i = 0; i < SFX_MAX_VOICES; i++)
        sfx_voices[i].type = SFX_NONE;
    if (sfx_stream) SDL_ClearAudioStream(sfx_stream);
}

static void sfx_update() {
    if (!sfx_stream) return;
    bool any_active = (music.scene != MUS_NONE);
    if (!any_active) {
        for (int i = 0; i < SFX_MAX_VOICES; i++)
            if (sfx_voices[i].type != SFX_NONE) { any_active = true; break; }
    }
    if (!any_active) return;
    int queued = SDL_GetAudioStreamQueued(sfx_stream);
    if (queued > (int)(SFX_SAMPLE_RATE * sizeof(float) / 8)) return;
    int frames = SFX_SAMPLE_RATE / 60;
    float buf[2048];
    if (frames > 2048) frames = 2048;
    sfx_generate(buf, frames);
    SDL_PutAudioStreamData(sfx_stream, buf, frames * sizeof(float));
}

// ── Procedural Music (implementation) ──

static float mus_note_synth(int slot) {
    auto &n = music.notes[slot];
    float t = n.t;
    float freq = n.freq;
    float dur = n.duration;
    float env;
    float s = 0.0f;
    switch (n.type) {
    case 0:
        env = (t < 0.01f) ? t / 0.01f : expf(-(t - 0.01f) * 4.0f / dur);
        s = sinf(t * freq * 6.2832f) * env;
        break;
    case 1: {
        env = (t < 0.005f) ? t / 0.005f : expf(-(t - 0.005f) * 3.0f / dur);
        float phase = fmodf(t * freq, 1.0f);
        s = (2.0f * phase - 1.0f) * env;
        float phase2 = fmodf(t * freq * 1.002f, 1.0f);
        s = (s + (2.0f * phase2 - 1.0f) * env * 0.7f) * 0.6f;
        s = tanhf(s * 2.0f);
    } break;
    case 2: {
        env = (t < 0.005f) ? t / 0.005f : expf(-(t - 0.005f) * 5.0f / dur);
        float phase = fmodf(t * freq, 1.0f);
        s = (phase < 0.5f ? 1.0f : -1.0f) * env * 0.5f;
    } break;
    case 3: {
        env = expf(-t * 30.0f);
        float kf = 150.0f * expf(-t * 40.0f) + 40.0f;
        s = sinf(t * kf * 6.2832f) * env;
    } break;
    case 4: {
        env = expf(-t * 20.0f);
        static uint32_t ns4 = 77777;
        ns4 = ns4 * 16807 % 2147483647;
        float noise = (float)ns4 / 1073741823.5f - 1.0f;
        s = (noise * 0.7f + sinf(t * 200.0f * 6.2832f) * 0.3f) * env;
    } break;
    case 5: {
        env = expf(-t * 60.0f);
        static uint32_t ns5 = 33333;
        ns5 = ns5 * 16807 % 2147483647;
        float noise = (float)ns5 / 1073741823.5f - 1.0f;
        s = noise * env * 0.3f;
    } break;
    }
    return s * n.volume;
}

static void mus_trigger(int slot, float freq, float dur, float vol, int type) {
    if (slot < 0 || slot >= 6) return;
    music.notes[slot] = {freq, 0.0f, dur, vol, type};
}

// note frequencies (MIDI-ish)
static float note_freq(int note) { return 440.0f * powf(2.0f, (note - 69) / 12.0f); }

// ── Combat music: DragonForce-style power metal ──
// E minor pentatonic arpeggios, fast double kick, power chords
static void mus_combat_beat(int beat, int bar) {
    int b16 = beat; // 16th notes at 170 BPM
    int bar16 = bar % 4;

    // Double kick: every 16th note
    if (b16 % 2 == 0) mus_trigger(0, 0, 0.08f, 0.5f, 3);

    // Snare on beats 4, 12 (backbeat in 16ths)
    if (b16 == 4 || b16 == 12) mus_trigger(1, 0, 0.1f, 0.45f, 4);

    // Hihat every other 16th
    if (b16 % 2 == 1) mus_trigger(2, 0, 0.05f, 0.2f, 5);

    // Power chord rhythm guitar — palm mute chug pattern
    // E2=82.4, B2=123.5, E3=164.8, G3=196, A3=220
    float chords[][2] = {{82.4f,123.5f},{82.4f,123.5f},{110.0f,164.8f},{98.0f,146.8f}};
    float *chord = chords[bar16];
    if (b16 % 4 == 0 || b16 % 4 == 3) {
        mus_trigger(3, chord[0], 0.12f, 0.3f, 2);
        mus_trigger(4, chord[1], 0.12f, 0.2f, 2);
    }

    // Lead arpeggio — fast E minor pentatonic runs
    // E4 B4 D5 E5 G5 B5 E6 — cycle through
    int lead_notes[] = {64, 71, 74, 76, 79, 83, 76, 71, 67, 64, 62, 59, 64, 67, 71, 74};
    int pattern = (bar16 * 16 + b16) % 16;
    int note = lead_notes[pattern];
    if (bar16 < 3 || b16 < 12) {
        mus_trigger(5, note_freq(note), 0.15f, 0.25f, 1);
    } else {
        // held power chord for drama
        mus_trigger(5, note_freq(64), 0.4f, 0.3f, 1);
    }
}

// ── Town music: Heartless Bastards-style bluesy rock ──
// Warm, driving, pentatonic blues in A
static void mus_town_beat(int beat, int bar) {
    int b8 = beat; // 8th notes at 110 BPM
    int bar8 = bar % 8;

    // Kick: 1 and 3 (beats 0, 4 in 8th notes)
    if (b8 == 0 || b8 == 4) mus_trigger(0, 0, 0.12f, 0.45f, 3);
    // Snare: 2 and 4
    if (b8 == 2 || b8 == 6) mus_trigger(1, 0, 0.12f, 0.35f, 4);
    // Hihat: every 8th
    mus_trigger(2, 0, 0.06f, 0.15f, 5);

    // Bass: A blues pattern — A2 C3 D3 E3
    float bass_notes[] = {110.0f, 130.8f, 146.8f, 164.8f, 146.8f, 130.8f, 110.0f, 98.0f};
    int bass_pattern[][8] = {
        {0,0,0,3,0,0,0,5}, {0,0,1,3,0,0,2,5},
        {3,3,3,5,3,3,3,7}, {0,0,0,3,0,0,2,1},
        {0,0,0,3,0,0,0,5}, {0,0,1,3,0,0,2,5},
        {3,3,5,5,3,3,0,0}, {0,0,0,3,2,1,0,0},
    };
    int bi = bass_pattern[bar8][b8];
    float bf = bass_notes[bi];
    if (b8 % 2 == 0)
        mus_trigger(3, bf, 0.25f, 0.35f, 0);

    // Lead: pentatonic blues melody — A C D Eb E G
    int blues_scale[] = {57, 60, 62, 63, 64, 67, 69, 72, 74, 76};
    int melody[][8] = {
        {4,4,-1,7,6,-1,4,2}, {4,4,-1,6,4,-1,2,0},
        {5,5,-1,7,9,-1,7,5}, {4,2,-1,4,6,-1,4,2},
        {7,7,-1,9,7,-1,5,4}, {2,4,-1,5,4,-1,2,0},
        {5,7,-1,9,7,-1,5,4}, {4,2,-1,0,2,-1,4,4},
    };
    int mi = melody[bar8][b8];
    if (mi >= 0) {
        int note = blues_scale[mi % 10];
        mus_trigger(5, note_freq(note + 12), 0.3f, 0.2f, 0);
    }

    // Rhythm guitar: warm chords on downbeats
    // A5=55, D5=73.4, E5=82.4
    float rchords[][2] = {
        {110.0f,165.0f},{110.0f,165.0f},{146.8f,220.0f},{146.8f,220.0f},
        {110.0f,165.0f},{110.0f,165.0f},{164.8f,247.0f},{110.0f,165.0f}
    };
    if (b8 == 0 || b8 == 4) {
        mus_trigger(4, rchords[bar8][0], 0.3f, 0.15f, 2);
    }
}

// ── Title music: gentle, anticipatory ──
static void mus_title_beat(int beat, int bar) {
    int b4 = beat;
    int bar4 = bar % 4;

    // Soft arpeggiated chords — Am, F, C, G
    int chord_notes[][3] = {{57,60,64},{53,57,60},{48,52,55},{55,59,62}};
    int *cn = chord_notes[bar4];
    int arp = cn[b4 % 3];
    mus_trigger(5, note_freq(arp + 12), 0.6f, 0.2f, 0);
    if (b4 == 0) mus_trigger(3, note_freq(cn[0]), 0.8f, 0.25f, 0);
}

// ── Victory fanfare ──
static void mus_victory_beat(int beat, int bar) {
    int b = beat;
    // Triumphant ascending pattern
    int fanfare[] = {60,64,67,72,67,72,76,72};
    if (b < 8) {
        mus_trigger(5, note_freq(fanfare[b]), 0.3f, 0.3f, 0);
        if (b == 0) mus_trigger(0, 0, 0.12f, 0.4f, 3);
        if (b == 4) mus_trigger(1, 0, 0.12f, 0.3f, 4);
    }
    if (b == 0 && bar % 2 == 0) mus_trigger(3, note_freq(48), 0.8f, 0.3f, 0);
}

static float mus_scene_volume(MusicScene scene) {
    switch (scene) {
    case MUS_COMBAT: return 0.35f;
    case MUS_TOWN:   return 0.3f;
    case MUS_TITLE:  return 0.25f;
    case MUS_VICTORY:return 0.3f;
    default: return 0.0f;
    }
}

static void mus_apply_scene(MusicScene scene) {
    music.scene = scene;
    music.beat_t = 0;
    music.beat = 0;
    music.bar = 0;
    memset(music.notes, 0, sizeof(music.notes));
    switch (scene) {
    case MUS_COMBAT: music.bpm = 170; break;
    case MUS_TOWN:   music.bpm = 110; break;
    case MUS_TITLE:  music.bpm = 80;  break;
    case MUS_VICTORY:music.bpm = 140; break;
    default: music.bpm = 0; break;
    }
    music.target_volume = mus_scene_volume(scene);
    music.fade_speed = 3.0f; // fade in over ~0.3s
}

static void mus_set_scene(MusicScene scene) {
    if (music.scene == scene && music.pending_scene == MUS_NONE) return;
    if (music.scene == MUS_NONE) {
        mus_apply_scene(scene);
        music.volume = 0.0f;
        return;
    }
    if (scene == music.pending_scene) return;
    music.pending_scene = scene;
    music.target_volume = 0.0f;
    music.fade_speed = 4.0f; // fade out over ~0.25s
}

static int mus_beats_per_bar() {
    switch (music.scene) {
    case MUS_COMBAT: return 16; // 16th notes
    case MUS_TOWN:   return 8;  // 8th notes
    case MUS_TITLE:  return 4;
    case MUS_VICTORY:return 8;
    default: return 4;
    }
}

static float mus_beat_duration() {
    switch (music.scene) {
    case MUS_COMBAT: return 60.0f / music.bpm / 4.0f; // 16th notes
    case MUS_TOWN:   return 60.0f / music.bpm / 2.0f; // 8th notes
    default:         return 60.0f / music.bpm;
    }
}

static float mus_generate_sample() {
    float dt = 1.0f / SFX_SAMPLE_RATE;

    // fade volume toward target
    if (music.volume < music.target_volume) {
        music.volume += music.fade_speed * dt;
        if (music.volume > music.target_volume) music.volume = music.target_volume;
    } else if (music.volume > music.target_volume) {
        music.volume -= music.fade_speed * dt;
        if (music.volume < music.target_volume) music.volume = music.target_volume;
    }

    // when faded out and a scene change is pending, switch
    if (music.pending_scene != MUS_NONE && music.volume <= 0.001f) {
        mus_apply_scene(music.pending_scene);
        music.pending_scene = MUS_NONE;
        music.volume = 0.0f;
    }

    if (music.scene == MUS_NONE || music.bpm == 0) return 0.0f;

    float beat_dur = mus_beat_duration();
    music.beat_t += dt;
    if (music.beat_t >= beat_dur) {
        music.beat_t -= beat_dur;
        music.beat++;
        if (music.beat >= mus_beats_per_bar()) {
            music.beat = 0;
            music.bar++;
        }
        switch (music.scene) {
        case MUS_COMBAT: mus_combat_beat(music.beat, music.bar); break;
        case MUS_TOWN:   mus_town_beat(music.beat, music.bar); break;
        case MUS_TITLE:  mus_title_beat(music.beat, music.bar); break;
        case MUS_VICTORY:mus_victory_beat(music.beat, music.bar); break;
        default: break;
        }
    }

    float sample = 0.0f;
    for (int i = 0; i < 6; i++) {
        if (music.notes[i].duration <= 0) continue;
        sample += mus_note_synth(i);
        music.notes[i].t += dt;
        if (music.notes[i].t >= music.notes[i].duration)
            music.notes[i].duration = 0;
    }
    return sample * music.volume;
}

// ── RNG ──
static uint64_t rng_state = 12345;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (uint32_t)(rng_state & 0xFFFFFFFF);
}
static int rng_range(int lo, int hi) { return lo + (int)(rng() % (hi - lo + 1)); }
static int d20(void) { return rng_range(1, 20); }

// ── Constants ──
#define MAX_PARTY 4
#define MAX_ENEMIES 6
#define MAX_LEVEL 20
#define MAX_CLASSES 6
#define MAX_FEATS 30
#define MAX_INVENTORY 20
#define MAX_COMBAT_LOG 40
#define MAX_QUESTS 8
#define MAX_PAST_LIVES 3

enum Class { CLASS_FIGHTER, CLASS_ROGUE, CLASS_WIZARD, CLASS_CLERIC, CLASS_RANGER, CLASS_BARBARIAN };
static const char *CLASS_NAMES[] = { "Fighter", "Rogue", "Wizard", "Cleric", "Ranger", "Barbarian" };
static const ImVec4 CLASS_COLORS[] = {
    {0.8f,0.5f,0.2f,1}, {0.6f,0.6f,0.2f,1}, {0.4f,0.4f,0.9f,1},
    {0.9f,0.8f,0.3f,1}, {0.3f,0.7f,0.3f,1}, {0.8f,0.3f,0.3f,1}
};

enum Race { RACE_HUMAN, RACE_DWARF, RACE_ELF, RACE_HALFLING };
static const char *RACE_NAMES[] = { "Human", "Dwarf", "Elf", "Halfling" };

enum Stat { STR, DEX, CON, INT, WIS, CHA, NUM_STATS };
static const char *STAT_NAMES[] = { "STR", "DEX", "CON", "INT", "WIS", "CHA" };
static const int RACE_BONUSES[][NUM_STATS] = {
    {0,0,0,0,0,0},  // human: bonus feat
    {0,0,2,0,2,-2}, // dwarf
    {0,2,0,2,0,-2}, // elf
    {-2,2,0,0,0,2}, // halfling
};

// ── Feats ──
struct FeatDef {
    const char *name;
    const char *desc;
    int req_level;
    int req_class; // -1 = any
    int req_stat;  // -1 = none
    int req_stat_val;
};

static const FeatDef FEATS[] = {
    {"Power Attack",    "+50% melee dmg, -2 atk",    1, CLASS_FIGHTER, STR, 13},
    {"Cleave",          "Kill grants bonus attack",   1, CLASS_FIGHTER, STR, 13},
    {"Weapon Focus",    "+1 attack rolls",            1, -1, -1, 0},
    {"Toughness",       "+3 HP per level",            1, -1, -1, 0},
    {"Two Weapon",      "Dual wield penalty reduced", 1, -1, DEX, 15},
    {"Sneak Attack+",   "+1d6 sneak attack",          1, CLASS_ROGUE, -1, 0},
    {"Evasion",         "DEX save = no damage",       3, CLASS_ROGUE, -1, 0},
    {"Empower Spell",   "+50% spell damage",          3, CLASS_WIZARD, INT, 15},
    {"Maximize Spell",  "Max spell damage",           6, CLASS_WIZARD, INT, 17},
    {"Extra Turning",   "+4 turn undead uses",        1, CLASS_CLERIC, -1, 0},
    {"Combat Casting",  "+4 concentration",           1, -1, -1, 0},
    {"Improved Crit",   "Double crit range",          8, -1, -1, 0},
    {"Great Fortitude", "+2 Fort saves",              1, -1, -1, 0},
    {"Iron Will",       "+2 Will saves",              1, -1, -1, 0},
    {"Lightning Ref.",  "+2 Ref saves",               1, -1, -1, 0},
    {"Dodge",           "+1 AC",                      1, -1, DEX, 13},
    {"Mobility",        "+4 AC vs opportunity",       3, -1, DEX, 13},
    {"Rage Power+",     "+2 rage damage",             4, CLASS_BARBARIAN, -1, 0},
    {"Favored Enemy+",  "+2 dmg vs chosen type",      1, CLASS_RANGER, -1, 0},
    {"Rapid Shot",      "Extra ranged attack",        1, CLASS_RANGER, DEX, 13},
    {"Spell Penetration","+2 spell resistance",       1, CLASS_WIZARD, -1, 0},
    {"Extend Spell",    "Double buff duration",       1, -1, -1, 0},
    {"Shield Mastery",  "+2 AC with shield",          4, CLASS_FIGHTER, -1, 0},
    {"Precision",       "+1d6 dmg on full attack",    6, -1, DEX, 15},
    {"Great Cleave",    "Unlimited cleave attacks",   4, CLASS_FIGHTER, STR, 15},
};
static const int NUM_FEATS = sizeof(FEATS) / sizeof(FEATS[0]);

// ── Items ──
enum ItemSlot { SLOT_WEAPON, SLOT_ARMOR, SLOT_SHIELD, SLOT_RING, SLOT_NONE };
struct Item {
    char name[32];
    int slot;
    int bonus;        // main stat: atk for weapon, AC for armor
    int stat_bonus;   // which stat gets +bonus_val
    int bonus_val;
    int level_req;
    bool equipped;
};

// ── Character ──
struct Character {
    char name[24];
    int race;
    int class_levels[MAX_CLASSES]; // multiclass
    int primary_class;
    int level;
    int xp, xp_next;
    int base_stats[NUM_STATS];
    int hp, max_hp, sp, max_sp;
    bool feats[NUM_FEATS];
    Item inventory[MAX_INVENTORY];
    int inv_count;
    int past_lives[MAX_CLASSES]; // reincarnation count per class
    int total_past_lives;
    bool alive;
    // combat state
    int combat_hp;
    int initiative;
    bool defending;
    float shake_timer; // seconds remaining of shake animation
};

// ── Enemies ──
struct Enemy {
    char name[32];
    int level, hp, max_hp, ac, attack, damage;
    bool alive;
    int xp_reward, gold_reward;
    int initiative;
    float shake_timer;
};

// ── Combat ──
enum CombatAction { ACT_ATTACK, ACT_ABILITY, ACT_DEFEND, ACT_ITEM };
struct CombatLog {
    char lines[MAX_COMBAT_LOG][128];
    int count;
};
static void log_combat(CombatLog *cl, const char *fmt, ...) {
    if (cl->count >= MAX_COMBAT_LOG) {
        memmove(cl->lines[0], cl->lines[1], (MAX_COMBAT_LOG-1)*128);
        cl->count = MAX_COMBAT_LOG - 1;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cl->lines[cl->count], 128, fmt, ap);
    va_end(ap);
    cl->count++;
}

// ── Quest ──
struct Quest {
    char name[48];
    char desc[128];
    int min_level, enemy_count, enemy_level;
    int xp_reward, gold_reward;
    bool completed;
};

// ── Game State ──
enum Screen { SCR_TITLE, SCR_CREATE, SCR_TOWN, SCR_QUEST_BOARD, SCR_COMBAT, SCR_LEVELUP, SCR_CHARACTER, SCR_REINCARNATE, SCR_VICTORY };

struct Game {
    Screen screen;
    Character party[MAX_PARTY];
    int party_size;
    int gold;

    Quest quests[MAX_QUESTS];
    int quest_count;
    int current_quest;

    Enemy enemies[MAX_ENEMIES];
    int enemy_count;
    int turn_index; // index into init_order
    bool player_turn; // true = waiting for player input on current turn
    int selected_target;
    int selected_ability;
    CombatLog combat_log;
    int combat_round;

    // Initiative order: positive = party index, negative = -(enemy_index+1)
    int init_order[MAX_PARTY + MAX_ENEMIES];
    int init_count;
    int init_current; // current position in init_order

    // creation state
    int create_race;
    int create_class;
    int create_stats[NUM_STATS];
    int create_points;
    char create_name[24];

    // levelup state
    int levelup_who;
    int levelup_class_choice;
    int levelup_feat_choice;
    bool levelup_needs_feat;

    int difficulty; // 0=Normal, 1=Hard, 2=Elite
};

// ── Bug Tracker ──
#define MAX_BUGS 32
#define BUG_DESC_LEN 256

struct BugReport {
    char desc[BUG_DESC_LEN];
    char screen_name[24];
    int gold, party_size, combat_round;
    bool completed;
};

static struct {
    BugReport bugs[MAX_BUGS];
    int count;
    bool panel_open;
    bool creating;
    char new_desc[BUG_DESC_LEN];
} qa;

// ── Dev Message Log ──
#define MAX_MSGS 25
#define MSG_LEN 512
struct DevMsg {
    char text[MSG_LEN];
    char timestamp[12]; // "HH:MM:SS"
    bool from_dev;
};
static struct {
    DevMsg msgs[MAX_MSGS];
    int count;
    char input[MSG_LEN];
    bool composing;
} dev_log;

static const char *dev_log_path_buf = nullptr;
static const char *get_dev_log_path() {
    if (dev_log_path_buf) return dev_log_path_buf;
    const char *pref = SDL_GetPrefPath("com.playground", "questglory");
    if (!pref) return nullptr;
    static char path[512];
    snprintf(path, sizeof(path), "%sdev_log.txt", pref);
    dev_log_path_buf = path;
    return path;
}

static void dev_log_save() {
    const char *path = get_dev_log_path();
    if (!path) return;
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f) return;
    for (int i = 0; i < dev_log.count; i++) {
        char prefix[32];
        if (dev_log.msgs[i].timestamp[0])
            snprintf(prefix, sizeof(prefix), "[%s %s] ",
                     dev_log.msgs[i].from_dev ? "DEV" : "YOU",
                     dev_log.msgs[i].timestamp);
        else
            snprintf(prefix, sizeof(prefix), "[%s] ",
                     dev_log.msgs[i].from_dev ? "DEV" : "YOU");
        SDL_WriteIO(f, prefix, strlen(prefix));
        SDL_WriteIO(f, dev_log.msgs[i].text, strlen(dev_log.msgs[i].text));
        SDL_WriteIO(f, "\n", 1);
    }
    SDL_CloseIO(f);
}

static void dev_log_load() {
    memset(&dev_log, 0, sizeof(dev_log));
    const char *path = get_dev_log_path();
    if (!path) return;
    size_t sz = 0;
    char *data = (char *)SDL_LoadFile(path, &sz);
    if (!data) return;
    char *p = data;
    char *end = data + sz;
    // Keep the newest MAX_MSGS lines: skip leading lines if file is longer
    int total = 0;
    for (char *q = data; q < end; ) {
        char *nl = (char *)memchr(q, '\n', end - q);
        if (!nl) nl = end;
        if (nl > q) total++;
        q = nl + 1;
    }
    for (int skip_lines = total - MAX_MSGS; skip_lines > 0 && p < end; skip_lines--) {
        char *nl = (char *)memchr(p, '\n', end - p);
        if (!nl) nl = end;
        p = nl + 1;
    }
    while (p < end && dev_log.count < MAX_MSGS) {
        char *nl = (char *)memchr(p, '\n', end - p);
        if (!nl) nl = end;
        int len = (int)(nl - p);
        DevMsg *m = &dev_log.msgs[dev_log.count];
        m->timestamp[0] = 0;
        // [DEV HH:MM:SS] or [YOU HH:MM:SS] (15 or 15 chars prefix)
        if (len > 15 && p[0] == '[' &&
            (memcmp(p+1, "DEV ", 4) == 0 || memcmp(p+1, "YOU ", 4) == 0)) {
            m->from_dev = (p[1] == 'D');
            // extract timestamp (8 chars after "DEV " or "YOU ")
            memcpy(m->timestamp, p + 5, 8);
            m->timestamp[8] = 0;
            int skip = 15; // "[DEV HH:MM:SS] "
            int cpy = len - skip < MSG_LEN - 1 ? len - skip : MSG_LEN - 1;
            memcpy(m->text, p + skip, cpy);
            m->text[cpy] = 0;
            dev_log.count++;
        } else if (len > 6 && memcmp(p, "[DEV] ", 6) == 0) {
            m->from_dev = true;
            int cpy = len - 6 < MSG_LEN - 1 ? len - 6 : MSG_LEN - 1;
            memcpy(m->text, p + 6, cpy);
            m->text[cpy] = 0;
            dev_log.count++;
        } else if (len > 6 && memcmp(p, "[YOU] ", 6) == 0) {
            m->from_dev = false;
            int cpy = len - 6 < MSG_LEN - 1 ? len - 6 : MSG_LEN - 1;
            memcpy(m->text, p + 6, cpy);
            m->text[cpy] = 0;
            dev_log.count++;
        }
        p = nl + 1;
    }
    SDL_free(data);
}

static void dev_log_add(const char *text, bool from_dev) {
    char saved_input[MSG_LEN];
    memcpy(saved_input, dev_log.input, MSG_LEN);
    dev_log_load();
    memcpy(dev_log.input, saved_input, MSG_LEN);
    if (dev_log.count >= MAX_MSGS) {
        memmove(&dev_log.msgs[0], &dev_log.msgs[1], sizeof(DevMsg) * (MAX_MSGS - 1));
        dev_log.count = MAX_MSGS - 1;
    }
    DevMsg *m = &dev_log.msgs[dev_log.count++];
    strncpy(m->text, text, MSG_LEN - 1);
    m->text[MSG_LEN - 1] = 0;
    m->from_dev = from_dev;
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    snprintf(m->timestamp, sizeof(m->timestamp), "%02d:%02d:%02d",
             t->tm_hour, t->tm_min, t->tm_sec);
    dev_log_save();
}

static bool screenshot_requested = false;
static int screenshot_w = 0, screenshot_h = 0;

static void take_screenshot(int w, int h) {
    unsigned char *pixels = (unsigned char *)malloc(w * h * 4);
    if (!pixels) return;
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    // Flip vertically (GL reads bottom-up)
    int stride = w * 4;
    unsigned char *row = (unsigned char *)malloc(stride);
    for (int y = 0; y < h / 2; y++) {
        memcpy(row, pixels + y * stride, stride);
        memcpy(pixels + y * stride, pixels + (h - 1 - y) * stride, stride);
        memcpy(pixels + (h - 1 - y) * stride, row, stride);
    }
    free(row);

    // Downscale to max 800px on longest side
    int max_dim = 800;
    int out_w = w, out_h = h;
    if (w > max_dim || h > max_dim) {
        if (h > w) {
            out_h = max_dim;
            out_w = w * max_dim / h;
        } else {
            out_w = max_dim;
            out_h = h * max_dim / w;
        }
    }
    unsigned char *out = pixels;
    if (out_w != w || out_h != h) {
        out = (unsigned char *)malloc(out_w * out_h * 4);
        if (out) {
            for (int y = 0; y < out_h; y++) {
                int sy = y * h / out_h;
                for (int x = 0; x < out_w; x++) {
                    int sx = x * w / out_w;
                    memcpy(out + (y * out_w + x) * 4,
                           pixels + (sy * w + sx) * 4, 4);
                }
            }
        } else {
            out = pixels;
            out_w = w;
            out_h = h;
        }
    }

    const char *pref = SDL_GetPrefPath("com.playground", "questglory");
    if (pref) {
        char path[512];
        snprintf(path, sizeof(path), "%sscreenshot.png", pref);
        stbi_write_png(path, out_w, out_h, 4, out, out_w * 4);
        dev_log_add("[screenshot]", false);
    }
    if (out != pixels) free(out);
    free(pixels);
}

static int dev_log_last_count = 0;
static void dev_log_check_new() {
    char saved_input[MSG_LEN];
    memcpy(saved_input, dev_log.input, MSG_LEN);
    int old_count = dev_log.count;
    dev_log_load();
    memcpy(dev_log.input, saved_input, MSG_LEN);
    if (dev_log.count != old_count) {
        dev_log_last_count = dev_log.count;
    }
}

static const char *qa_save_path_buf = nullptr;

static const char *get_qa_path() {
    if (qa_save_path_buf) return qa_save_path_buf;
    const char *pref = SDL_GetPrefPath("com.playground", "questglory");
    if (!pref) return nullptr;
    static char path[512];
    snprintf(path, sizeof(path), "%sbugs.dat", pref);
    qa_save_path_buf = path;
    return path;
}

static const int QA_SAVE_VER = 2;

static void qa_save() {
    const char *path = get_qa_path();
    if (!path) return;
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f) return;
    SDL_WriteIO(f, &QA_SAVE_VER, sizeof(int));
    SDL_WriteIO(f, &qa.count, sizeof(qa.count));
    SDL_WriteIO(f, qa.bugs, sizeof(BugReport) * qa.count);
    SDL_CloseIO(f);
}

static void qa_load() {
    memset(&qa, 0, sizeof(qa));
    qa.panel_open = true;
    const char *path = get_qa_path();
    if (!path) return;
    SDL_IOStream *f = SDL_IOFromFile(path, "rb");
    if (!f) return;
    int ver = 0;
    SDL_ReadIO(f, &ver, sizeof(int));
    if (ver != QA_SAVE_VER) { SDL_CloseIO(f); return; }
    SDL_ReadIO(f, &qa.count, sizeof(qa.count));
    if (qa.count > MAX_BUGS) qa.count = MAX_BUGS;
    SDL_ReadIO(f, qa.bugs, sizeof(BugReport) * qa.count);
    SDL_CloseIO(f);
}

static const char *screen_name(Screen s) {
    const char *names[] = {"Title","Create","Town","QuestBoard","Combat","LevelUp","CharSheet","Reincarnate","Victory"};
    return names[(int)s];
}

static void qa_create_bug(Game *g) {
    if (qa.count >= MAX_BUGS) return;
    BugReport *b = &qa.bugs[qa.count++];
    memset(b, 0, sizeof(BugReport));
    strncpy(b->desc, qa.new_desc, BUG_DESC_LEN - 1);
    strncpy(b->screen_name, screen_name(g->screen), 23);
    b->gold = g->gold;
    b->party_size = g->party_size;
    b->combat_round = g->combat_round;
    b->completed = false;
    qa.new_desc[0] = 0;
    qa.creating = false;
    qa_save();
}

// ── Save / Load ──
// Format: int version, int sizeof(Game), raw Game bytes.
// Bump SAVE_VERSION when the Game layout changes. Old saves are discarded, not migrated.
#define SAVE_VERSION 2
static const char *save_path_buf = nullptr;

static const char *get_save_path() {
    if (save_path_buf) return save_path_buf;
    const char *pref = SDL_GetPrefPath("com.playground", "questglory");
    if (!pref) return nullptr;
    static char path[512];
    snprintf(path, sizeof(path), "%ssave.dat", pref);
    save_path_buf = path;
    return path;
}

static void save_game(Game *g) {
    const char *path = get_save_path();
    if (!path) return;
    SDL_IOStream *f = SDL_IOFromFile(path, "wb");
    if (!f) return;
    int ver = SAVE_VERSION;
    SDL_WriteIO(f, &ver, sizeof(ver));
    int struct_size = (int)sizeof(Game);
    SDL_WriteIO(f, &struct_size, sizeof(struct_size));
    SDL_WriteIO(f, g, sizeof(Game));
    SDL_CloseIO(f);
}

// Sanity-check a loaded Game so a stale/corrupt blob can't index tables out of range.
static bool validate_game(Game *g) {
    if (g->screen < SCR_TITLE || g->screen > SCR_VICTORY) return false;
    if (g->party_size < 0 || g->party_size > MAX_PARTY) return false;
    if (g->enemy_count < 0 || g->enemy_count > MAX_ENEMIES) return false;
    for (int i = 0; i < g->party_size; i++) {
        Character *c = &g->party[i];
        if (c->primary_class < CLASS_FIGHTER || c->primary_class > CLASS_BARBARIAN) return false;
        if (c->race < RACE_HUMAN || c->race > RACE_HALFLING) return false;
    }
    return true;
}

// Loads a save written by the current SAVE_VERSION with an identical Game layout.
// Any other version or layout is rejected and the game starts fresh: the Game struct
// embeds Character/Enemy, so a raw memcpy of an older layout scrambles every field
// after the first changed struct (this crashed the app on load once).
static bool load_game(Game *g) {
    const char *path = get_save_path();
    if (!path) return false;
    SDL_IOStream *f = SDL_IOFromFile(path, "rb");
    if (!f) return false;
    int ver = 0, struct_size = 0;
    bool ok = SDL_ReadIO(f, &ver, sizeof(ver)) == sizeof(ver)
           && SDL_ReadIO(f, &struct_size, sizeof(struct_size)) == sizeof(struct_size)
           && ver == SAVE_VERSION
           && struct_size == (int)sizeof(Game);
    if (ok) {
        Game *tmp = (Game *)malloc(sizeof(Game));
        ok = tmp && SDL_ReadIO(f, tmp, sizeof(Game)) == sizeof(Game) && validate_game(tmp);
        if (ok) memcpy(g, tmp, sizeof(Game));
        free(tmp);
    }
    SDL_CloseIO(f);
    if (!ok) SDL_Log("save.dat rejected (ver %d, size %d, expected %d/%d) — starting fresh",
                     ver, struct_size, SAVE_VERSION, (int)sizeof(Game));
    return ok;
}

// ── Stat helpers ──
static int stat_mod(int val) { return (val - 10) / 2; }

static int char_stat(Character *c, int stat) {
    int base = c->base_stats[stat] + RACE_BONUSES[c->race][stat];
    for (int i = 0; i < c->inv_count; i++)
        if (c->inventory[i].equipped && c->inventory[i].stat_bonus == stat)
            base += c->inventory[i].bonus_val;
    // past life bonuses: +1 per life to primary stat of that class
    int class_stats[] = { STR, DEX, INT, WIS, DEX, STR };
    for (int cl = 0; cl < MAX_CLASSES; cl++)
        if (c->past_lives[cl] > 0)
            if (class_stats[cl] == stat)
                base += c->past_lives[cl];
    return base;
}

static int char_ac(Character *c) {
    int ac = 10 + stat_mod(char_stat(c, DEX));
    for (int i = 0; i < c->inv_count; i++)
        if (c->inventory[i].equipped && (c->inventory[i].slot == SLOT_ARMOR || c->inventory[i].slot == SLOT_SHIELD))
            ac += c->inventory[i].bonus;
    if (c->feats[15]) ac += 1; // Dodge
    if (c->feats[22]) ac += 2; // Shield Mastery
    return ac;
}

static int char_attack(Character *c) {
    int atk = c->level;
    int main_stat = (c->primary_class == CLASS_WIZARD) ? INT :
                    (c->primary_class == CLASS_ROGUE || c->primary_class == CLASS_RANGER) ? DEX : STR;
    atk += stat_mod(char_stat(c, main_stat));
    for (int i = 0; i < c->inv_count; i++)
        if (c->inventory[i].equipped && c->inventory[i].slot == SLOT_WEAPON)
            atk += c->inventory[i].bonus;
    if (c->feats[2]) atk += 1; // Weapon Focus
    return atk;
}

static int char_damage(Character *c) {
    int dmg = 4 + stat_mod(char_stat(c, STR));
    for (int i = 0; i < c->inv_count; i++)
        if (c->inventory[i].equipped && c->inventory[i].slot == SLOT_WEAPON)
            dmg += c->inventory[i].bonus;
    if (c->feats[0]) dmg = (int)(dmg * 1.5f); // Power Attack
    return dmg < 1 ? 1 : dmg;
}

static void calc_hp(Character *c) {
    int hp_per_class[] = {10, 6, 4, 8, 8, 12};
    int total = 0;
    for (int cl = 0; cl < MAX_CLASSES; cl++)
        total += c->class_levels[cl] * hp_per_class[cl];
    total += c->level * stat_mod(char_stat(c, CON));
    if (c->feats[3]) total += c->level * 3; // Toughness
    total += c->total_past_lives * 5; // past life HP bonus
    if (total < c->level) total = c->level;
    c->max_hp = total;
    if (c->hp > c->max_hp) c->hp = c->max_hp;
}

static void calc_sp(Character *c) {
    int sp = 0;
    sp += c->class_levels[CLASS_WIZARD] * (8 + stat_mod(char_stat(c, INT)) * 2);
    sp += c->class_levels[CLASS_CLERIC] * (6 + stat_mod(char_stat(c, WIS)) * 2);
    sp += c->class_levels[CLASS_RANGER] * (3 + stat_mod(char_stat(c, WIS)));
    sp += c->class_levels[CLASS_BARBARIAN] * 4; // rage points
    if (sp < 0) sp = 0;
    c->max_sp = sp;
    if (c->sp > c->max_sp) c->sp = c->max_sp;
}

// ── Item generation ──
static void gen_item(Item *item, int level) {
    const char *weapons[] = {"Sword","Axe","Dagger","Staff","Bow","Mace","Hammer","Spear"};
    const char *armors[]  = {"Leather","Chain","Plate","Robes","Scale","Hide"};
    const char *rings[]   = {"Ruby Ring","Sapphire Ring","Emerald Ring","Topaz Ring"};
    int type = rng_range(0, 3);
    int bonus = 1 + level / 4;
    if (bonus > 5) bonus = 5;
    item->level_req = level > 1 ? level - 1 : 1;
    item->equipped = false;
    item->stat_bonus = rng_range(0, NUM_STATS-1);
    item->bonus_val = rng_range(0, 1) ? rng_range(1, bonus) : 0;

    if (type == 0) {
        item->slot = SLOT_WEAPON;
        item->bonus = bonus;
        snprintf(item->name, 32, "+%d %s", bonus, weapons[rng() % 8]);
    } else if (type == 1) {
        item->slot = SLOT_ARMOR;
        item->bonus = bonus;
        snprintf(item->name, 32, "+%d %s", bonus, armors[rng() % 6]);
    } else if (type == 2) {
        item->slot = SLOT_SHIELD;
        item->bonus = rng_range(1, bonus);
        snprintf(item->name, 32, "+%d Shield", item->bonus);
    } else {
        item->slot = SLOT_RING;
        item->bonus = 0;
        item->bonus_val = rng_range(1, bonus);
        snprintf(item->name, 32, "%s +%d %s", rings[rng()%4], item->bonus_val, STAT_NAMES[item->stat_bonus]);
    }
}

// ── Quest generation ──
static void gen_quests(Game *g) {
    const char *prefixes[] = {"The","A","Lost","Dark","Forgotten","Hidden","Cursed","Ancient"};
    const char *nouns[] = {"Crypt","Cave","Tower","Ruins","Dungeon","Temple","Lair","Tomb","Mine","Fortress"};
    const char *descs[] = {"Clear the monsters","Find the treasure","Rescue the prisoner",
                           "Defeat the boss","Explore the depths","Recover the artifact"};
    int avg_level = 1;
    for (int i = 0; i < g->party_size; i++) avg_level += g->party[i].level;
    avg_level /= (g->party_size > 0 ? g->party_size : 1);

    g->quest_count = 0;
    for (int i = 0; i < MAX_QUESTS; i++) {
        Quest *q = &g->quests[i];
        int ql = avg_level + rng_range(-2, 3);
        if (ql < 1) ql = 1;
        if (ql > 20) ql = 20;
        snprintf(q->name, 48, "%s %s", prefixes[rng()%8], nouns[rng()%10]);
        snprintf(q->desc, 128, "%s (Lv %d)", descs[rng()%6], ql);
        q->min_level = ql > 2 ? ql - 2 : 1;
        q->enemy_count = rng_range(2, 5);
        q->enemy_level = ql;
        q->xp_reward = ql * 50 * q->enemy_count;
        q->gold_reward = ql * 20 + rng_range(0, ql * 10);
        q->completed = false;
        g->quest_count++;
    }
}

// ── Enemy generation ──
static const char *DIFF_NAMES[] = {"Normal","Hard","Elite"};

static void spawn_enemies(Game *g, int count, int level) {
    const char *names[] = {"Goblin","Skeleton","Orc","Spider","Zombie","Troll",
                           "Wraith","Ogre","Kobold","Gnoll","Bandit","Cultist",
                           "Minotaur","Lich","Dragon","Demon"};
    float hp_mult[] = {1.0f, 1.5f, 2.0f};
    float dmg_mult[] = {1.0f, 1.3f, 1.6f};
    int extra_enemies[] = {0, 1, 2};
    int diff = g->difficulty < 0 ? 0 : (g->difficulty > 2 ? 2 : g->difficulty);
    count += extra_enemies[diff];
    g->enemy_count = count > MAX_ENEMIES ? MAX_ENEMIES : count;
    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        int el = level + rng_range(-1, 1);
        if (el < 1) el = 1;
        int name_idx = (el < 4) ? rng_range(0, 4) : (el < 8) ? rng_range(3, 8) :
                       (el < 14) ? rng_range(6, 12) : rng_range(10, 15);
        snprintf(e->name, 32, "%s L%d", names[name_idx], el);
        e->level = el;
        e->max_hp = (int)((el * 8 + rng_range(0, el * 4)) * hp_mult[diff]);
        e->hp = e->max_hp;
        e->ac = 10 + el / 2 + diff;
        e->attack = el + rng_range(0, 3) + diff;
        e->damage = (int)((3 + el + rng_range(0, el / 2)) * dmg_mult[diff]);
        e->alive = true;
        e->xp_reward = el * 25 * (1 + diff);
        e->gold_reward = rng_range(el, el * 5) * (1 + diff);
    }
}

// ── Combat helpers ──
static int count_alive_enemies(Game *g) {
    int n = 0;
    for (int i = 0; i < g->enemy_count; i++) if (g->enemies[i].alive) n++;
    return n;
}
static int count_alive_party(Game *g) {
    int n = 0;
    for (int i = 0; i < g->party_size; i++) if (g->party[i].alive) n++;
    return n;
}

static void auto_target(Game *g) {
    if (g->selected_target >= 0 && g->selected_target < g->enemy_count &&
        g->enemies[g->selected_target].alive) return;
    for (int i = 0; i < g->enemy_count; i++) {
        if (g->enemies[i].alive) { g->selected_target = i; return; }
    }
}

static void do_single_attack(Game *g, Character *c, int target) {
    Enemy *e = &g->enemies[target];
    if (!e->alive) return;
    int roll = d20();
    int atk = char_attack(c);
    bool crit = (roll == 20) || (c->feats[11] && roll >= 19);
    if (roll + atk >= e->ac || crit) {
        int dmg = rng_range(1, char_damage(c));
        if (crit) dmg *= 2;
        if (c->class_levels[CLASS_ROGUE] > 0) {
            int sneak = c->class_levels[CLASS_ROGUE] / 2 + 1;
            if (c->feats[5]) sneak += 1;
            dmg += rng_range(sneak, sneak * 3);
        }
        e->hp -= dmg;
        e->shake_timer = 0.3f;
        if (e->hp <= 0) { e->hp = 0; e->alive = false; }
        log_combat(&g->combat_log, "%s %s %s for %d%s%s", c->name,
            crit ? "CRITS" : "hits", e->name, dmg,
            crit ? "!" : "", e->alive ? "" : " [DEAD]");
        sfx_play(crit ? SFX_CRIT : SFX_HIT);
        if (!e->alive) { sfx_play(SFX_DEATH, 0.3f); auto_target(g); }
    } else {
        log_combat(&g->combat_log, "%s misses %s (%d+%d vs AC %d)", c->name, e->name, roll, atk, e->ac);
        sfx_play(SFX_MISS);
    }
}

static void do_player_attack(Game *g, Character *c, int target) {
    Enemy *e = &g->enemies[target];
    bool was_alive = e->alive;
    do_single_attack(g, c, target);
    if (was_alive && !e->alive && c->feats[1]) {
        // Cleave: bonus attack on another alive enemy
        bool great_cleave = c->feats[24];
        do {
            int next = -1;
            for (int i = 0; i < g->enemy_count; i++)
                if (g->enemies[i].alive) { next = i; break; }
            if (next < 0) break;
            log_combat(&g->combat_log, "%s cleaves!", c->name);
            bool alive_before = g->enemies[next].alive;
            do_single_attack(g, c, next);
            if (!great_cleave || alive_before == g->enemies[next].alive) break;
            if (g->enemies[next].alive) break;
        } while (great_cleave);
    }
}

static void do_ability(Game *g, Character *c, int target) {
    if (c->class_levels[CLASS_WIZARD] > 0 && c->sp >= 5) {
        c->sp -= 5;
        int dmg = rng_range(c->class_levels[CLASS_WIZARD]*2, c->class_levels[CLASS_WIZARD]*6);
        if (c->feats[7]) dmg = (int)(dmg * 1.5f);
        if (c->feats[8]) dmg = c->class_levels[CLASS_WIZARD] * 6;
        // hit all enemies
        for (int i = 0; i < g->enemy_count; i++) {
            if (!g->enemies[i].alive) continue;
            g->enemies[i].hp -= dmg;
            g->enemies[i].shake_timer = 0.3f;
            if (g->enemies[i].hp <= 0) { g->enemies[i].hp = 0; g->enemies[i].alive = false; }
        }
        log_combat(&g->combat_log, "%s casts Fireball for %d to all!", c->name, dmg);
        sfx_play(SFX_FIREBALL);
        auto_target(g);
    } else if (c->class_levels[CLASS_CLERIC] > 0 && c->sp >= 4) {
        c->sp -= 4;
        int heal = rng_range(c->class_levels[CLASS_CLERIC]*2, c->class_levels[CLASS_CLERIC]*5);
        // heal lowest HP party member
        int lowest = -1; float lowest_pct = 2.0f;
        for (int i = 0; i < g->party_size; i++) {
            if (!g->party[i].alive) continue;
            float pct = (float)g->party[i].combat_hp / g->party[i].max_hp;
            if (pct < lowest_pct) { lowest_pct = pct; lowest = i; }
        }
        if (lowest >= 0) {
            g->party[lowest].combat_hp += heal;
            if (g->party[lowest].combat_hp > g->party[lowest].max_hp)
                g->party[lowest].combat_hp = g->party[lowest].max_hp;
            log_combat(&g->combat_log, "%s heals %s for %d", c->name, g->party[lowest].name, heal);
            sfx_play(SFX_HEAL);
        }
    } else if (c->class_levels[CLASS_BARBARIAN] > 0 && c->sp >= 3) {
        c->sp -= 3;
        Enemy *e = &g->enemies[target];
        if (!e->alive) return;
        int dmg = rng_range(char_damage(c), char_damage(c) * 2) + 5;
        if (c->feats[17]) dmg += 4;
        e->hp -= dmg;
        e->shake_timer = 0.3f;
        if (e->hp <= 0) { e->hp = 0; e->alive = false; }
        log_combat(&g->combat_log, "%s RAGES at %s for %d!%s", c->name, e->name, dmg, e->alive ? "" : " [DEAD]");
        sfx_play(SFX_RAGE);
        if (!e->alive) auto_target(g);
    } else if (c->class_levels[CLASS_RANGER] > 0) {
        // double shot
        do_player_attack(g, c, target);
        if (c->feats[19]) do_player_attack(g, c, target); // Rapid Shot
        return;
    } else {
        do_player_attack(g, c, target);
    }
}

static void do_enemy_turn(Game *g, Enemy *e) {
    if (!e->alive) return;
    // pick random alive party member
    int alive[MAX_PARTY], n = 0;
    for (int i = 0; i < g->party_size; i++)
        if (g->party[i].alive) alive[n++] = i;
    if (n == 0) return;
    int ti = alive[rng() % n];
    Character *c = &g->party[ti];

    int roll = d20();
    int ac = char_ac(c);
    if (c->defending) ac += 4;
    if (roll + e->attack >= ac) {
        int dmg = rng_range(1, e->damage);
        if (c->defending) dmg /= 2;
        if (c->feats[6] && c->class_levels[CLASS_ROGUE] > 0) {
            // Evasion: 50% chance to dodge
            if (rng() % 2 == 0) {
                log_combat(&g->combat_log, "%s evades %s's attack!", c->name, e->name);
                return;
            }
        }
        c->combat_hp -= dmg;
        c->shake_timer = 0.3f;
        if (c->combat_hp <= 0) { c->combat_hp = 0; c->alive = false; }
        log_combat(&g->combat_log, "%s hits %s for %d%s", e->name, c->name, dmg, c->alive ? "" : " [DOWN]");
        sfx_play(SFX_ENEMY_HIT);
        if (!c->alive) sfx_play(SFX_DEATH, 0.3f);
    } else {
        log_combat(&g->combat_log, "%s misses %s", e->name, c->name);
    }
}

// ── Initiative order (D&D 3.5e style) ──
static void roll_initiative(Game *g) {
    g->init_count = 0;
    // Roll for party
    for (int i = 0; i < g->party_size; i++) {
        if (!g->party[i].alive) continue;
        g->party[i].initiative = d20() + stat_mod(char_stat(&g->party[i], DEX));
        g->init_order[g->init_count++] = i; // positive = party
    }
    // Roll for enemies
    for (int i = 0; i < g->enemy_count; i++) {
        if (!g->enemies[i].alive) continue;
        // Enemy DEX mod approximated from level
        g->enemies[i].initiative = d20() + (g->enemies[i].level / 3);
        g->init_order[g->init_count++] = -(i + 1); // negative = enemy
    }
    // Sort descending by initiative (bubble sort, small N)
    for (int i = 0; i < g->init_count - 1; i++) {
        for (int j = i + 1; j < g->init_count; j++) {
            int init_i, init_j;
            if (g->init_order[i] >= 0) init_i = g->party[g->init_order[i]].initiative;
            else init_i = g->enemies[-(g->init_order[i] + 1)].initiative;
            if (g->init_order[j] >= 0) init_j = g->party[g->init_order[j]].initiative;
            else init_j = g->enemies[-(g->init_order[j] + 1)].initiative;
            if (init_j > init_i) {
                int tmp = g->init_order[i];
                g->init_order[i] = g->init_order[j];
                g->init_order[j] = tmp;
            }
        }
    }
    g->init_current = 0;
}

static void advance_initiative(Game *g) {
    // Safety: max iterations to prevent infinite loops
    for (int safety = 0; safety < 64; safety++) {
        g->init_current++;
        // Skip dead combatants
        while (g->init_current < g->init_count) {
            int idx = g->init_order[g->init_current];
            if (idx >= 0 && !g->party[idx].alive) { g->init_current++; continue; }
            if (idx < 0 && !g->enemies[-(idx+1)].alive) { g->init_current++; continue; }
            break;
        }
        // Round over — new round
        if (g->init_current >= g->init_count) {
            // Check if combat should end (no alive on one side)
            if (count_alive_enemies(g) == 0 || count_alive_party(g) == 0) {
                g->player_turn = false;
                return;
            }
            g->combat_round++;
            for (int i = 0; i < g->party_size; i++) g->party[i].defending = false;
            roll_initiative(g);
            if (g->init_count == 0) return; // no combatants
            log_combat(&g->combat_log, "--- Round %d ---", g->combat_round);
        }
        // Check current turn
        int cur = g->init_order[g->init_current];
        if (cur < 0) {
            // Enemy turn — auto-execute and continue loop
            do_enemy_turn(g, &g->enemies[-(cur+1)]);
            // Check if combat ended from this enemy turn
            if (count_alive_party(g) == 0) { g->player_turn = false; return; }
            continue;
        }
        // Player turn — set for UI and return
        g->turn_index = cur;
        g->player_turn = true;
        return;
    }
}

// ── Init character ──
static void init_character(Character *c, const char *name, int race, int cls, const int *stats) {
    memset(c, 0, sizeof(*c));
    snprintf(c->name, 24, "%s", name);
    c->race = race;
    c->primary_class = cls;
    c->class_levels[cls] = 1;
    c->level = 1;
    c->xp = 0;
    c->xp_next = 100;
    memcpy(c->base_stats, stats, NUM_STATS * sizeof(int));
    c->alive = true;
    if (race == RACE_HUMAN) c->feats[2] = true; // bonus feat: Weapon Focus
    calc_hp(c);
    c->hp = c->max_hp;
    calc_sp(c);
    c->sp = c->max_sp;

    // starter weapon
    Item *w = &c->inventory[0];
    c->inv_count = 1;
    w->slot = SLOT_WEAPON;
    w->bonus = 0;
    w->stat_bonus = -1;
    w->bonus_val = 0;
    w->level_req = 1;
    w->equipped = true;
    const char *starter[] = {"Short Sword","Dagger","Quarterstaff","Mace","Longbow","Greataxe"};
    snprintf(w->name, 32, "%s", starter[cls]);
}

// ── Level up ──
static bool can_level(Character *c) {
    return c->level < MAX_LEVEL && c->xp >= c->xp_next;
}

static void apply_levelup(Character *c, int cls, int feat) {
    c->xp -= c->xp_next;
    c->level++;
    c->class_levels[cls]++;
    c->xp_next = c->level * c->level * 50;
    if (feat >= 0 && feat < NUM_FEATS) c->feats[feat] = true;
    calc_hp(c);
    c->hp = c->max_hp;
    calc_sp(c);
    c->sp = c->max_sp;
}

static void start_next_levelup(Game *g, int after = -1) {
    for (int i = after + 1; i < g->party_size; i++) {
        if (can_level(&g->party[i])) {
            g->levelup_who = i;
            g->levelup_class_choice = g->party[i].primary_class;
            g->levelup_feat_choice = -1;
            g->levelup_needs_feat = (g->party[i].level % 3 == 0);
            g->screen = SCR_LEVELUP;
            return;
        }
    }
    g->screen = SCR_TOWN;
}

// ── Reincarnation ──
static void reincarnate(Character *c) {
    int pl_class = c->primary_class;
    int past[MAX_CLASSES];
    memcpy(past, c->past_lives, sizeof(past));
    int total_pl = c->total_past_lives + 1;
    if (past[pl_class] < MAX_PAST_LIVES) past[pl_class]++;

    bool saved_feats[NUM_FEATS];
    memcpy(saved_feats, c->feats, sizeof(saved_feats));

    char name[24];
    snprintf(name, 24, "%s", c->name);
    int race = c->race;
    int stats[NUM_STATS];
    memcpy(stats, c->base_stats, sizeof(stats));
    // +1 to a build point on reincarnation
    stats[rng_range(0, NUM_STATS-1)] += 1;

    init_character(c, name, race, pl_class, stats);
    memcpy(c->past_lives, past, sizeof(past));
    c->total_past_lives = total_pl;
}

// ── UI style ──
static void setup_touch_style(float dpi_scale) {
    ImGuiStyle &s = ImGui::GetStyle();
    float s2 = dpi_scale * 0.5f;
    s.FramePadding    = ImVec2(8 * s2, 6 * s2);
    s.ItemSpacing     = ImVec2(6 * s2, 4 * s2);
    s.ItemInnerSpacing = ImVec2(4 * s2, 4 * s2);
    s.TouchExtraPadding = ImVec2(4 * s2, 4 * s2);
    s.ScrollbarSize   = 12 * s2;
    s.GrabMinSize     = 10 * s2;
    s.WindowPadding   = ImVec2(8 * s2, 8 * s2);
    s.WindowRounding  = 4 * s2;
    s.FrameRounding   = 3 * s2;
    s.GrabRounding    = 2 * s2;
    s.ScrollbarRounding = 3 * s2;
    s.TabRounding     = 3 * s2;
    s.WindowBorderSize = 0;
    s.FrameBorderSize  = 0;

    ImVec4 *c = s.Colors;
    c[ImGuiCol_WindowBg]        = ImVec4(0.10f, 0.10f, 0.12f, 0.95f);
    c[ImGuiCol_FrameBg]         = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.35f, 0.35f, 0.42f, 1.00f);
    c[ImGuiCol_Button]          = ImVec4(0.24f, 0.42f, 0.65f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.30f, 0.50f, 0.75f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.18f, 0.36f, 0.58f, 1.00f);
    c[ImGuiCol_Header]          = ImVec4(0.24f, 0.42f, 0.65f, 0.80f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.30f, 0.50f, 0.75f, 0.80f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.18f, 0.36f, 0.58f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.45f, 0.70f, 1.00f, 1.00f);
    c[ImGuiCol_TitleBg]         = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.14f, 0.14f, 0.18f, 1.00f);
}

// ── HP bar helper ──
static void draw_hp_bar(float hp, float max_hp, float width, float height) {
    float pct = max_hp > 0 ? hp / max_hp : 0;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p, ImVec2(p.x+width, p.y+height), IM_COL32(40,40,40,255), 4);
    ImU32 col = pct > 0.5f ? IM_COL32(40,180,40,255) : pct > 0.25f ? IM_COL32(200,180,40,255) : IM_COL32(200,40,40,255);
    dl->AddRectFilled(p, ImVec2(p.x+width*pct, p.y+height), col, 4);
    char buf[32];
    snprintf(buf, 32, "%d/%d", (int)hp, (int)max_hp);
    ImVec2 ts = ImGui::CalcTextSize(buf);
    dl->AddText(ImVec2(p.x+(width-ts.x)*0.5f, p.y+(height-ts.y)*0.5f), IM_COL32(255,255,255,255), buf);
    ImGui::Dummy(ImVec2(width, height));
}


#include "game_api.h"

struct GameState {
    Game game;
    float dpi_scale;
    bool quit_requested;
};

static void *game_create(float dpi_scale) {
    sfx_init();
    rng_state = (uint64_t)time(NULL);
    setup_touch_style(dpi_scale);
    GameState *gs = new GameState();
    memset(&gs->game, 0, sizeof(Game));
    gs->game.screen = SCR_TITLE;
    gs->game.gold = 50;
    gs->game.create_points = 25;
    gs->game.create_class = CLASS_FIGHTER;
    for (int i = 0; i < NUM_STATS; i++) gs->game.create_stats[i] = 10;
    snprintf(gs->game.create_name, 24, "Hero");
    gs->dpi_scale = dpi_scale;
    gs->quit_requested = false;
    qa_load();
    dev_log_load();
    if (load_game(&gs->game)) {
        if (gs->game.screen == SCR_COMBAT) gs->game.screen = SCR_TOWN;
    }
    return gs;
}

static void game_destroy(void *state) {
    GameState *gs = (GameState *)state;
    save_game(&gs->game);
    if (sfx_stream) { SDL_DestroyAudioStream(sfx_stream); sfx_stream = nullptr; }
    delete gs;
}

static void game_on_save_event(void *state) {
    GameState *gs = (GameState *)state;
    save_game(&gs->game);
}

static int game_wants_quit(void *state) {
    GameState *gs = (GameState *)state;
    return gs->quit_requested ? 1 : 0;
}

static size_t game_serialize(void *state, void *buf, size_t buf_size) {
    GameState *gs = (GameState *)state;
    size_t needed = sizeof(Game) + sizeof(float) + sizeof(uint64_t);
    if (!buf || buf_size < needed) return needed;
    char *p = (char *)buf;
    memcpy(p, &gs->game, sizeof(Game)); p += sizeof(Game);
    memcpy(p, &gs->dpi_scale, sizeof(float)); p += sizeof(float);
    memcpy(p, &rng_state, sizeof(uint64_t));
    return needed;
}

static void game_deserialize(void *state, const void *buf, size_t size) {
    GameState *gs = (GameState *)state;
    const char *p = (const char *)buf;
    if (size >= sizeof(Game)) { memcpy(&gs->game, p, sizeof(Game)); p += sizeof(Game); }
    if (size >= sizeof(Game) + sizeof(float)) { memcpy(&gs->dpi_scale, p, sizeof(float)); p += sizeof(float); }
    if (size >= sizeof(Game) + sizeof(float) + sizeof(uint64_t)) { memcpy(&rng_state, p, sizeof(uint64_t)); }
    setup_touch_style(gs->dpi_scale);
}

static void game_tick(void *state, int w, int h, float dpi_scale) {
    GameState *gs = (GameState *)state;
    Game &game = gs->game;
    gs->dpi_scale = dpi_scale;
    setup_touch_style(dpi_scale);

        // Handle screenshot from previous frame
        if (screenshot_requested) {
            take_screenshot(w, h);
            screenshot_requested = false;
        }

        float margin = w * 0.02f;
        float full_w = w - margin * 2;

        // ── QA Debug Panel (top 1/3) ──
        float qa_h = h * 0.30f;
        float game_h = h - qa_h - margin * 2;
        ImGuiWindowFlags wf_qa = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                                 ImGuiWindowFlags_NoCollapse;
        ImGui::SetNextWindowPos(ImVec2(margin, margin));
        ImGui::SetNextWindowSize(ImVec2(full_w, qa_h));
        ImGui::Begin("QA Tools", &qa.panel_open, wf_qa);
        {
            float qa_bw = full_w - 30*dpi_scale;
            ImGui::Text("Screen: %s | Gold: %d | Party: %d", screen_name(game.screen), game.gold, game.party_size);

            if (ImGui::BeginTabBar("##qa_tabs")) {
                // ── Bugs Tab ──
                int open_bugs = 0;
                for (int i = 0; i < qa.count; i++) if (!qa.bugs[i].completed) open_bugs++;
                char bug_tab[32];
                snprintf(bug_tab, sizeof(bug_tab), "Bugs (%d)###bugs", open_bugs);
                if (ImGui::BeginTabItem(bug_tab)) {
                    if (!qa.creating) {
                        if (ImGui::Button("Report Bug", ImVec2(qa_bw * 0.45f, 0))) {
                            qa.creating = true;
                            qa.new_desc[0] = 0;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Done", ImVec2(qa_bw * 0.45f, 0))) {
                            int write = 0;
                            for (int i = 0; i < qa.count; i++)
                                if (!qa.bugs[i].completed) qa.bugs[write++] = qa.bugs[i];
                            qa.count = write;
                            qa_save();
                        }
                    } else {
                        ImGui::InputTextMultiline("##bugdesc", qa.new_desc, BUG_DESC_LEN, ImVec2(qa_bw * 0.95f, 60*dpi_scale));
                        if (ImGui::Button("Submit", ImVec2(qa_bw * 0.3f, 0)) && qa.new_desc[0]) {
                            qa_create_bug(&game);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel##qa", ImVec2(qa_bw * 0.3f, 0))) qa.creating = false;
                    }
                    ImGui::Separator();
                    ImGui::BeginChild("##buglist", ImVec2(0, 0));
                    for (int i = qa.count - 1; i >= 0; i--) {
                        BugReport *b = &qa.bugs[i];
                        ImGui::PushID(800 + i);
                        bool done = b->completed;
                        if (done) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.5f,0.5f,1));
                        if (ImGui::Checkbox("##done", &b->completed)) qa_save();
                        ImGui::SameLine();
                        ImGui::TextWrapped("[%s] %s", b->screen_name, b->desc);
                        if (done) ImGui::PopStyleColor();
                        ImGui::PopID();
                    }
                    ImGui::EndChild();
                    ImGui::EndTabItem();
                }

                // ── Messages Tab ──
                char msg_tab[32];
                snprintf(msg_tab, sizeof(msg_tab), "Messages (%d)###msgs", dev_log.count);
                if (ImGui::BeginTabItem(msg_tab)) {
                    // Check for new messages from dev every ~2 seconds
                    static int msg_check = 0;
                    if (++msg_check >= 30) {
                        msg_check = 0;
                        dev_log_check_new();
                    }

                    ImGui::BeginChild("##msglist", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4*dpi_scale));

                    // Touch drag-to-scroll: track drag start inside child, keep scrolling even outside
                    static bool msglist_dragging = false;
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
                        msglist_dragging = true;
                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                        msglist_dragging = false;
                    if (msglist_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                        ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y);
                    }

                    float msg_scale = 0.75f;
                    ImGui::SetWindowFontScale(msg_scale);
                    float orig_spacing = ImGui::GetStyle().ItemSpacing.y;
                    ImGui::GetStyle().ItemSpacing.y = 1.0f * dpi_scale;

                    for (int i = 0; i < dev_log.count; i++) {
                        DevMsg *m = &dev_log.msgs[i];
                        const char *ts = m->timestamp[0] ? m->timestamp : "";
                        if (m->from_dev) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
                            if (ts[0])
                                ImGui::TextWrapped("[%s] DEV: %s", ts, m->text);
                            else
                                ImGui::TextWrapped("DEV: %s", m->text);
                            ImGui::PopStyleColor();
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 1.0f, 0.5f, 1.0f));
                            if (ts[0])
                                ImGui::TextWrapped("[%s] YOU: %s", ts, m->text);
                            else
                                ImGui::TextWrapped("YOU: %s", m->text);
                            ImGui::PopStyleColor();
                        }
                    }

                    ImGui::SetWindowFontScale(1.0f);
                    ImGui::GetStyle().ItemSpacing.y = orig_spacing;
                    // Auto-scroll only when new messages arrive AND user is near bottom
                    static int prev_msg_count = 0;
                    bool at_bottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10;
                    if (dev_log.count > prev_msg_count && at_bottom) {
                        ImGui::SetScrollHereY(1.0f);
                    }
                    prev_msg_count = dev_log.count;
                    ImGui::EndChild();

                    // Input area
                    float btn_w = qa_bw * 0.15f;
                    ImGui::PushItemWidth(qa_bw - btn_w * 2 - 16*dpi_scale);
                    bool enter = ImGui::InputText("##msginput", dev_log.input, MSG_LEN,
                        ImGuiInputTextFlags_EnterReturnsTrue);
                    ImGui::PopItemWidth();
                    ImGui::SameLine();
                    if ((ImGui::Button("Send", ImVec2(btn_w, 0)) || enter) && dev_log.input[0]) {
                        dev_log_add(dev_log.input, false);
                        dev_log.input[0] = 0;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Snap", ImVec2(btn_w, 0))) {
                        if (dev_log.input[0]) {
                            dev_log_add(dev_log.input, false);
                            dev_log.input[0] = 0;
                        }
                        screenshot_requested = true;
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();

        // ── Game Window (bottom 2/3) ──
        float bw = full_w;
        float bh = game_h;
        ImGui::SetNextWindowPos(ImVec2(margin, qa_h + margin * 1.5f));
        ImGui::SetNextWindowSize(ImVec2(bw, bh));
        ImGuiWindowFlags wf = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar;

        // auto-save when arriving at town or victory
        static Screen last_screen = SCR_TITLE;
        if (game.screen != last_screen) {
            if (game.screen == SCR_TOWN || game.screen == SCR_VICTORY)
                save_game(&game);
            last_screen = game.screen;
        }

        // set music based on current screen
        switch (game.screen) {
        case SCR_TITLE: case SCR_CREATE: mus_set_scene(MUS_TITLE); break;
        case SCR_TOWN: case SCR_QUEST_BOARD: case SCR_CHARACTER:
        case SCR_LEVELUP: case SCR_REINCARNATE: mus_set_scene(MUS_TOWN); break;
        case SCR_COMBAT: mus_set_scene(MUS_COMBAT); break;
        case SCR_VICTORY: mus_set_scene(MUS_VICTORY); break;
        default: mus_set_scene(MUS_NONE); break;
        }

        // ── TITLE ──
        if (game.screen == SCR_TITLE) {
            ImGui::Begin("##title", nullptr, wf);
            ImGui::SetCursorPosY(bh * 0.15f);
            ImGui::SetCursorPosX(bw * 0.25f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.75f, 0.3f, 1));
            ImGui::Text("QUEST & GLORY");
            ImGui::PopStyleColor();
            ImGui::SetCursorPosX(bw * 0.1f);
            ImGui::TextWrapped("A turn-based RPG of multiclassing, loot, and reincarnation");
            // buttons pinned to bottom
            float title_btn_h = ImGui::GetFrameHeightWithSpacing() * 2 + 40*dpi_scale;
            ImGui::SetCursorPosY(bh - title_btn_h);
            ImGui::SetCursorPosX(bw * 0.25f);
            if (ImGui::Button("New Game", ImVec2(bw*0.5f, 0))) {
                memset(&game, 0, sizeof(game));
                game.screen = SCR_CREATE;
                game.gold = 50;
                game.create_points = 25;
                game.create_class = CLASS_FIGHTER;
                for (int i = 0; i < NUM_STATS; i++) game.create_stats[i] = 10;
                snprintf(game.create_name, 24, "Hero");
            }
            ImGui::SetCursorPosX(bw * 0.25f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f,0.5f,0.3f,1));
            if (ImGui::Button("Quick Party", ImVec2(bw*0.5f, 0))) {
                game.party_size = 0;
                // Fighter: STR focus
                int fs[] = {16,12,10,10,14,10};
                init_character(&game.party[0], "Bron", RACE_DWARF, CLASS_FIGHTER, fs);
                // Cleric: WIS focus
                int cs[] = {14,10,10,16,12,10};
                init_character(&game.party[1], "Lyra", RACE_HUMAN, CLASS_CLERIC, cs);
                // Wizard: INT focus
                int ws[] = {8,14,16,12,10,12};
                init_character(&game.party[2], "Zeph", RACE_ELF, CLASS_WIZARD, ws);
                // Rogue: DEX focus
                int rs[] = {10,16,12,10,10,14};
                init_character(&game.party[3], "Pip", RACE_HALFLING, CLASS_ROGUE, rs);
                game.party_size = 4;
                gen_quests(&game);
                game.screen = SCR_TOWN;
            }
            ImGui::PopStyleColor();
            ImGui::End();
        }

        // ── CHARACTER CREATION ──
        else if (game.screen == SCR_CREATE) {
            ImGui::Begin("##create", nullptr, wf);
            ImGui::BeginChild("##create_scroll", ImVec2(0, 0));

            // Touch drag-to-scroll for create panel
            static bool create_dragging = false;
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered())
                create_dragging = true;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                create_dragging = false;
            if (create_dragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
                ImGui::SetScrollY(ImGui::GetScrollY() - ImGui::GetIO().MouseDelta.y);

            ImGui::Text("Create Hero %d of %d", game.party_size + 1, MAX_PARTY);
            ImGui::Separator();

            // templates
            struct Template { const char *label; int race; int cls; int stats[6]; };
            Template templates[] = {
                {"Tank",   RACE_DWARF,    CLASS_FIGHTER,  {16,12,10,10,14,10}},
                {"Healer", RACE_HUMAN,    CLASS_CLERIC,   {14,10,10,16,12,10}},
                {"Mage",   RACE_ELF,      CLASS_WIZARD,   {8,14,16,12,10,12}},
                {"Rogue",  RACE_HALFLING,  CLASS_ROGUE,    {10,16,12,10,10,14}},
                {"Archer", RACE_ELF,      CLASS_RANGER,   {12,16,10,12,12,10}},
                {"Brute",  RACE_HUMAN,    CLASS_BARBARIAN,{16,10,10,10,12,14}},
            };
            float tmpl_w = bw * 0.30f;
            for (int i = 0; i < 6; i++) {
                if (i % 3 != 0) ImGui::SameLine();
                ImGui::PushID(700+i);
                if (ImGui::Button(templates[i].label, ImVec2(tmpl_w, 0))) {
                    game.create_race = templates[i].race;
                    game.create_class = templates[i].cls;
                    game.create_points = 0;
                    for (int s = 0; s < 6; s++) game.create_stats[s] = templates[i].stats[s];
                    // auto-calculate remaining points
                    int used = 0;
                    for (int s = 0; s < 6; s++) used += game.create_stats[s] - 10;
                    game.create_points = 25 - used;
                }
                ImGui::PopID();
            }
            ImGui::Separator();

            ImGui::Text("Name:");
            ImGui::SameLine();
            ImGui::PushItemWidth(bw * 0.5f);
            ImGui::InputText("##name", game.create_name, 24);
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("\xF0\x9F\x8E\xB2", ImVec2(0, 0))) { // 🎲
                // German/Frieren-style syllable generator
                static const char *starts[] = {
                    "Fr", "H", "St", "Fl", "Gr", "Kr", "Br", "Sch",
                    "W", "Z", "L", "R", "D", "N", "S", "E", "A",
                    "Str", "Gl", "Bl", "Tr", "Kl", "Pf", "Sp",
                };
                static const char *mids[] = {
                    "ie", "ei", "au", "im", "ar", "en", "il", "er",
                    "al", "an", "ol", "ul", "iel", "ier", "eis", "ach",
                    "ind", "ell", "orn", "ung", "alt", "ern", "ieg",
                };
                static const char *ends[] = {
                    "en", "el", "er", "n", "t", "ke", "ne", "se",
                    "ren", "den", "cht", "nd", "rt", "gen", "zel",
                    "ler", "ner", "mer", "fen", "ben", "ten", "de",
                };
                int ns = sizeof(starts)/sizeof(starts[0]);
                int nm = sizeof(mids)/sizeof(mids[0]);
                int ne = sizeof(ends)/sizeof(ends[0]);
                char buf[24];
                snprintf(buf, sizeof(buf), "%s%s%s",
                    starts[rand()%ns], mids[rand()%nm], ends[rand()%ne]);
                if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] -= 32;
                snprintf(game.create_name, 24, "%s", buf);
            }

            ImGui::Text("Race:");
            float race_w = bw * 0.44f;
            for (int i = 0; i < 4; i++) {
                if (i % 2 != 0) ImGui::SameLine();
                bool sel = game.create_race == i;
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f,0.6f,0.3f,1));
                if (ImGui::Button(RACE_NAMES[i], ImVec2(race_w, 0))) game.create_race = i;
                if (sel) ImGui::PopStyleColor();
            }
            ImGui::Text("  Bonuses: ");
            ImGui::SameLine();
            for (int s = 0; s < NUM_STATS; s++) {
                int b = RACE_BONUSES[game.create_race][s];
                if (b != 0) { ImGui::SameLine(); ImGui::Text("%s%+d ", STAT_NAMES[s], b); }
            }
            if (game.create_race == RACE_HUMAN) { ImGui::SameLine(); ImGui::Text("(Bonus Feat)"); }

            ImGui::Spacing();
            ImGui::Text("Class:");
            float cls_w = bw * 0.44f;
            for (int i = 0; i < MAX_CLASSES; i++) {
                if (i % 2 != 0) ImGui::SameLine();
                bool sel = game.create_class == i;
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(CLASS_COLORS[i].x*0.7f, CLASS_COLORS[i].y*0.7f, CLASS_COLORS[i].z*0.7f, 1));
                if (ImGui::Button(CLASS_NAMES[i], ImVec2(cls_w, 0))) game.create_class = i;
                if (sel) ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Text("Stats (Points: %d)", game.create_points);
            float stat_label_w = 90 * dpi_scale;
            float stat_btn = 36 * dpi_scale;
            for (int s = 0; s < NUM_STATS; s++) {
                ImGui::PushID(s);
                char stat_buf[16];
                snprintf(stat_buf, sizeof(stat_buf), "%s: %2d", STAT_NAMES[s],
                    game.create_stats[s] + RACE_BONUSES[game.create_race][s]);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX());
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(stat_buf);
                ImGui::SameLine(stat_label_w);
                if (ImGui::Button("-", ImVec2(stat_btn, 0)) && game.create_stats[s] > 8) { game.create_stats[s]--; game.create_points++; }
                ImGui::SameLine();
                if (ImGui::Button("+", ImVec2(stat_btn, 0)) && game.create_points > 0 && game.create_stats[s] < 18) { game.create_stats[s]++; game.create_points--; }
                ImGui::PopID();
            }

            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::Button("Add to Party", ImVec2(bw*0.45f, 0))) {
                init_character(&game.party[game.party_size], game.create_name,
                              game.create_race, game.create_class, game.create_stats);
                game.party_size++;
                game.create_points = 25;
                for (int i = 0; i < NUM_STATS; i++) game.create_stats[i] = 10;
                snprintf(game.create_name, 24, "Hero %d", game.party_size + 1);
                if (game.party_size >= MAX_PARTY) {
                    gen_quests(&game);
                    game.screen = SCR_TOWN;
                }
            }
            if (game.party_size >= 1) {
                ImGui::SameLine();
                if (ImGui::Button("Done", ImVec2(bw*0.45f, 0))) {
                    gen_quests(&game);
                    game.screen = SCR_TOWN;
                }
            }
            ImGui::EndChild();
            ImGui::End();
        }

        // ── TOWN ──
        else if (game.screen == SCR_TOWN) {
            ImGui::Begin("##town", nullptr, wf);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.75f, 0.3f, 1));
            ImGui::Text("TOWN");
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::Text("    Gold: %d", game.gold);
            ImGui::Separator();

            // party summary — 2-line compact layout
            float bar_w = bw * 0.45f;
            float bar_h = 14 * dpi_scale;
            float indent = 12 * dpi_scale;
            for (int i = 0; i < game.party_size; i++) {
                Character *c = &game.party[i];
                ImGui::PushID(i);
                // Line 1: Name + level-up/reincarnate status
                ImGui::PushStyleColor(ImGuiCol_Text, CLASS_COLORS[c->primary_class]);
                ImGui::Text("%s", c->name);
                ImGui::PopStyleColor();
                if (can_level(c)) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1,0.85f,0.2f,1), "[LEVEL UP]");
                }
                if (c->level >= MAX_LEVEL) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.3f,0.7f,1));
                    if (ImGui::Button("REINCARNATE", ImVec2(0, 0))) {
                        game.levelup_who = i;
                        game.screen = SCR_REINCARNATE;
                    }
                    ImGui::PopStyleColor();
                }
                if (c->total_past_lives > 0) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1, 0.85f, 0.2f, 1), "[%dPL]", c->total_past_lives);
                }
                // Line 2: Class + HP bar (indented, smaller text)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indent);
                ImGui::SetWindowFontScale(0.8f);
                ImGui::TextColored(ImVec4(0.7f,0.7f,0.7f,1), "L%d %s", c->level, CLASS_NAMES[c->primary_class]);
                ImGui::SameLine();
                draw_hp_bar((float)c->hp, (float)c->max_hp, bar_w, bar_h);
                ImGui::SetWindowFontScale(1.0f);
                if (i < game.party_size - 1) ImGui::Spacing();
                ImGui::PopID();
            }

            // check if anyone can level
            bool any_can_level = false;
            for (int i = 0; i < game.party_size; i++)
                if (can_level(&game.party[i])) { any_can_level = true; break; }

            // pin buttons at bottom
            int btn_rows = any_can_level ? 5 : 4;
            float town_btn_h = ImGui::GetFrameHeightWithSpacing() * btn_rows + 40*dpi_scale;
            ImGui::SetCursorPosY(bh - town_btn_h);
            ImGui::Separator();

            float btn_w = bw * 0.45f;

            for (int d = 0; d < 3; d++) {
                if (d > 0) ImGui::SameLine();
                bool selected = (game.difficulty == d);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.2f, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.65f, 0.25f, 1));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.15f, 1));
                }
                if (ImGui::Button(DIFF_NAMES[d], ImVec2(bw * 0.30f, 0))) game.difficulty = d;
                if (selected) ImGui::PopStyleColor(3);
            }
            if (any_can_level) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f,0.5f,0.1f,1));
                if (ImGui::Button("Level Up All", ImVec2(bw*0.92f, 0)))
                    start_next_levelup(&game);
                ImGui::PopStyleColor();
            }

            if (ImGui::Button("Random Quest", ImVec2(btn_w, 0))) {
                int avail[32]; int navail = 0;
                for (int i = 0; i < game.quest_count && navail < 32; i++)
                    if (!game.quests[i].completed) avail[navail++] = i;
                if (navail == 0) {
                    gen_quests(&game);
                    for (int i = 0; i < game.quest_count && navail < 32; i++)
                        if (!game.quests[i].completed) avail[navail++] = i;
                }
                if (navail > 0) {
                    int pick = avail[rng() % navail];
                    game.current_quest = pick;
                    Quest *q = &game.quests[pick];
                    spawn_enemies(&game, q->enemy_count, q->enemy_level);
                    game.combat_log.count = 0;
                    game.combat_round = 1;
                    game.selected_target = 0;
                    for (int j = 0; j < game.party_size; j++) {
                        game.party[j].combat_hp = game.party[j].hp;
                        game.party[j].defending = false;
                    }
                    log_combat(&game.combat_log, "--- Round %d ---", game.combat_round);
                    roll_initiative(&game);
                    // Process first turn: run enemy turns until a player turn
                    if (game.init_count > 0) {
                        int first = game.init_order[game.init_current];
                        if (first < 0) {
                            do_enemy_turn(&game, &game.enemies[-(first+1)]);
                            advance_initiative(&game);
                        } else {
                            game.turn_index = first;
                            game.player_turn = true;
                        }
                    }
                    game.screen = SCR_COMBAT;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Character", ImVec2(btn_w, 0))) game.screen = SCR_CHARACTER;

            if (ImGui::Button("Rest (Heal All)", ImVec2(btn_w, 0))) {
                for (int i = 0; i < game.party_size; i++) {
                    game.party[i].hp = game.party[i].max_hp;
                    game.party[i].sp = game.party[i].max_sp;
                    game.party[i].alive = true;
                }
                sfx_play(SFX_HEAL);
            }
            if (ImGui::Button("Quest Board", ImVec2(btn_w, 0))) game.screen = SCR_QUEST_BOARD;
            ImGui::SameLine();
            if (ImGui::Button("Buy Item (50g)", ImVec2(btn_w, 0)) && game.gold >= 50) {
                game.gold -= 50;
                sfx_play(SFX_COIN);
                int who = rng() % game.party_size;
                Character *c = &game.party[who];
                if (c->inv_count < MAX_INVENTORY) {
                    gen_item(&c->inventory[c->inv_count], c->level);
                    c->inv_count++;
                }
            }
            ImGui::End();
        }

        // ── QUEST BOARD ──
        else if (game.screen == SCR_QUEST_BOARD) {
            ImGui::Begin("##quests", nullptr, wf);
            ImGui::Text("QUEST BOARD");
            ImGui::Separator();
            for (int i = 0; i < game.quest_count; i++) {
                Quest *q = &game.quests[i];
                if (q->completed) continue;
                ImGui::PushID(i);
                ImGui::Text("%s", q->name);
                ImGui::SameLine();
                ImGui::TextDisabled("%s", q->desc);
                ImGui::Text("  Enemies: %d  |  XP: %d  |  Gold: %d", q->enemy_count, q->xp_reward, q->gold_reward);
                if (ImGui::Button("Embark!", ImVec2(bw*0.45f, 0))) {
                    game.current_quest = i;
                    spawn_enemies(&game, q->enemy_count, q->enemy_level);
                    game.combat_log.count = 0;
                    game.combat_round = 1;
                    game.selected_target = 0;
                    // init combat HP
                    for (int j = 0; j < game.party_size; j++) {
                        game.party[j].combat_hp = game.party[j].hp;
                        game.party[j].defending = false;
                        game.party[j].initiative = d20() + stat_mod(char_stat(&game.party[j], DEX));
                    }
                    game.turn_index = 0;
                    game.player_turn = true;
                    log_combat(&game.combat_log, "--- Round %d ---", game.combat_round);
                    game.screen = SCR_COMBAT;
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            float qb_btn_h = ImGui::GetFrameHeightWithSpacing() * 2 + 30*dpi_scale;
            ImGui::SetCursorPosY(bh - qb_btn_h);
            ImGui::Separator();
            float qb_w = bw * 0.45f;
            if (ImGui::Button("Random Quest", ImVec2(qb_w, 0))) {
                int avail[32]; int navail = 0;
                for (int i = 0; i < game.quest_count && navail < 32; i++)
                    if (!game.quests[i].completed) avail[navail++] = i;
                if (navail == 0) {
                    gen_quests(&game);
                    for (int i = 0; i < game.quest_count && navail < 32; i++)
                        if (!game.quests[i].completed) avail[navail++] = i;
                }
                if (navail > 0) {
                    int pick = avail[rng() % navail];
                    game.current_quest = pick;
                    Quest *q = &game.quests[pick];
                    spawn_enemies(&game, q->enemy_count, q->enemy_level);
                    game.combat_log.count = 0;
                    game.combat_round = 1;
                    game.selected_target = 0;
                    for (int j = 0; j < game.party_size; j++) {
                        game.party[j].combat_hp = game.party[j].hp;
                        game.party[j].defending = false;
                    }
                    log_combat(&game.combat_log, "--- Round %d ---", game.combat_round);
                    roll_initiative(&game);
                    // Process first turn: run enemy turns until a player turn
                    if (game.init_count > 0) {
                        int first = game.init_order[game.init_current];
                        if (first < 0) {
                            do_enemy_turn(&game, &game.enemies[-(first+1)]);
                            advance_initiative(&game);
                        } else {
                            game.turn_index = first;
                            game.player_turn = true;
                        }
                    }
                    game.screen = SCR_COMBAT;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh", ImVec2(qb_w, 0))) gen_quests(&game);
            if (ImGui::Button("Back", ImVec2(qb_w, 0))) game.screen = SCR_TOWN;
            ImGui::End();
        }

        // ── COMBAT ──
        else if (game.screen == SCR_COMBAT) {
            ImGui::Begin("##combat", nullptr, wf);

            // reserve bottom space for fixed action bar + log
            float action_bar_h = 50*dpi_scale;
            float log_h = 150*dpi_scale;
            float bottom_h = action_bar_h + log_h + 30*dpi_scale;
            float top_h = bh - bottom_h - ImGui::GetCursorPosY();

            ImGui::BeginChild("##combat_top", ImVec2(0, top_h));

            // ── Enemies: 4-column grid ──
            ImGui::SetWindowFontScale(0.8f);
            float cell_w = bw / 4.0f - 2*dpi_scale;
            float io_dt = ImGui::GetIO().DeltaTime;
            for (int i = 0; i < game.enemy_count; i++) {
                Enemy *e = &game.enemies[i];
                if (e->shake_timer > 0) e->shake_timer -= io_dt;
                ImGui::PushID(100+i);
                if (i % 4 != 0) ImGui::SameLine();
                // Apply shake offset
                float shake_x = 0;
                if (e->shake_timer > 0) {
                    float intensity = e->shake_timer / 0.3f; // 1→0
                    shake_x = sinf(e->shake_timer * 60.0f) * 4.0f * dpi_scale * intensity;
                }
                if (shake_x != 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + shake_x);
                ImVec2 cell_start = ImGui::GetCursorScreenPos();
                ImGui::BeginGroup();
                bool sel = (game.selected_target == i);
                if (!e->alive) {
                    ImGui::TextDisabled("[X] %s", e->name);
                } else {
                    if (sel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,0.3f,0.3f,1));
                    if (ImGui::Selectable(e->name, sel, 0, ImVec2(cell_w, 0)))
                        game.selected_target = i;
                    if (sel) ImGui::PopStyleColor();
                    float hp_pct = (float)e->hp / (float)e->max_hp;
                    ImVec4 hp_col = ImVec4(1.0f, hp_pct, hp_pct, 1.0f); // red→white lerp
                    ImGui::TextColored(hp_col, "%d", e->hp);
                }
                ImGui::EndGroup();
                // Alternating cell background
                ImVec2 cell_end = ImVec2(cell_start.x + cell_w + 2*dpi_scale, ImGui::GetItemRectMax().y + 2*dpi_scale);
                ImU32 bg = (i % 2 == 0) ? IM_COL32(40, 40, 55, 100) : IM_COL32(55, 55, 70, 100);
                ImGui::GetWindowDrawList()->AddRectFilled(cell_start, cell_end, bg);
                ImGui::PopID();
            }
            ImGui::SetWindowFontScale(1.0f);

            // ── Combat canvas: blank area for GL rendering ──
            ImGui::Separator();
            float used_y = ImGui::GetCursorPosY();
            // Reserve space for party grid below (1 row of 4)
            float party_grid_h = ImGui::GetFrameHeightWithSpacing() * 3 + 20*dpi_scale;
            float canvas_h = top_h - used_y - party_grid_h;
            if (canvas_h < 40*dpi_scale) canvas_h = 40*dpi_scale;
            ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
            ImGui::Dummy(ImVec2(bw, canvas_h));
            ImVec2 canvas_size = ImVec2(bw, canvas_h);
            ImGui::GetWindowDrawList()->AddRect(
                canvas_pos,
                ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                IM_COL32(60, 60, 80, 120));
            (void)canvas_pos; (void)canvas_size;
            ImGui::Separator();

            // ── Party: 4-column single row ──
            ImGui::SetWindowFontScale(0.8f);
            ImGui::Text("Round %d", game.combat_round);
            float pcell_w = bw / 4.0f - 2*dpi_scale;
            for (int i = 0; i < game.party_size; i++) {
                Character *c = &game.party[i];
                if (c->shake_timer > 0) c->shake_timer -= io_dt;
                ImGui::PushID(200+i);
                if (i % 4 != 0) ImGui::SameLine();
                // Apply shake offset
                float pshake_x = 0;
                if (c->shake_timer > 0) {
                    float intensity = c->shake_timer / 0.3f;
                    pshake_x = sinf(c->shake_timer * 60.0f) * 4.0f * dpi_scale * intensity;
                }
                if (pshake_x != 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pshake_x);
                ImVec2 pcell_start = ImGui::GetCursorScreenPos();
                ImGui::BeginGroup();
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + pcell_w);
                bool is_active = (game.player_turn && game.turn_index == i && c->alive);
                if (is_active) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,0.3f,1));
                else ImGui::PushStyleColor(ImGuiCol_Text, CLASS_COLORS[c->primary_class]);
                ImGui::Text("%s%s", is_active ? ">" : " ", c->name);
                ImGui::PopStyleColor();
                float hp_pct = c->max_hp > 0 ? (float)c->combat_hp / (float)c->max_hp : 0;
                ImVec4 hp_col = ImVec4(1.0f, hp_pct, hp_pct, 1.0f); // red→white lerp
                if (!c->alive) {
                    ImGui::TextDisabled("DEAD");
                } else {
                    ImGui::TextColored(hp_col, "%d", c->combat_hp);
                    if (c->max_sp > 0) {
                        ImGui::SameLine();
                        float sp_pct = (float)c->sp / (float)c->max_sp;
                        ImVec4 sp_col = ImVec4(0.4f + 0.6f*(1-sp_pct), 0.4f + 0.6f*sp_pct, 1.0f, 1.0f);
                        ImGui::TextColored(sp_col, "%d", c->sp);
                    }
                    if (c->defending) { ImGui::SameLine(); ImGui::TextDisabled("DEF"); }
                }
                ImGui::PopTextWrapPos();
                ImGui::EndGroup();
                ImVec2 pcell_end = ImVec2(pcell_start.x + pcell_w + 2*dpi_scale, ImGui::GetItemRectMax().y + 2*dpi_scale);
                ImU32 pbg = (i % 2 == 0) ? IM_COL32(40, 50, 40, 100) : IM_COL32(50, 60, 50, 100);
                ImGui::GetWindowDrawList()->AddRectFilled(pcell_start, pcell_end, pbg);
                ImGui::PopID();
            }
            ImGui::SetWindowFontScale(1.0f);

            ImGui::EndChild();

            // fixed action bar at bottom
            ImGui::Separator();
            auto_target(&game);
            if (game.player_turn && game.turn_index < game.party_size) {
                Character *c = &game.party[game.turn_index];
                ImGui::TextColored(CLASS_COLORS[c->primary_class], "%s's Turn", c->name);
                float abw = bw * 0.30f;
                if (ImGui::Button("Attack", ImVec2(abw, 0))) {
                    do_player_attack(&game, c, game.selected_target);
                    advance_initiative(&game);
                }
                ImGui::SameLine();
                bool has_ability = (c->class_levels[CLASS_WIZARD] > 0 && c->sp >= 5) ||
                                  (c->class_levels[CLASS_CLERIC] > 0 && c->sp >= 4) ||
                                  (c->class_levels[CLASS_BARBARIAN] > 0 && c->sp >= 3) ||
                                  (c->class_levels[CLASS_RANGER] > 0);
                if (!has_ability) ImGui::BeginDisabled();
                if (ImGui::Button("Ability", ImVec2(abw, 0))) {
                    do_ability(&game, c, game.selected_target);
                    advance_initiative(&game);
                }
                if (!has_ability) ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Defend", ImVec2(abw, 0))) {
                    c->defending = true;
                    log_combat(&game.combat_log, "%s defends (+4 AC, half dmg)", c->name);
                    sfx_play(SFX_DEFEND);
                    advance_initiative(&game);
                }
            }

            // check victory/defeat
            if (count_alive_enemies(&game) == 0) {
                Quest *q = &game.quests[game.current_quest];
                q->completed = true;
                game.gold += q->gold_reward;
                int xp_each = q->xp_reward / game.party_size;
                for (int i = 0; i < game.party_size; i++) {
                    game.party[i].xp += xp_each;
                    game.party[i].hp = game.party[i].combat_hp;
                    for (int j = 0; j < game.enemy_count; j++) {
                        if (rng() % 3 == 0 && game.party[i].inv_count < MAX_INVENTORY) {
                            gen_item(&game.party[i].inventory[game.party[i].inv_count], q->enemy_level);
                            game.party[i].inv_count++;
                        }
                    }
                }
                sfx_stop_all();
                game.screen = SCR_VICTORY;
                sfx_play(SFX_VICTORY, 0.6f);
            }
            if (count_alive_party(&game) == 0) {
                sfx_stop_all();
                log_combat(&game.combat_log, "DEFEAT! Your party has fallen.");
                sfx_play(SFX_DEATH, 0.6f);
                for (int i = 0; i < game.party_size; i++) {
                    game.party[i].alive = true;
                    game.party[i].hp = 1;
                    game.party[i].sp = 0;
                }
                game.screen = SCR_TOWN;
            }

            // combat log
            ImGui::BeginChild("##log", ImVec2(0, log_h), ImGuiChildFlags_Borders);
            for (int i = 0; i < game.combat_log.count; i++)
                ImGui::TextWrapped("%s", game.combat_log.lines[i]);
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 10)
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::End();
        }

        // ── VICTORY ──
        else if (game.screen == SCR_VICTORY) {
            ImGui::Begin("##victory", nullptr, wf);
            Quest *q = &game.quests[game.current_quest];
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.85f, 0.2f, 1));
            ImGui::Text("VICTORY!");
            ImGui::PopStyleColor();
            ImGui::Text("Completed: %s", q->name);
            ImGui::Text("XP: +%d   Gold: +%d", q->xp_reward, q->gold_reward);
            ImGui::Spacing();
            for (int i = 0; i < game.party_size; i++) {
                Character *c = &game.party[i];
                ImGui::Text("%s: %d / %d XP %s", c->name, c->xp, c->xp_next,
                           can_level(c) ? "[LEVEL UP AVAILABLE]" : "");
            }
            float vic_btn_h = ImGui::GetFrameHeightWithSpacing() + 30*dpi_scale;
            ImGui::SetCursorPosY(bh - vic_btn_h);
            ImGui::Separator();
            if (ImGui::Button("Continue", ImVec2(bw*0.5f, 0)))
                game.screen = SCR_TOWN;
            ImGui::End();
        }

        // ── LEVEL UP ──
        else if (game.screen == SCR_LEVELUP) {
            Character *c = &game.party[game.levelup_who];
            ImGui::Begin("##levelup", nullptr, wf);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.85f, 0.2f, 1));
            ImGui::Text("LEVEL UP: %s (Lv %d -> %d)", c->name, c->level, c->level+1);
            ImGui::PopStyleColor();

            ImGui::Separator();

            ImGui::Text("Choose class for this level:");
            for (int i = 0; i < MAX_CLASSES; i++) {
                ImGui::PushID(i);
                bool sel = (game.levelup_class_choice == i);
                if (sel) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(CLASS_COLORS[i].x*0.6f, CLASS_COLORS[i].y*0.6f, CLASS_COLORS[i].z*0.6f, 1));
                char lbl[64];
                snprintf(lbl, 64, "%s (%d)", CLASS_NAMES[i], c->class_levels[i]);
                if (ImGui::Button(lbl, ImVec2(bw*0.44f, 0))) game.levelup_class_choice = i;
                if (sel) ImGui::PopStyleColor();
                if (i % 2 != 1) ImGui::SameLine();
                ImGui::PopID();
            }

            if (game.levelup_needs_feat) {
                ImGui::Spacing();
                ImGui::Text("Choose a feat:");
                ImGui::BeginChild("##feats", ImVec2(0, bh*0.35f), ImGuiChildFlags_Borders);
                for (int f = 0; f < NUM_FEATS; f++) {
                    if (c->feats[f]) continue; // already have
                    const FeatDef *fd = &FEATS[f];
                    if (fd->req_level > c->level + 1) continue;
                    if (fd->req_class >= 0 && c->class_levels[fd->req_class] == 0 &&
                        game.levelup_class_choice != fd->req_class) continue;
                    if (fd->req_stat >= 0 && char_stat(c, fd->req_stat) < fd->req_stat_val) continue;
                    ImGui::PushID(300+f);
                    bool sel = (game.levelup_feat_choice == f);
                    if (ImGui::Selectable(fd->name, sel)) game.levelup_feat_choice = f;
                    ImGui::SameLine(bw*0.35f);
                    ImGui::TextDisabled("%s", fd->desc);
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }

            float lu_btn_h = ImGui::GetFrameHeightWithSpacing() + 30*dpi_scale;
            ImGui::SetCursorPosY(bh - lu_btn_h);
            ImGui::Separator();
            bool can_apply = true;
            if (game.levelup_needs_feat && game.levelup_feat_choice < 0) can_apply = false;
            float lu_w = bw * 0.45f;
            if (!can_apply) ImGui::BeginDisabled();
            if (ImGui::Button("Confirm##bot", ImVec2(lu_w, 0))) {
                apply_levelup(c, game.levelup_class_choice,
                             game.levelup_needs_feat ? game.levelup_feat_choice : -1);
                sfx_play(SFX_LEVELUP, 0.6f);
                start_next_levelup(&game, game.levelup_who);
            }
            if (!can_apply) ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(lu_w, 0))) game.screen = SCR_TOWN;
            ImGui::End();
        }

        // ── CHARACTER SHEET ──
        else if (game.screen == SCR_CHARACTER) {
            ImGui::Begin("##char", nullptr, wf);
            ImGui::Text("PARTY");
            ImGui::Separator();

            if (ImGui::BeginTabBar("##partytabs")) {
                for (int pi = 0; pi < game.party_size; pi++) {
                    Character *c = &game.party[pi];
                    if (ImGui::BeginTabItem(c->name)) {
                        ImGui::TextColored(CLASS_COLORS[c->primary_class], "%s %s Lv%d",
                            RACE_NAMES[c->race], CLASS_NAMES[c->primary_class], c->level);
                        if (c->total_past_lives > 0) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImVec4(1,0.85f,0.2f,1), "(%d Past Lives)", c->total_past_lives);
                        }
                        ImGui::Text("XP: %d / %d", c->xp, c->xp_next);
                        draw_hp_bar((float)c->hp, (float)c->max_hp, bw*0.6f, 24*dpi_scale);

                        // class breakdown
                        ImGui::Text("Classes:");
                        for (int cl = 0; cl < MAX_CLASSES; cl++)
                            if (c->class_levels[cl] > 0)
                                ImGui::SameLine(), ImGui::Text("%s %d", CLASS_NAMES[cl], c->class_levels[cl]);

                        // stats
                        ImGui::Spacing();
                        ImGui::Columns(3, "##stats");
                        for (int s = 0; s < NUM_STATS; s++) {
                            int val = char_stat(c, s);
                            ImGui::Text("%s: %d (%+d)", STAT_NAMES[s], val, stat_mod(val));
                            ImGui::NextColumn();
                        }
                        ImGui::Columns(1);

                        ImGui::Text("AC: %d  |  Attack: +%d  |  Damage: %d", char_ac(c), char_attack(c), char_damage(c));

                        // past lives
                        if (c->total_past_lives > 0) {
                            ImGui::Spacing();
                            ImGui::Text("Past Lives:");
                            for (int cl = 0; cl < MAX_CLASSES; cl++)
                                if (c->past_lives[cl] > 0)
                                    ImGui::Text("  %s x%d (+%d %s)", CLASS_NAMES[cl], c->past_lives[cl],
                                        c->past_lives[cl], STAT_NAMES[(int[]){STR,DEX,INT,WIS,DEX,STR}[cl]]);
                        }

                        // feats
                        ImGui::Spacing();
                        ImGui::Text("Feats:");
                        for (int f = 0; f < NUM_FEATS; f++)
                            if (c->feats[f]) ImGui::BulletText("%s", FEATS[f].name);

                        // inventory
                        ImGui::Spacing();
                        ImGui::Text("Inventory (%d):", c->inv_count);
                        for (int ii = 0; ii < c->inv_count; ii++) {
                            Item *item = &c->inventory[ii];
                            ImGui::PushID(400+ii);
                            bool eq = item->equipped;
                            if (eq) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f,0.8f,0.4f,1));
                            ImGui::Text("%s %s", eq ? "[E]" : "   ", item->name);
                            if (eq) ImGui::PopStyleColor();
                            ImGui::SameLine();
                            if (ImGui::Button(eq ? "Unequip" : "Equip")) {
                                if (!eq) {
                                    // unequip same slot first
                                    for (int j = 0; j < c->inv_count; j++)
                                        if (c->inventory[j].equipped && c->inventory[j].slot == item->slot)
                                            c->inventory[j].equipped = false;
                                    item->equipped = true;
                                } else {
                                    item->equipped = false;
                                }
                                calc_hp(c);
                                calc_sp(c);
                            }
                            ImGui::PopID();
                        }

                        // sell unequipped items
                        int unequipped = 0;
                        int sell_value = 0;
                        for (int ii = 0; ii < c->inv_count; ii++) {
                            if (!c->inventory[ii].equipped) {
                                unequipped++;
                                sell_value += c->inventory[ii].bonus * 5 + 5;
                            }
                        }
                        if (unequipped > 0) {
                            char sell_lbl[64];
                            snprintf(sell_lbl, 64, "Sell %d Unequipped (%dg)", unequipped, sell_value);
                            if (ImGui::Button(sell_lbl, ImVec2(bw*0.6f, 0))) {
                                game.gold += sell_value;
                                int write = 0;
                                for (int ii = 0; ii < c->inv_count; ii++) {
                                    if (c->inventory[ii].equipped)
                                        c->inventory[write++] = c->inventory[ii];
                                }
                                c->inv_count = write;
                                sfx_play(SFX_COIN);
                            }
                        }

                        ImGui::EndTabItem();
                    }
                }
                ImGui::EndTabBar();
            }

            float cs_btn_h = ImGui::GetFrameHeightWithSpacing() + 30*dpi_scale;
            ImGui::SetCursorPosY(bh - cs_btn_h);
            ImGui::Separator();
            if (ImGui::Button("Back", ImVec2(bw*0.45f, 0))) game.screen = SCR_TOWN;
            ImGui::End();
        }

        // ── REINCARNATE ──
        else if (game.screen == SCR_REINCARNATE) {
            Character *c = &game.party[game.levelup_who];
            ImGui::Begin("##reinc", nullptr, wf);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.4f, 0.8f, 1));
            ImGui::Text("REINCARNATION");
            ImGui::PopStyleColor();
            ImGui::Separator();
            ImGui::TextWrapped("Reset %s to level 1 with permanent bonuses:", c->name);
            ImGui::BulletText("+1 to a random stat (permanent)");
            ImGui::BulletText("+5 HP per past life (permanent)");
            ImGui::BulletText("+1 %s (from %s past life)",
                STAT_NAMES[(int[]){STR,DEX,INT,WIS,DEX,STR}[c->primary_class]], CLASS_NAMES[c->primary_class]);
            ImGui::Spacing();
            ImGui::Text("Current past lives: %d / %d max per class", c->total_past_lives, MAX_PAST_LIVES * MAX_CLASSES);
            for (int cl = 0; cl < MAX_CLASSES; cl++)
                if (c->past_lives[cl] > 0)
                    ImGui::Text("  %s: %d/%d", CLASS_NAMES[cl], c->past_lives[cl], MAX_PAST_LIVES);
            float rc_btn_h = ImGui::GetFrameHeightWithSpacing() + 30*dpi_scale;
            ImGui::SetCursorPosY(bh - rc_btn_h);
            ImGui::Separator();
            float rc_w = bw * 0.45f;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f,0.2f,0.7f,1));
            if (ImGui::Button("Reincarnate!", ImVec2(rc_w, 0))) {
                reincarnate(c);
                sfx_play(SFX_REINCARNATE, 0.5f);
                game.screen = SCR_TOWN;
            }
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(rc_w, 0))) game.screen = SCR_TOWN;
            ImGui::End();
        }

        sfx_update();
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
