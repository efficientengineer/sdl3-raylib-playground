// audio.cpp — every sound the game makes: the FM voices and the adaptive music sequencer.
//
//   owns      the twenty-voice mixer, the two-operator FM voices that give it the Genesis flavour,
//             the six moods' chords and tempi (MOODS), the pattern writer (mus_step), the stinger
//             and tempo glide on a mood change, and the SDL audio stream it is all queued to.
//   never     decides WHEN to change mood: a scene line's {mood} tag does that through mus_start,
//             and the bar count never resets, so a change is a turn rather than a new track.
//   exposes   au_play/au_init/au_update and mus_start/mus_fade_out/mus_stinger/mus_sample.
//   tested by ear, and by --robustness D (the settings round-trip drives the gain path).
//
// Audio is generated on the main thread and queued to an SDL stream, so no locking is needed.
#include "star_internal.h"

Voice au_voices[AU_VOICES];
SDL_AudioStream *au_stream;

float au_noise() {
    static uint32_t n = 48271;
    n = n * 16807u % 2147483647u;
    return (float)n / 1073741823.5f - 1.0f;
}

float note_freq(int midi) { return 440.0f * powf(2.0f, (midi - 69) / 12.0f); }

void au_play(int type, float freq, float dur, float vol) {
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
float fm(float t, float freq, float ratio, float index) {
    return sinf(TAU * freq * t + index * sinf(TAU * freq * ratio * t));
}

float voice_sample(Voice *v) {
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

extern const MoodDef MOODS[CS_MOOD_COUNT] = {
    /* wonder   */ { 72,  {{53,57,64}, {55,59,62}, {57,60,64}, {52,55,59}} },   // F  G  Am Em
    /* dread    */ { 56,  {{45,52,57}, {45,52,58}, {44,51,56}, {45,52,57}} },   // Am  Am(b9)  G#  Am
    /* tense    */ { 96,  {{45,52,60}, {45,52,60}, {41,48,57}, {43,50,59}} },   // Am Am F  G
    /* confront */ { 132, {{45,52,57}, {41,48,53}, {43,50,55}, {40,47,52}} },   // Am F  G  E
    /* sorrow   */ { 60,  {{57,60,64}, {53,57,60}, {48,52,55}, {52,56,59}} },   // Am F  C  E
    /* hope     */ { 84,  {{48,52,55}, {55,59,62}, {57,60,64}, {53,57,60}} },   // C  G  Am F
};

// The one sequencer (MusState is in star_internal.h: dev_panel.cpp reads it, game.cpp clears it).
MusState mus;
void mus_start(int mood) {
    if (!mus.on) { mus.on = true; mus.mood = mood; mus.bpm = MOODS[mood].bpm; mus.step = -1; mus.step_t = 1e9f; mus.bar = 0; mus.volume = 0; }
    mus.target = mood;
    mus.target_volume = 1.0f;
}
void mus_fade_out() { mus.target_volume = 0.0f; }

void mus_stinger(int mood) {
    switch (mood) {
    case CS_CONFRONT: au_play(V_CRASH, 0, 1.6f, 0.30f); au_play(V_SUB, 82, 1.2f, 0.55f); au_play(V_BASS, note_freq(33), 1.0f, 0.5f); break;
    case CS_DREAD:    au_play(V_SUB, 70, 2.4f, 0.55f); au_play(V_BELL, note_freq(82), 2.5f, 0.10f); break;
    case CS_SORROW:   au_play(V_BELL, note_freq(88), 3.0f, 0.22f); au_play(V_BELL, note_freq(76), 3.0f, 0.14f); break;
    case CS_HOPE:     au_play(V_BELL, note_freq(84), 2.0f, 0.2f); au_play(V_BELL, note_freq(79), 2.0f, 0.14f); break;
    case CS_WONDER:   au_play(V_BELL, note_freq(81), 3.0f, 0.16f); break;
    case CS_TENSE:    au_play(V_HAT, 0, 0.2f, 0.4f); au_play(V_BASS, note_freq(33), 0.8f, 0.4f); break;
    }
}

void mus_step(int mood, int s, int bar) {
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

float mus_sample(float dt) {
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
void au_init() {
    memset(au_voices, 0, sizeof(au_voices));
    SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, AU_RATE };
    au_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (au_stream) SDL_ResumeAudioStreamDevice(au_stream);
}

void au_update() {
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
