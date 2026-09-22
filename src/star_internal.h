// star_internal.h — what the game's translation units share. The game is star_logic.cpp's
// libgame_logic.so, split the way the screens are split.
//
//   owns      the includes every game file needs, the Star state struct, the Screen enum, the
//             cutscene/portrait constants, and the cross-file declarations of each module.
//   never     declares anything host.cpp sees — that is game_api.h — and holds no definitions but
//             the type and constant ones, so any file may include it.
//   exposed   to star_logic.cpp, game.cpp, cutscene.cpp, chapter.cpp, dev_panel.cpp, settings.cpp,
//             audio.cpp, star_test.cpp and playtest.cpp. Nothing outside src/ includes it.
//
// The modules, and what each one owns:
//   star_logic.cpp  the game_api entry point and the single stb_image/stb_image_write TU
//   game.cpp        game_create/tick/serialize, the screen switch, the fades, title and end card
//   cutscene.cpp    the panel and talk player: textures, portraits, expressions, the typewriter
//   chapter.cpp     the chapter's runtime in the field: the world screen, the goal line, its test
//   dev_panel.cpp   the phone<->dev message log and the Dev window
//   settings.cpp    the player's settings file and its debounce
//   audio.cpp       the FM voices and the adaptive music sequencer
//   star_test.cpp   --clips-selftest, --battle-ui-test and --robustness
//   playtest.cpp    --chapter-playtest: the bot that plays the chapter through the real game
#pragma once

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

#include "stb_image.h"           // the IMPLEMENTATION lives in star_logic.cpp, once
#include "stb_image_write.h"     // likewise: the capture path's PNG writer
#include "game_api.h"
#include "cutscene_data.h"
#include "voxfield.h"
#include "dialogue.h"
#include "battle.h"
#include "chapter01.h"
#include "field_text.h"   // the robustness sweep measures every one of these in the box
// The game plays CHAPTERS now — `## intro` is gone from story/playlist.md and CS_INTRO with it.
// New Game starts at CS_CHAPTERS[0]. The cutscene player reads one chapter's scene list at a time;
// CS_CUR is that list, so the player itself did not have to learn what a chapter is.
#ifndef CS_HAS_CHAPTERS
#error "src/cutscene_data.h is stale: run ./story_prompt.py export (it must define CS_HAS_CHAPTERS)"
#endif
extern const CsScene *CS_CUR;
extern const int CS_CUR_COUNT;

#define PREF_ORG "com.playground"
#define PREF_APP "questglory"   // host.cpp uses the same pref path for reload.flag and the .so

// ───────────────────────── audio.cpp ─────────────────────────
#define AU_RATE 44100
#define AU_VOICES 20
#define TAU 6.2831853f

enum VoiceType { V_OFF = 0, V_BASS, V_BELL, V_LEAD, V_PAD, V_KICK, V_SNARE, V_HAT, V_CRASH, V_SUB, V_BLIP, V_THUMP };

struct Voice { int type; float freq, t, dur, vol; };
extern Voice au_voices[AU_VOICES];
extern SDL_AudioStream *au_stream;
float note_freq(int midi);
void au_play(int type, float freq, float dur, float vol);
void au_init();
void au_update();

struct MoodDef { float bpm; int chords[4][3]; };      // chord tones as MIDI notes, one chord per bar
extern const MoodDef MOODS[CS_MOOD_COUNT];

// The one sequencer. `mood` is what is playing, `target` what the next beat will switch to; the
// bar count never resets, which is what keeps a mood change from sounding like a new track.
struct MusState {
    bool on;
    int mood, target;
    float bpm, step_t, volume, target_volume;
    int step, bar;                                     // 16th-note step within the bar
};
extern MusState mus;
void mus_start(int mood);
void mus_fade_out();
void mus_stinger(int mood);
float mus_sample(float dt);

// ───────────────────────── settings.cpp ─────────────────────────
struct StarSettings {
    float volume;          // 0..1
    bool muted;
    bool env_forced;       // STAR_MUTE was set at startup
    bool dirty;            // a change is waiting to be written
    float save_t;          // debounce
    float gain;            // the ramped gain the mixer actually applies
    bool open;             // the window
};
extern StarSettings g_set;
const char *settings_path();
void settings_save(void);
void settings_load(void);
void settings_tick(float dt);

// ───────────────────────── cutscene.cpp: files and textures ─────────────────────────
const char *pref_path();
struct Tex { GLuint id; int w, h; };

// Panels, speaker portraits, and talk backdrops all load from <pref>/cutscenes/ first
// (pushed by fast_reload.sh), then from the APK assets.
Tex tex_load(const char *file);

// ───────────────────────── dev_panel.cpp ─────────────────────────
#define MAX_MSGS 25
#define MSG_LEN 512
struct DevMsg { char text[MSG_LEN]; char stamp[12]; bool from_dev; };
struct DevChat { DevMsg msgs[MAX_MSGS]; int count; char input[MSG_LEN]; bool open; float poll_t; bool scroll; };
extern DevChat dev;
const char *dev_log_path();
void dev_load();
void dev_send(const char *text);

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

#ifndef CS_NARRATOR
#define CS_NARRATOR "Narrator"    // older cutscene_data.h; ./story_prompt.py export writes this itself
#endif

// ───────────────────────── cutscene.cpp: the player ─────────────────────────
void star_free_textures(Star *st);
bool line_is_narrator(const CsLine *ln);
extern char g_expr_force[16];
const char *line_expr(const CsLine *ln);
void load_faces(Star *st, const CsScene *sc);
void reveal_panel(Star *st, const CsScene *sc, int n, bool instant);
void star_goto(Star *st, int scene, int line, bool instant);
void star_advance(Star *st);
float ease_out(float x);
ImU32 with_alpha(ImU32 c, float a);
void draw_typed(ImDrawList *dl, ImFont *font, float size, ImVec2 pos, float width, ImU32 col,
                const char *text, int chars, bool center);
void draw_box(ImDrawList *dl, ImVec2 a, ImVec2 b, float u);
void draw_scene(Star *st, int w, int h, float dt);

// ───────────────────────── game.cpp ─────────────────────────
void draw_title(Star *st, int w, int h, float dt);
void draw_end(Star *st, int w, int h, float dt);
void *game_create(float dpi_scale);
void game_destroy(void *state);
void game_tick(void *state, int w, int h, float dpi_scale);
void game_on_save_event(void *);
int game_wants_quit(void *state);
size_t game_serialize(void *state, void *buf, size_t buf_size);
void game_deserialize(void *state, const void *buf, size_t size);

// Hot reload keeps your place: screen, scene and line survive, so a dialogue or music edit can be
// judged on the exact line you were looking at, and the field keeps the map, the spot on the navmesh
// and whatever camera the owner was tuning. Everything else is rebuilt from those few numbers.
struct ReloadBlob {
    uint32_t magic; int32_t screen, scene, line, line_page, to_field;
    int32_t has_vx; VxSave vx;
    Chapter ch;                    // the story's memory: step, flags, party, the bell
};
#define RELOAD_MAGIC 0x44525456u   // bumped: the old 3D field and the tile field left ReloadBlob

// ───────────────────────── chapter.cpp ─────────────────────────
int scene_by_id(const char *id);
void enter_field(Star *st);
void field_scene(Star *st, const char *id);
void draw_field(Star *st, int w, int h, float dt);
void draw_goal(Star *st, int w, int h);
const char *ch_text_for_flag(int flag);
const char *ch_item_for_flag(int flag);
int chapter_selftest();

// ───────────────────────── dev_panel.cpp: the window ─────────────────────────
void dev_step(Star *st, int step);
void draw_dev(Star *st, int w, int h, float dt, float dpi);

// ───────────────────────── star_test.cpp ─────────────────────────
void ct_begin(Star *st, int scene);
int ct_drive(Star *st, int w, int h);
void bui_tap(float x, float y);                  // the ONE way a test presses a button
void bui_pump();
bool bui_start(Star *st, const char *enc, int party_n);
void battle_ui_test(Star *st, int w, int h, float dt);
int rb_drive(Star *st, int w, int h, float dt);

// ───────────────────────── playtest.cpp ─────────────────────────
void pb_begin(Star *st, int lazy);
int pb_drive();
int playtest_hidden_finds(Star *st);
int playtest_bell_run(Star *st);
int playtest_hill_climb(Star *st);
void pb_set_size(int w, int h);                  // the REAL window size the bot taps in
int pb_fail_count();                             // failures found by the run just finished
